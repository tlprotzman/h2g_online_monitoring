/*
LFHCal Test Beam Online Monitoring
Tristan Protzman, 27-08-2024
*/

#include "Monitor.h"

#include "file_stream.h"
#include "line_stream.h"
#include "channel_stream.h"
#include "configuration.h"
#include "canvas_manager.h"
#include "server.h"
#include "event_builder.h"

#include <TROOT.h>
#include <TH1.h>
#include <TH2.h>
#include <TH2D.h>
#include <TH3.h>
#include <TH3D.h>
#include <TRint.h>
#include <TApplication.h>
#include <TSystem.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TFile.h>
#include <THttpServer.h>

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <csignal>

int channel_map[72] = {64, 63, 66, 65, 69, 70, 67, 68,
                       55, 56, 57, 58, 62, 61, 60, 59,
                       45, 46, 47, 48, 52, 51, 50, 49,
                       37, 36, 39, 38, 42, 43, 40, 41,
                       34, 33, 32, 31, 27, 28, 29, 30,
                       25, 26, 23, 24, 20, 19, 22, 21,
                       16, 14, 15, 12,  9, 11, 10, 13,
                        7,  6,  5,  4,  0,  1,  2,  3,
                        -1, -1, -1, -1, -1, -1, -1, -1};

int get_channel_x(int channel) {
    return channel % 4;
}

int get_channel_y(int channel) {
    return (channel % 8) > 3 ? 1 : 0;
}

int get_channel_z(int channel) {
    return channel / 8;
} 

// catch ctrl-c
bool stop = false;
void signal_handler(int signal) {
    stop = true;
}

int decode_fpga(int fpga_id) {
    return fpga_id;
    // if (fpga_id == 0) {
    //     return 0;
    // }
    // return -1;
}

int encode_fpga(int fpga_id) {
    if (fpga_id == 0) {
        return 0;
    }
    return -1;
}

int decode_asic(int asic_id) {
    if (asic_id == 160) {
        return 0;
    }
    else if (asic_id == 161) {
        return 1;
    }
    return -1;
}

int encode_asic(int asic_id) {
    if (asic_id == 0) {
        return 160;
    }
    else if (asic_id == 1) {
        return 161;
    }
    return -1;
}

int decode_half(int half_id) {
    if (half_id == 36) {
        return 0;
    }
    else if (half_id == 37) {
        return 1;
    }
    return -1;
}

int encode_half(int half_id) {
    if (half_id == 0) {
        return 36;
    }
    else if (half_id == 1) {
        return 37;
    }
    return -1;
}

uint32_t bit_converter(uint8_t *buffer, int start, bool big_endian = true) {
    if (big_endian) {
        return (buffer[start] << 24) + (buffer[start + 1] << 16) + (buffer[start + 2] << 8) + buffer[start + 3];
    }
    return (buffer[start + 3] << 24) + (buffer[start + 2] << 16) + (buffer[start + 1] << 8) + buffer[start];
}

void decode_line(line &p, uint8_t *buffer) {
    p.asic_id = decode_asic(buffer[0]);
    p.fpga_id = decode_fpga(buffer[1]);
    p.half_id = decode_half(buffer[2]);
    p.line_number = buffer[3];
    p.timestamp = bit_converter(buffer, 4);
    for (int i = 0; i < 8; i++) {
        p.package[i] = bit_converter(buffer, i * 4 + 8);
    }
}

int decode_packet(std::vector<line> &lines, uint8_t *buffer) {
    int decode_ptr = 12; // the header is the first 12 bytes
    for (int i = 0; i < 36; i++) {
        decode_line(lines[i], buffer + decode_ptr);
        decode_ptr += 40;
    }
    return 0;
}

