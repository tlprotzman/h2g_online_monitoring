#include "online_monitor.h"

#include "server.h"
#include "decoders.h"

#include <TROOT.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TLatex.h>
#include <TParameter.h>

int lfhcal_channel_map[72] = {64, 63, 66, 65, 69, 70, 67, 68,
                       55, 56, 57, 58, 62, 61, 60, 59,
                       45, 46, 47, 48, 52, 51, 50, 49,
                       37, 36, 39, 38, 42, 43, 40, 41,
                       34, 33, 32, 31, 27, 28, 29, 30,
                       25, 26, 23, 24, 20, 19, 22, 21,
                       16, 14, 15, 12,  9, 11, 10, 13,
                        7,  6,  5,  4,  0,  1,  2,  3,
                        -1, -1, -1, -1, -1, -1, -1, -1};

// EEEMCal mapping - instead of "layers", we have a single plane, where each crystal is one connector
// FPGA IP | ID
// 208     | 0
// 209     | 1
// 210     | 2
// 211     | 3
int eeemcal_fpga_map[25] = {0, 3, 3, 0, 3,
                            2, 1, 1, 1, 2,
                            2, 1, 1, 1, 3,
                            2, 2, 1, 2, 3,
                            2, 0, 0, 1, 2};

// ASIC | ID
// 0    | 0
// 1    | 1
int eeemcal_asic_map[25] = { 1, 1, 1, 0, 0,
                             1, 1, 1, 1, 1,
                             1, 0, 0, 0, 0,
                             1, 0, 1, 0, 0,
                             0, 1, 1, 0, 0};

// Connector | ID
// A        | 0
// B        | 1
// C        | 2
// D        | 3
int eeemcal_connector_map[25] = { 2,  0,  1,  0,  1,
                                  0,  2,  0,  3,  3,
                                  1,  2,  0,  3,  0,
                                  2,  0,  1,  1,  2,
                                  3,  1,  1,  1,  2};

// int eeemcal_16i_channel_a_map[16] = { 0,  1,  2,  3,  4,  5,  6,  7,
//                                       9, 10, 11, 12, 13, 14, 15, 16};

// int eeemcal_16i_channel_b_map[16] = {19, 20, 21, 22, 23, 24, 25, 26,
//                                      27, 28, 29, 30, 31, 32, 33, 34};

// int eeemcal_16i_channel_c_map[16] = {55, 56, 57, 58, 59, 60, 61, 62,
//                                      63, 64, 65, 66, 67, 68, 69, 70};

// int eeemcal_16i_channel_d_map[16] = {36, 37, 38, 39, 40, 41, 42, 43,
//                                      45, 46, 47, 48, 49, 50, 51, 52};

int eeemcal_16i_channel_a_map[16] = { 2,  6, 11, 15,  0,  4,  9, 13,
                                      1,  5, 10, 14,  3,  7, 12, 16};
  
int eeemcal_16i_channel_b_map[16] = {20, 24, 29, 33, 18, 22, 27, 31,
                                     19, 23, 28, 32, 21, 25, 30, 34};
  
int eeemcal_16i_channel_c_map[16] = {67, 63, 59, 55, 69, 65, 61, 57,
                                     70, 66, 60, 56, 68, 64, 58, 54};
                                       
int eeemcal_16i_channel_d_map[16] = {50, 46, 40, 36, 52, 48, 42, 38,
                                     51, 47, 43, 39, 49, 45, 41, 37};

int *eeemcal_16i_channel_map[4] = {eeemcal_16i_channel_a_map, eeemcal_16i_channel_b_map, eeemcal_16i_channel_c_map, eeemcal_16i_channel_d_map};

