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

#include <TROOT.h>
#include <TH1.h>
#include <TH2.h>
#include <TH2D.h>
#include <TRint.h>
#include <TApplication.h>
#include <TSystem.h>
#include <TStyle.h>
#include <THttpServer.h>

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <chrono>
#include <thread>


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

uint32_t bit_converter(uint8_t *buffer, int start) {
    return (buffer[start] << 24) + (buffer[start + 1] << 16) + (buffer[start + 2] << 8) + buffer[start + 3];
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
    canvas_manager canvases;

    std::vector<TH2*> adc_per_channel;
    std::vector<TH2*> tot_per_channel;
    std::vector<TH2*> toa_per_channel;

public:
    channel_stream_vector channels;
    line_stream_vector line_streams;
    online_monitor();
    ~online_monitor();

    // TRint *app;
    void update_canvases() {
        canvases.update();
        gSystem->ProcessEvents();
    }
    void update();
};

online_monitor::online_monitor() {
    auto s = server::get_instance()->get_server();
    canvases = canvas_manager::get_instance();
    for (int i = 0; i < NUM_FPGA; i++) {
        adc_per_channel.push_back(new TH2D(Form("adc_per_channel_%d", i), Form("ADC per Channel FPGA %d", i), 144, 0, 144, 1024, 0, MAX_ADC));
        tot_per_channel.push_back(new TH2D(Form("tot_per_channel_%d", i), Form("TOT per Channel FPGA %d", i), 144, 0, 144, 1024, 0, MAX_TOT));
        toa_per_channel.push_back(new TH2D(Form("toa_per_channel_%d", i), Form("TOA per Channel FPGA %d", i), 144, 0, 144, 1024, 0, MAX_TOA));
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

    for (int fpga = 0; fpga < NUM_FPGA; fpga++) {
        line_streams.push_back(std::vector<std::vector<line_stream*>>());
        channels.push_back(std::vector<std::vector<channel_stream*>>());
        for (int asic = 0; asic < NUM_ASIC; asic++) {
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


    // Set up canvases
    uint32_t adc_canvas[NUM_FPGA * NUM_ASIC];
    uint32_t waveform_canvas[NUM_FPGA * NUM_ASIC];
    for (int i = 0; i < NUM_FPGA; i++) {
        for (int j = 0; j < NUM_ASIC; j++) {
            adc_canvas[i * NUM_ASIC + j] = canvases.new_canvas(Form("adc_fpga_%d_asic_%d", i, j), Form("ADC Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            auto c = canvases.get_canvas(adc_canvas[i * NUM_ASIC + j]);
            s->Register("/qa_plots/canvases", c);
            c->Divide(9, 8, 0, 0);
            waveform_canvas[i * NUM_ASIC + j] = canvases.new_canvas(Form("waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(waveform_canvas[i * NUM_ASIC + j]);
            s->Register("/qa_plots/canvases", c);
            c->Divide(9, 8, 0, 0);
        }
    }
    for (int fpga = 0; fpga < NUM_FPGA; fpga++) {
        for (int asic = 0; asic < NUM_ASIC; asic++) {
            for (int channel = 0; channel < 72; channel++) {
                auto c = canvases.get_canvas(adc_canvas[fpga * NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel]->draw_adc();
                c = canvases.get_canvas(waveform_canvas[fpga * NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel]->draw_waveform();
                gPad->SetLogz();
            }
        }
    }
}

void test_decoding() {
    uint8_t line0[PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x00, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line1[PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x01, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line2[PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x02, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line3[PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x03, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};
    uint8_t line4[PACKET_SIZE] = {0xA0, 0x00, 0x24, 0x04, 0x01, 0x52, 0x71, 0x24, 0x5A, 0xB4, 0x15, 0x85, 0x00, 0x00, 0x80, 0x00, 0x05, 0xF0, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00};

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
    auto m = new online_monitor();
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
    uint8_t buffer[PACKET_SIZE];
    // for (int iteration = 0; iteration < 50; iteration++) {
    // for (int iteration = 0; iteration < 61100; iteration++) {
    for (int iteration = 0; iteration < 10000000; iteration++) {
	s->ProcessRequests();
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time).count() > 4) {
            // fs.print_packet_numbers();
            std::cout << "Updating canvases...";
            m->update_canvases();
            gSystem->ProcessEvents();
            start_time = std::chrono::high_resolution_clock::now();
            std::cout << " done!" << std::endl;
        }
        bool good_data = fs.read_packet(buffer);
        if (!good_data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        std::vector<line> lines(36);    // 36 lines per packet
        decode_packet(lines, buffer);
        process_lines(lines, m->line_streams, data_rates);
        for (auto line : lines) {
            line_numbers->Fill(line.line_number);
        }

        // for (int i = 0; i < PACKET_SIZE; i++) {
        //     std::cout << setfill('0') << setw(2) << std::hex << (int)buffer[i] << " ";
        // }
        // std::cout << std::endl << std::endl << std::endl;;
    }
}

int Monitor(int run) {
    gStyle->SetOptStat(0);
    // 
    // test_decoding();
    test_reading(run);
    return 0;
}

int main() {
    TApplication app("app", 0, nullptr);
    Monitor(42);  // answer to the universe
    return 0;
}