void process_lines(std::vector<line> &lines, line_stream_vector &streams, TH1 *data_rates) {
    for (auto line : lines) {
        // std::cout << line.fpga_id << " " << line.asic_id << " " << line.half_id << " " << line.line_number << " " << line.timestamp << std::endl;
        if (line.fpga_id == -1 || line.asic_id == -1 || line.half_id == -1) {
            continue;
        }
        data_rates->Fill(4 * line.fpga_id + 2 * line.asic_id + line.half_id);
        streams[line.fpga_id][line.asic_id][line.half_id]->add_line(line);
    }
}

class online_monitor {
private:
    int run_number;
    int timestamp; 
    TFile *output;
    canvas_manager canvases;

    std::vector<TH2*> adc_per_channel;
    std::vector<TH2*> tot_per_channel;
    std::vector<TH2*> toa_per_channel;

    uint32_t event_drawn;
    TH3 *event_display;

    event_builder **builders;
    event_thunderdome *thunderdome;


public:
    channel_stream_vector channels;
    line_stream_vector line_streams;
    online_monitor(int run_number);
    ~online_monitor();

    // TRint *app;
    void update_canvases() {
        canvases.update();
        gSystem->ProcessEvents();
    }
    void update_builder_graphs();
    void update_events();
    void build_events();
    void make_event_display();
};