int eeemcal_4x4_channel_a_map[4] = {0, 4, 9, 12};
int eeemcal_4x4_channel_b_map[4] = {20, 24, 27, 31};
int eeemcal_4x4_channel_c_map[4] = {58, 62, 65, 69};
int eeemcal_4x4_channel_d_map[4] = {38, 42, 48, 52};
int *eeemcal_4x4_channel_map[4] = {eeemcal_4x4_channel_a_map, eeemcal_4x4_channel_b_map, eeemcal_4x4_channel_c_map, eeemcal_4x4_channel_d_map};

int eeemcal_16p_channel_map[4] = {6, 26, 63, 46};


int lfhcal_get_channel_x(int channel) {
    return channel % 4;
}

int lfhcal_get_channel_y(int channel) {
    return (channel % 8) > 3 ? 1 : 0;
}

int lfhcal_get_channel_z(int channel) {
    return channel / 8;
} 


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
        adc_per_channel.push_back(new TH2D(Form("strip_adc_per_channel_%d", i), Form("Run %03d ADC per Channel FPGA %d", run_number, i), 144, 0, 144, 1024, 0, config->MAX_ADC));
        tot_per_channel.push_back(new TH2D(Form("strip_tot_per_channel_%d", i), Form("Run %03d TOT per Channel FPGA %d", run_number, i), 144, 0, 144, 1024, 0, config->MAX_TOT));
        toa_per_channel.push_back(new TH2D(Form("strip_toa_per_channel_%d", i), Form("Run %03d TOA per Channel FPGA %d", run_number, i), 144, 0, 144, 1024, 0, config->MAX_TOA));
        s->Register("/QA Plots/Spectra/adc", adc_per_channel.back());
        s->Register("/QA Plots/Spectra/tot", tot_per_channel.back());
        s->Register("/QA Plots/Spectra/toa", toa_per_channel.back());
        int c = canvases.new_canvas(Form("FPGA_%i", i), Form("FPGA %i", i), 1200, 800);
        auto canvas = canvases.get_canvas(c);
        s->Register("/QA Plots/FPGA Summaries", canvas);

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
                line_streams[fpga][asic].push_back(l);
            }
            for (int channel = 0; channel < 72; channel++) {
                auto c = new channel_stream(fpga, asic, channel, adc_per_channel[fpga], tot_per_channel[fpga], toa_per_channel[fpga]);
                channels[fpga][asic].push_back(c);
            }
        }
    }

    // The line streams need to be able to fill the channel streams
    for (auto fpga : line_streams) {
        for (auto asic : fpga) {
            for (auto half : asic) {
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
    uint32_t tot_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t toa_canvas[config->NUM_FPGA * config->NUM_ASIC];
    for (int i = 0; i < config->NUM_FPGA; i++) {
        for (int j = 0; j < config->NUM_ASIC; j++) {
            adc_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("adc_fpga_%d_asic_%d", i, j), Form("ADC Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            auto c = canvases.get_canvas(adc_canvas[i * config->NUM_ASIC + j]);
            s->Register("/QA Plots/Spectra/adc", c);
            c->Divide(9, 8, 0, 0);
            waveform_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(waveform_canvas[i * config->NUM_ASIC + j]);
            s->Register("/QA Plots/Waveform", c);
            c->Divide(9, 8, 0, 0);
            tot_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("tot_fpga_%d_asic_%d", i, j), Form("TOT Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(tot_canvas[i * config->NUM_ASIC + j]);
            s->Register("/QA Plots/Spectra/tot", c);
            c->Divide(9, 8, 0, 0);
            toa_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("toa_fpga_%d_asic_%d", i, j), Form("TOA Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(toa_canvas[i * config->NUM_ASIC + j]);
            s->Register("/QA Plots/Spectra/toa", c);
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
                c = canvases.get_canvas(tot_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel]->draw_tot();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel));
                gPad->SetLogy();
                c = canvases.get_canvas(toa_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel]->draw_toa();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel));
                gPad->SetLogy();
            }
        }
    }
    
    if (config->DETECTOR_ID == 1) { // LFHCAL Configuration
        uint32_t ordered_adc_canvas[config->NUM_FPGA * config->NUM_ASIC];
        uint32_t ordered_waveform_canvas[config->NUM_FPGA * config->NUM_ASIC];
        uint32_t ordered_tot_canvas[config->NUM_FPGA * config->NUM_ASIC];
        uint32_t ordered_toa_canvas[config->NUM_FPGA * config->NUM_ASIC];
        uint32_t adc_max_canvas[config->NUM_FPGA * config->NUM_ASIC];
        for (int i = 0; i < config->NUM_FPGA; i++) {
            for (int j = 0; j < config->NUM_ASIC; j++) {
                ordered_adc_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_adc_fpga_%d_asic_%d", i, j), Form("ADC Spectra FPGA %d ASIC %d", i, j), 1200, 800);
                auto c = canvases.get_canvas(ordered_adc_canvas[i * config->NUM_ASIC + j]);
                s->Register("/LFHCal", c);
                c->Divide(8, 8, 0, 0);
                ordered_waveform_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
                c = canvases.get_canvas(ordered_waveform_canvas[i * config->NUM_ASIC + j]);
                s->Register("/LFHCal", c);
                c->Divide(8, 8, 0, 0);
                adc_max_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("adc_max_fpga_%d_asic_%d", i, j), Form("ADC Max FPGA %d ASIC %d", i, j), 1200, 800);
                c = canvases.get_canvas(adc_max_canvas[i * config->NUM_ASIC + j]);
                s->Register("/LFHCal", c);
                c->Divide(8, 8, 0, 0);
                ordered_tot_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_tot_fpga_%d_asic_%d", i, j), Form("TOT Spectra FPGA %d ASIC %d", i, j), 1200, 800);
                c = canvases.get_canvas(ordered_tot_canvas[i * config->NUM_ASIC + j]);
                s->Register("/LFHCal", c);
                c->Divide(8, 8, 0, 0);
                ordered_toa_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_toa_fpga_%d_asic_%d", i, j), Form("TOA Spectra FPGA %d ASIC %d", i, j), 1200, 800);
                c = canvases.get_canvas(ordered_toa_canvas[i * config->NUM_ASIC + j]);
                s->Register("/LFHCal", c);
                c->Divide(8, 8, 0, 0);
            }
        }
        for (int fpga = 0; fpga < config->NUM_FPGA; fpga++) {
            for (int asic = 0; asic < config->NUM_ASIC; asic++) {
                for (int channel = 0; channel < 64; channel++) {
                    auto c = canvases.get_canvas(ordered_adc_canvas[fpga * config->NUM_ASIC + asic]);
                    c->cd(channel + 1);
                    channels[fpga][asic][lfhcal_channel_map[channel]]->draw_adc();
                    text->SetTextAlign(33);
                    text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                    text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                    text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                    text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", lfhcal_channel_map[channel]));
                    gPad->SetLogy();
                    
                    c = canvases.get_canvas(ordered_waveform_canvas[fpga * config->NUM_ASIC + asic]);
                    c->cd(channel + 1);
                    channels[fpga][asic][lfhcal_channel_map[channel]]->draw_waveform();
                    text->SetTextAlign(33);
                    text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                    text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                    text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                    text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", lfhcal_channel_map[channel]));
                    gPad->SetLogz();

                    c = canvases.get_canvas(adc_max_canvas[fpga * config->NUM_ASIC + asic]);
                    c->cd(channel + 1);
                    channels[fpga][asic][lfhcal_channel_map[channel]]->draw_max();
                    text->SetTextAlign(33);
                    text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                    text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                    text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                    text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", lfhcal_channel_map[channel]));
                    text->DrawLatexNDC(0.95, 0.43, Form("max(samples) - samples[0]"));
                    gPad->SetLogy();

                    c = canvases.get_canvas(ordered_tot_canvas[fpga * config->NUM_ASIC + asic]);
                    c->cd(channel + 1);
                    channels[fpga][asic][lfhcal_channel_map[channel]]->draw_tot();
                    text->SetTextAlign(33);
                    text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                    text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                    text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                    text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", lfhcal_channel_map[channel]));
                    gPad->SetLogy();

                    c = canvases.get_canvas(ordered_toa_canvas[fpga * config->NUM_ASIC + asic]);
                    c->cd(channel + 1);
                    channels[fpga][asic][lfhcal_channel_map[channel]]->draw_toa();
                    text->SetTextAlign(33);
                    text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                    text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                    text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                    gPad->SetLogy();
                }
            }
        }
    }

    else if (config->DETECTOR_ID == 2) { // EEEMCal Configuration
        // 16 individual readout
        // Let's try to put everything on one canvas....
        uint32_t waveform_mega__canvas = canvases.new_canvas("Waveforms_eeemcal_individual_readout", "Individual Readout", 1200, 800);
        auto c = canvases.get_canvas(waveform_mega__canvas);
        s->Register("/EEEMCal/16 Individual", c);
        c->Divide(5, 5, 0.0002, 0.0002);
        for (int i = 0; i < 25; i++) {
            c->cd(i + 1);
            gPad->Divide(4, 4, 0, 0);
            for (int sipm = 0; sipm < 16; sipm++) {
                c->cd(i + 1);
                gPad->cd(sipm + 1);
                int channel_fpga = eeemcal_fpga_map[i];
                int channel_asic = eeemcal_asic_map[i];
                int channel_channel = eeemcal_16i_channel_map[eeemcal_connector_map[i]][sipm];
                channels[channel_fpga][channel_asic][channel_channel]->draw_waveform();
                // text->SetTextAlign(33);
                // text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                // text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
                // text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
                // text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
                // gPad->SetLogz();
            }
        }

        for (int sipm = 0; sipm < 16; sipm++) {
            uint32_t waveform_individual_readout_canvas = canvases.new_canvas(Form("Waveforms_eeemcal_individual_readout_SiPM%d", sipm), "Individual Readout", 1200, 800);
            c = canvases.get_canvas(waveform_individual_readout_canvas);
            s->Register(Form("/EEEMCal/16 Individual/SiPM %d", sipm), c);
            c->Divide(5, 5, 0, 0);
            for (int i = 0; i < 25; i++) {
                c->cd(i + 1);
                int channel_fpga = eeemcal_fpga_map[i];
                int channel_asic = eeemcal_asic_map[i];
                int channel_channel = eeemcal_16i_channel_map[eeemcal_connector_map[i]][sipm];
                channels[channel_fpga][channel_asic][channel_channel]->draw_waveform();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
                gPad->SetLogz();
            }

            uint32_t adc_individual_readout_canvas = canvases.new_canvas(Form("ADC_eeemcal_individual_readout_SiPM%d", sipm), "Individual Readout", 1200, 800);
            c = canvases.get_canvas(adc_individual_readout_canvas);
            s->Register(Form("/EEEMCal/16 Individual/SiPM %d", sipm), c);
            c->Divide(5, 5, 0, 0);
            for (int i = 0; i < 25; i++) {
                c->cd(i + 1);
                int channel_fpga = eeemcal_fpga_map[i];
                int channel_asic = eeemcal_asic_map[i];
                int channel_channel = eeemcal_16i_channel_map[eeemcal_connector_map[i]][sipm];
                channels[channel_fpga][channel_asic][channel_channel]->draw_adc();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
                gPad->SetLogy();
            }

            uint32_t tot_individual_readout_canvas = canvases.new_canvas(Form("ToT_eeemcal_individual_readout_SiPM%d", sipm), "Individual Readout", 1200, 800);
            c = canvases.get_canvas(tot_individual_readout_canvas);
            s->Register(Form("/EEEMCal/16 Individual/SiPM %d", sipm), c);
            c->Divide(5, 5, 0, 0);
            for (int i = 0; i < 25; i++) {
                c->cd(i + 1);
                int channel_fpga = eeemcal_fpga_map[i];
                int channel_asic = eeemcal_asic_map[i];
                int channel_channel = eeemcal_16i_channel_map[eeemcal_connector_map[i]][sipm];
                channels[channel_fpga][channel_asic][channel_channel]->draw_tot();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
                gPad->SetLogy();
            }

            uint32_t toa_individual_readout_canvas = canvases.new_canvas(Form("ToA_eeemcal_individual_readout_SiPM%d", sipm), "Individual Readout", 1200, 800);
            c = canvases.get_canvas(toa_individual_readout_canvas);
            s->Register(Form("/EEEMCal/16 Individual/SiPM %d", sipm), c);
            c->Divide(5, 5, 0, 0);
            for (int i = 0; i < 25; i++) {
                c->cd(i + 1);
                int channel_fpga = eeemcal_fpga_map[i];
                int channel_asic = eeemcal_asic_map[i];
                int channel_channel = eeemcal_16i_channel_map[eeemcal_connector_map[i]][sipm];
                channels[channel_fpga][channel_asic][channel_channel]->draw_toa();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
                gPad->SetLogy();
            }
        }


        // 16 in parallel configurations
        uint32_t waveform_parallel_readout_canvas = canvases.new_canvas("Waveforms_eeemcal_parallel_readout", "Parallel Readout", 1200, 800);
        c = canvases.get_canvas(waveform_parallel_readout_canvas);
        s->Register("/EEEMCal/16 Parallel", c);
        c->Divide(5, 5, 0, 0);
        for (int i = 0; i < 25; i++) {
            c->cd(i + 1);
            int channel_fpga = eeemcal_fpga_map[i];
            int channel_asic = eeemcal_asic_map[i];
            int channel_channel = eeemcal_16p_channel_map[eeemcal_connector_map[i]];
            channels[channel_fpga][channel_asic][channel_channel]->draw_waveform();
            text->SetTextAlign(33);
            text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
            text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
            text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
            text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
            gPad->SetLogz();
        }

        uint32_t adc_parallel_readout_canvas = canvases.new_canvas("ADC_eeemcal_parallel_readout", "Parallel Readout", 1200, 800);
        c = canvases.get_canvas(adc_parallel_readout_canvas);
        s->Register("/EEEMCal/16 Parallel", c);
        c->Divide(5, 5, 0, 0);
        for (int i = 0; i < 25; i++) {
            c->cd(i + 1);
            int channel_fpga = eeemcal_fpga_map[i];
            int channel_asic = eeemcal_asic_map[i];
            int channel_channel = eeemcal_16p_channel_map[eeemcal_connector_map[i]];
            channels[channel_fpga][channel_asic][channel_channel]->draw_adc();
            text->SetTextAlign(33);
            text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
            text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
            text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
            text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
            gPad->SetLogy();
        }

        uint32_t tot_parallel_readout_canvas = canvases.new_canvas("ToT_eeemcal_parallel_readout", "Parallel Readout", 1200, 800);
        c = canvases.get_canvas(tot_parallel_readout_canvas);
        s->Register("/EEEMCal/16 Parallel", c);
        c->Divide(5, 5, 0, 0);
        for (int i = 0; i < 25; i++) {
            c->cd(i + 1);
            int channel_fpga = eeemcal_fpga_map[i];
            int channel_asic = eeemcal_asic_map[i];
            int channel_channel = eeemcal_16p_channel_map[eeemcal_connector_map[i]];
            channels[channel_fpga][channel_asic][channel_channel]->draw_tot();
            text->SetTextAlign(33);
            text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
            text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
            text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
            text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
            gPad->SetLogy();
        }

        uint32_t toa_parallel_readout_canvas = canvases.new_canvas("ToA_eeemcal_parallel_readout", "Parallel Readout", 1200, 800);
        c = canvases.get_canvas(toa_parallel_readout_canvas);
        s->Register("/EEEMCal/16 Parallel", c);
        c->Divide(5, 5, 0, 0);
        for (int i = 0; i < 25; i++) {
            c->cd(i + 1);
            int channel_fpga = eeemcal_fpga_map[i];
            int channel_asic = eeemcal_asic_map[i];
            int channel_channel = eeemcal_16p_channel_map[eeemcal_connector_map[i]];
            channels[channel_fpga][channel_asic][channel_channel]->draw_toa();
            text->SetTextAlign(33);
            text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
            text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", channel_fpga));
            text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", channel_asic));
            text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_channel));
            gPad->SetLogy();
        }

    }

    // Set up event display
    // event_drawn = 0;
    // c = canvases.new_canvas("event_display_canvas", Form("Run %03d Event Display", run_number), 1200, 800);
    // auto canvas = canvases.get_canvas(c);
    // event_display = new TH3D("event_display", Form("Run %03d Event Display", run_number), 64, 0, 64, 4, 0, 4, 2, 0, 2);
    // event_display->Draw("BOX2");
    // s->Register("/event_display", canvas);

    // Register commands
    TParameter<bool> *reset = new TParameter<bool>("reset", false);
    gDirectory->GetList()->Add(reset);
    s->Register("/", reset);
    s->Hide("/reset");
    s->RegisterCommand("/Clear_Histograms", "/reset/->SetVal(1);");
    // s->SetItemField("/Clear_Histograms", "_fastcmd", "true");
    // s->SetItemField('/Clear_Histograms", "hide", "true");')
}

