#include "online_monitor.h"

#include "server.h"
#include "decoders.h"

#include <TROOT.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TLatex.h>



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
            s->Register("/qa_plots/unordered", c);
            c->Divide(9, 8, 0, 0);
            waveform_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(waveform_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/unordered", c);
            c->Divide(9, 8, 0, 0);
            tot_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("tot_fpga_%d_asic_%d", i, j), Form("TOT Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(tot_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/unordered", c);
            c->Divide(9, 8, 0, 0);
            toa_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("toa_fpga_%d_asic_%d", i, j), Form("TOA Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(toa_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/unordered", c);
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

    int channel_map[72] = {64, 63, 66, 65, 69, 70, 67, 68,
	    		   //63, 64, 65, 66, 70, 69, 68, 67, 
	    		   55, 56, 57, 58, 62, 61, 60, 59,
                           45, 46, 47, 48, 52, 51, 50, 49,
                           37, 36, 39, 38, 42, 43, 40, 41,
			   //36, 37, 38, 39, 43, 42, 41, 40,
                           34, 33, 32, 31, 27, 28, 29, 30,
                           25, 26, 23, 24, 20, 19, 22, 21,
			   //26, 25, 24, 23, 19, 20, 21, 22,
                           16, 14, 15, 12, 9, 11, 10, 13,
		           //16, 15, 14, 13,  9, 10, 11, 12,
                           7, 6, 5, 4, 0, 1, 2, 3,
			   // 7,  6,  5,  4,  0,  1,  2,  3
			   };

    uint32_t ordered_adc_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t ordered_waveform_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t ordered_tot_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t ordered_toa_canvas[config->NUM_FPGA * config->NUM_ASIC];
    uint32_t adc_max_canvas[config->NUM_FPGA * config->NUM_ASIC];
    for (int i = 0; i < config->NUM_FPGA; i++) {
        for (int j = 0; j < config->NUM_ASIC; j++) {
            ordered_adc_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_adc_fpga_%d_asic_%d", i, j), Form("ADC Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            auto c = canvases.get_canvas(ordered_adc_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/ordered", c);
            c->Divide(8, 8, 0, 0);
            ordered_waveform_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_waveform_fpga_%d_asic_%d", i, j), Form("Waveform FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(ordered_waveform_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/ordered", c);
            c->Divide(8, 8, 0, 0);
            adc_max_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("adc_max_fpga_%d_asic_%d", i, j), Form("ADC Max FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(adc_max_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/ordered", c);
            c->Divide(8, 8, 0, 0);
            ordered_tot_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_tot_fpga_%d_asic_%d", i, j), Form("TOT Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(ordered_tot_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/ordered", c);
            c->Divide(8, 8, 0, 0);
            ordered_toa_canvas[i * config->NUM_ASIC + j] = canvases.new_canvas(Form("ordered_toa_fpga_%d_asic_%d", i, j), Form("TOA Spectra FPGA %d ASIC %d", i, j), 1200, 800);
            c = canvases.get_canvas(ordered_toa_canvas[i * config->NUM_ASIC + j]);
            s->Register("/qa_plots/ordered", c);
            c->Divide(8, 8, 0, 0);
        }
    }
    for (int fpga = 0; fpga < config->NUM_FPGA; fpga++) {
        for (int asic = 0; asic < config->NUM_ASIC; asic++) {
            for (int channel = 0; channel < 64; channel++) {
                auto c = canvases.get_canvas(ordered_adc_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel_map[channel]]->draw_adc();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_map[channel]));
                gPad->SetLogy();
                
                c = canvases.get_canvas(ordered_waveform_canvas[fpga * config->NUM_ASIC + asic]);
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

                c = canvases.get_canvas(ordered_tot_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel_map[channel]]->draw_tot();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                text->DrawLatexNDC(0.95, 0.56, Form("Channel %d", channel_map[channel]));
                gPad->SetLogy();

                c = canvases.get_canvas(ordered_toa_canvas[fpga * config->NUM_ASIC + asic]);
                c->cd(channel + 1);
                channels[fpga][asic][channel_map[channel]]->draw_toa();
                text->SetTextAlign(33);
                text->DrawLatexNDC(0.95, 0.95, Form("Run %d", run_number));
                text->DrawLatexNDC(0.95, 0.82, Form("FPGA %d", fpga));
                text->DrawLatexNDC(0.95, 0.69, Form("ASIC %d", asic));
                gPad->SetLogy();


            }
        }
    }
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