online_monitor::online_monitor(int run_number) {
    gSystem->mkdir("monitoring_plots", kTRUE);
    gSystem->mkdir(Form("monitoring_plots/run_%03d", run_number), kTRUE);
    auto time = std::chrono::system_clock::now();
    timestamp = std::chrono::system_clock::to_time_t(time);
    output = new TFile(Form("monitoring_plots/run_%03d/run_%03d_monitoring_%d.root", run_number, run_number, timestamp), "RECREATE");

    this->run_number = run_number;
    auto s = server::get_instance()->get_server();
    canvases = canvas_manager::get_instance();
    auto config = configuration::get_instance();
    for (int i = 0; i < config->NUM_FPGA; i++) {
        adc_per_channel.push_back(new TH2D(Form("adc_per_channel_%d", i), Form("Run %03d ADC per Channel FPGA %d", run_number, i), 144, 0, 144, 1024, 0, config->MAX_ADC));
        tot_per_channel.push_back(new TH2D(Form("tot_per_channel_%d", i), Form("Run %03d TOT per Channel FPGA %d", run_number, i), 144, 0, 144, 1024, 0, config->MAX_TOT));
        toa_per_channel.push_back(new TH2D(Form("toa_per_channel_%d", i), Form("Run %03d TOA per Channel FPGA %d", run_number, i), 144, 0, 144, 1024, 0, config->MAX_TOA));
        s->Register("/qa_plots/graphs/adc", adc_per_channel.back());
        s->Register("/qa_plots/graphs/tot", tot_per_channel.back());
        s->Register("/qa_plots/graphs/toa", toa_per_channel.back());
        int c = canvases.new_canvas(Form("FPGA_%i", i), Form("FPGA %i", i), 1200, 800);
        auto canvas = canvases.get_canvas(c);
        s->Register("/qa_plots/canvases", canvas);

        canvas->Divide(1, 3, 0, 0);
        canvas->cd(1);
        adc_per_channel[i]->Draw("colz");
        gPad->SetLogz();
        canvas->cd(2);
        tot_per_channel[i]->Draw("colz");
        gPad->SetLogz();
        canvas->cd(3);
        toa_per_channel[i]->Draw("colz");
        gPad->SetLogz();
    }



    for (int fpga = 0; fpga < config->NUM_FPGA; fpga++) {
        line_streams.push_back(std::vector<std::vector<line_stream*>>());
        channels.push_back(std::vector<std::vector<channel_stream*>>());
        for (int asic = 0; asic < config->NUM_ASIC; asic++) {
            line_streams[fpga].push_back(std::vector<line_stream*>());
            channels[fpga].push_back(std::vector<channel_stream*>());
            for (int half = 0; half < 2; half++) {
                auto l = new line_stream();
                // std::cout << "address of l: " << l << std::endl;
                line_streams[fpga][asic].push_back(l);
            }
            for (int channel = 0; channel < 72; channel++) {
                // std::cout << "asic here: " << asic << std::endl;
                auto c = new channel_stream(fpga, asic, channel, adc_per_channel[fpga], tot_per_channel[fpga], toa_per_channel[fpga]);
                // std::cout << "address of c: " << c << std::endl;
                channels[fpga][asic].push_back(c);
            }
        }
    }

    // std::cerr << "point a: " << channels[0][0][0]->test << std::endl;

    // The line streams need to be able to fill the channel streams
    for (auto fpga : line_streams) {
        for (auto asic : fpga) {
            for (auto half : asic) {
                // std::cout << "address of half: " << &half << std::endl;
                half->associate_channels(channels);
            }
        }
    }

    // Set up event builders 
    builders = new event_builder*[config->NUM_FPGA];
    for (int i = 0; i < config->NUM_FPGA; i++) {
        builders[i] = new event_builder(decode_fpga(i));
    }
    thunderdome = new event_thunderdome(builders);

    auto text = new TLatex();
    text->SetTextSize(0.12);
    text->SetTextFont(42);
    

    // Set up canvases
    uint32_t adc_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t waveform_canvas[config->NUM_FPGA * config->NUM_ASIC];
    for (int i = 0; i < config->NUM_FPGA; i++) {
        for (int j = 0; j < config->NUM_ASIC; j++) {
            adc_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("adc_fpga_%d_asic_%d", i, j), Form("ADC Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            auto c = canvases.get_canvas(adc_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/canvases", c);
            c->Divide(9, 8, 0, 0);
            waveform_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(waveform_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/canvases", c);
            c->Divide(9, 8, 0, 0);
        }
    }
    for (int fpga = 0; fpga < config->NUM_FPGA; fpga++) {
        for (int asic = 0; asic < config->NUM_ASIC; asic++) {
            for (int channel = 0; channel < 72; channel++) {
                auto c = canvases.get_canvas(adc_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel]->draw_adc();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel));
                gPad->SetLogy();
                c = canvases.get_canvas(waveform_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel]->draw_waveform();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel));
                gPad->SetLogz();
            }
        }
    }



    uint32_t ordered_adc_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t ordered_waveform_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t adc_max_canvas[config->NUM_FPGA * config->NUM_ASIC];
    for (int i = 0; i < config->NUM_FPGA; i++) {
        for (int j = 0; j < config->NUM_ASIC; j++) {
            adc_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_adc_fpga_%d_asic_%d", i, j), Form("ADC Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            auto c = canvases.get_canvas(adc_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/spatial", c);
            c->Divide(8, 8, 0, 0);
            waveform_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(waveform_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/spatial", c);
            c->Divide(8, 8, 0, 0);
            adc_max_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("adc_max_fpga_%d_asic_%d", i, j), Form("ADC Max FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(adc_max_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/spatial", c);
            c->Divide(8, 8, 0, 0);
        }
    }
    for (int fpga = 0; fpga < config->NUM_FPGA; fpga++) {
        for (int asic = 0; asic < config->NUM_ASIC; asic++) {
            for (int channel = 0; channel < 64; channel++) {
                auto c = canvases.get_canvas(adc_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel_map[channel]]->draw_adc();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_map[channel]));
                gPad->SetLogy();
                
                c = canvases.get_canvas(waveform_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel_map[channel]]->draw_waveform();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_map[channel]));
                gPad->SetLogz();

                c = canvases.get_canvas(adc_max_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel_map[channel]]->draw_max();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_map[channel]));
                text->DrawLatexNDC(0.95, 0.43, Form("max(samples) - samples[0]"));
                gPad->SetLogy();

            }
        }
    }


    // Set up event display
    event_drawn = 0;
    auto c = canvases.new_canvas("event_display", Form("Run %03d Event Display", run_number), 1200, 800);
    auto canvas = canvases.get_canvas(c);
    event_display = new TH3D("event_display", Form("Run %03d Event Display", run_number), 64, 0, 64, 4, 0, 4, 2, 0, 2);
    event_display->Draw("BOX2");
    s->Register("/event_display", canvas);
}