online_monitor::~online_monitor() {
    server::get_instance()->get_server()->SetTerminate();
    server::get_instance()->kill_server();
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
    return;
    thunderdome->align_events();
    std::cout << "Built " << thunderdome->get_num_events() << " events" << std::endl;
}

void online_monitor::make_event_display() {
    return;
    // Make sure we actually have completed events...
    if (thunderdome->get_num_events() == 0) {
        std::cout << "No events to display" << std::endl;
        return;
    }
    // Clear existing event display
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
                auto channel_id = lfhcal_channel_map[channel % configuration::get_instance()->NUM_CHANNELS];
                if (channel_id < 0) {   // We record more channels than we use
                    continue;
                }
                auto asic = channel >= configuration::get_instance()->NUM_CHANNELS ? 1 : 0;
                auto x = lfhcal_get_channel_x(channel
                );
                auto y = lfhcal_get_channel_y(channel);
                auto z = fpga_factors[fpga] * 16 + 8 * asic + lfhcal_get_channel_z(channel % configuration::get_instance()->NUM_CHANNELS);
                // z = lfhcal_get_channel_z(channel % configuration::get_instance()->NUM_CHANNELS);

                // std::cout << "FPGA" << fpga << "Channel" << channel << "Filling " << z << " " << x << " " << y << " " << c->get_max_sample() << std::endl;
                event_display->Fill(z, x, y, c->get_max_sample());
                // event_display->Fill(z, x, y, 1);
            }
        }
    }
}

void online_monitor::check_reset() {
    auto reset = dynamic_cast<TParameter<bool>*>(gROOT->FindObject("reset"));

    if (reset != nullptr && reset->GetVal()) {
        std::cout << "Resetting histograms..." << std::endl;
        reset->SetVal(0);
        for (auto hist : adc_per_channel) {
            hist->Reset("ICESM");
        }
        for (auto hist : tot_per_channel) {
            hist->Reset("ICESM");
        }
        for (auto hist : toa_per_channel) {
            hist->Reset("ICESM");
        }
        for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
            for (int j = 0; j < configuration::get_instance()->NUM_ASIC; j++) {
                for (int k = 0; k < 72; k++) {
                    channels[i][j][k]->reset();
                }
            }
        }
    } else if (reset == nullptr) {
        std::cerr << "Reset parameter not found" << std::endl;
    }
}