online_monitor::~online_monitor() {
    canvases.save_all(run_number, timestamp);
    for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
        builders[i]->update_stats();
    }
    output->Write();
    std::cout << "Writing root file..." << std::endl;
    output->Close();

}

void online_monitor::update_events() {
    for (auto fpga_id : channels) {
        for (auto asic_id : fpga_id) {
            for (auto channel : asic_id) {
                while (channel->has_events()) {
                    auto event = channel->get_event();
                    builders[event->get_fpga_id()]->channel_hit(event);
                }
            }
        }
    }
}

void online_monitor::update_builder_graphs() {
    for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
        builders[i]->update_stats();
    }
}

void online_monitor::build_events() {
    thunderdome->align_events();
    std::cout << "Built " << thunderdome->get_num_events() << " events" << std::endl;
}

void online_monitor::make_event_display() {
    // return;
    // Clear existing event display
    // event_display->Clear();
    // Make sure we actually have completed events...
    if (thunderdome->get_num_events() == 0) {
        std::cout << "No events to display" << std::endl;
        return;
    }
    event_display->Reset();
    auto last_event = thunderdome->get_event(event_drawn);
    std::cout << "\nDrawing event " << event_drawn << std::endl;
    event_drawn++;
    // std::cerr << "last event: " << thunderdome->get_num_events() - 1 << std::endl;
    uint32_t fpga_factors[4] = {1, 3, 0, 2};
    for (auto event : last_event) {
        auto fpga = event.get_fpga_id();
        // std::cerr << "trying fpga " << fpga << std::endl;
        auto num_channels = configuration::get_instance()->NUM_CHANNELS * configuration::get_instance()->NUM_ASIC;
        for (int channel = 0; channel < num_channels; channel++) {
            // std::cerr << "trying channel " << channel << std::endl;
            auto c = event.get_channel(channel);
            if (c != nullptr) {
                if (!c->is_complete()) {
                    std::cerr << "Incomplete channel event" << std::endl;
                    continue;
                }
                auto channel_id = channel_map[channel % configuration::get_instance()->NUM_CHANNELS];
                if (channel_id < 0) {   // We record more channels than we use
                    continue;
                }
                auto asic = channel >= configuration::get_instance()->NUM_CHANNELS ? 1 : 0;
                auto x = get_channel_x(channel
                );
                auto y = get_channel_y(channel);
                auto z = fpga_factors[fpga] * 16 + 8 * asic + get_channel_z(channel % configuration::get_instance()->NUM_CHANNELS);
                // z = get_channel_z(channel % configuration::get_instance()->NUM_CHANNELS);

                // std::cout << "FPGA" << fpga << "Channel" << channel << "Filling " << z << " " << x << " " << y << " " << c->get_max_sample() << std::endl;
                event_display->Fill(z, x, y, c->get_max_sample());
                // event_display->Fill(z, x, y, 1);
            }
        }
    }
}

void test_decoding() {
    auto config = configuration::get_instance();
    uint8_t line0[config->PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x00, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line1[config->PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x01, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line2[config->PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x02, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line3[config->PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x03, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line4[config->PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x04, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};

    line p0;
    line p1;
    line p2;
    line p3;
    line p4;

    decode_line(p0, line0);
    decode_line(p1, line1);
    decode_line(p2, line2);
    decode_line(p3, line3);
    decode_line(p4, line4);

    std::cout << "asic id: " << p0.asic_id << std::endl;
    std::cout << "fpga id: " << p0.fpga_id << std::endl;
    std::cout << "half id: " << p0.half_id << std::endl;
    std::cout << "line number: " << p0.line_number << std::endl;
    std::cout << "timestamp: " << p0.timestamp << std::endl;
}

void test_reading(int run) {
    // auto server = canvases->get_server();
    auto s = server::get_instance()->get_server();
    auto start_time = std::chrono::high_resolution_clock::now();
    auto m = new online_monitor(run);
    auto line_numbers = new TH1I("line_numbers", "Line Numbers", 5, 0, 5);
    // const char *fname = "run051.txt";

    // Some histograms to check the data rates from each board/asic
    auto data_rates = new TH1D("lines_received", "Lines Received;;#", 16, 0, 16);
    const char *readout_label[16]  = {"F0A0H0","F0A0H1","F0A1H0","F0A1H1",
                                      "F1A0H0","F1A0H1","F1A1H0","F1A1H1",
                                      "F2A0H0","F2A0H1","F2A1H0","F2A1H1",
                                      "F3A0H0","F3A0H1","F3A1H0","F3A1H1"};
    for (int i = 0; i < 16; i++) {
        data_rates->GetXaxis()->SetBinLabel(i + 1, readout_label[i]);
    }
    // data_rates->SetMinimum(0);
    s->Register("/qa_plots/daq/", data_rates);

    auto dir = getenv("DATA_DIRECTORY");
    if (dir == nullptr) {
        std::cerr << "DATA_DIRECTORY not set!  Defaulting to current directory." << std::endl;
        dir = new char[2];
        strcpy(dir, ".");
        return;
    }

    const char *fname = Form("%s/Run%03d.h2g", dir, run);
    // const char *fname = "PhaseScanChannel16Phase2.txt";
    file_stream fs(fname);
    uint8_t buffer[configuration::get_instance()->PACKET_SIZE];
    uint32_t heartbeat_seconds = 0;
    uint32_t heartbeat_milliseconds = 0;
    // for (int iteration = 0; iteration < 50; iteration++) {
    // for (int iteration = 0; iteration < 61100; iteration++) {
    bool all_events_built = false;
    for (int iteration = 0; iteration < 10000000; iteration++) {
        if (stop) {
            break;
        }
        s->ProcessRequests();
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time).count() > 4) {
            std::cout << "Building events...";
            m->build_events();
            std::cout << " done!" << std::endl;
            std::cout << "Updating canvases...";
            fs.print_packet_numbers();
            m->update_builder_graphs();
            m->update_canvases();
            gSystem->ProcessEvents();
            start_time = std::chrono::high_resolution_clock::now();
            // m->make_event_display();
            std::cout << " done!" << std::endl;
            all_events_built = true;
        }
        int good_data = fs.read_packet(buffer);
        if (!good_data) {
            if (all_events_built) {
                std::cout << "All events built, exiting..." << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        all_events_built = false;
        if (good_data == 2) {
            // std::cout << "Heartbeat packet" << std::endl;
            heartbeat_seconds = bit_converter(buffer, 12, false);
            heartbeat_milliseconds = bit_converter(buffer, 16, false);
            // std::cout << "Heartbeat: " << heartbeat_seconds << "." << heartbeat_milliseconds << std::endl;
            continue;
        }
        std::vector<line> lines(36);    // 36 lines per packet
        decode_packet(lines, buffer);
        process_lines(lines, m->line_streams, data_rates);
        for (auto line : lines) {
            line_numbers->Fill(line.line_number);
        }
        m->update_events();

        // for (int i = 0; i < PACKET_SIZE; i++) {
        //     std::cout << setfill('0') << setw(2) << std::hex << (int)buffer[i] << " ";
        // }
        // std::cout << std::endl << std::endl << std::endl;;
    }
    delete m;
    std::cout << "Exiting cleanly, just for you Fredi!" << std::endl;
}

int Monitor(int run, std::string config_file) {
    // register signal handler
    signal(SIGINT, signal_handler);

    gStyle->SetOptStat(0);
    // 
    // test_decoding();
    load_configs(config_file);
    print_configs();
    test_reading(run);
    return 0;
}

int main() {
    TApplication app("app", 0, nullptr);
    Monitor(42, "");  // answer to the universe
    return 0;
}
