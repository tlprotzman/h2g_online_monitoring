#include "event_builder.h"

#include "configuration.h"
#include "server.h"
#include "canvas_manager.h"

#include <cstdint>
#include <iostream>

#include <TROOT.h>
#include <TGraph.h>
#include <TLegend.h>
#include <TDatime.h>
#include <TAxis.h>

single_channel_event::single_channel_event(uint32_t fpga_id, uint32_t channel, uint32_t event_id, uint32_t expected_samples) {
    this->fpga_id = fpga_id;
    this->channel = channel;
    this->event_id = event_id;
    this->expected_samples = expected_samples;
    this->found_samples = 0;
    this->complete = false;

    this->timestamps = new uint32_t[expected_samples];
    this->samples = new uint32_t[expected_samples];
}

single_channel_event::~single_channel_event() {
    delete[] this->timestamps;
    delete[] this->samples;
}

bool single_channel_event::add_sample(uint32_t timestamp, uint32_t sample) {
    auto config = configuration::get_instance();
    // Make sure the event isn't already complete
    if (this->found_samples >= this->expected_samples) {
        return false;
    }

    // Check if this is part of the machine gun, or a new event
    if (this->found_samples > 0) {
        uint32_t last_timestamp = this->timestamps[this->found_samples - 1];
        uint32_t time_diff = timestamp - last_timestamp;

        if (time_diff > config->MACHINE_GUN_MAX_TIME) {
            this->found_samples = 0;
            return false;
        }
    }

    this->timestamps[this->found_samples] = timestamp;
    this->samples[this->found_samples] = sample;
    this->found_samples++;

    if (this->found_samples == this->expected_samples) {
        this->complete = true;
    }

    return true;
}

void single_channel_event::fill_waveform(TH2 *waveform, TH1 *max) {
    int max_sample =0;
    int pedestal = samples[0];
    for (int i = 0; i < this->found_samples; i++) {
        waveform->Fill(i, this->samples[i]);
        if (this->samples[i] > max_sample) {
            max_sample = this->samples[i];
        }
    }
    max->Fill(max_sample - pedestal);
}

kcu_event::kcu_event(uint32_t ts, uint32_t fpga) {
    auto configs = configuration::get_instance();
    timestamp = ts;
    fpga_id = fpga;
    channels_found = 0;
    channels = std::vector<single_channel_event*>(configs->NUM_CHANNELS);
    for (int i = 0; i < configs->NUM_CHANNELS; i++) {
        channels[i] = nullptr;
    }
}

kcu_event::~kcu_event() {
    // for (auto &c : channels) {
        // if (c != nullptr) {
            // delete c;
        // }
    // }
}

event_builder::event_builder(uint32_t fpga) {
    fpga_id = fpga;

    completed_events = 0;
    attempted_events = 0;

    auto canvases = canvas_manager::get_instance();
    auto s = server::get_instance()->get_server();
    auto canvas_id = canvases.new_canvas(Form("FPGA_%i_Events", fpga_id), Form("FPGA %i Events", fpga_id), 1200, 800);
    s->Register("/qa_plots/daq", canvases.get_canvas(canvas_id));
    TLegend *legend = new TLegend(0.15, 0.75, 0.48, 0.9);
    legend->SetBorderSize(0);
    
    events_attempted = new TGraph();
    events_attempted->SetName(Form("fpga_%i_events_attempted", fpga_id));
    gROOT->Add(events_attempted);
    events_attempted->SetTitle(Form("FPGA %i Events Attempted", fpga_id));
    events_attempted->GetXaxis()->SetTitle("Time");
    events_attempted->GetXaxis()->SetTimeDisplay(1);
    events_attempted->GetXaxis()->SetTimeFormat("%H:%M:%S");
    events_attempted->GetYaxis()->SetTitle("Events");
    events_attempted->SetLineColor(kBlue);
    events_attempted->SetLineWidth(2);
    events_attempted->Draw("AL");
    legend->AddEntry(events_attempted, "Events Attempted", "l");

    events_complete = new TGraph();
    events_complete->SetName(Form("fpga_%i_events_completed", fpga_id));
    gROOT->Add(events_complete);
    events_complete->SetTitle(Form("FPGA %i Events Completed", fpga_id));
    events_complete->GetXaxis()->SetTitle("Time");
    events_complete->GetXaxis()->SetTimeDisplay(1);
    events_complete->GetXaxis()->SetTimeFormat("%H:%M:%S");
    events_complete->GetYaxis()->SetTitle("Completed Events");
    events_complete->SetLineColor(kRed);
    events_complete->SetLineWidth(2);
    legend->AddEntry(events_complete, "Events Completed", "l");
    events_complete->Draw("L");

    legend->Draw();

    missed_event_fraction = new TGraph();
    missed_event_fraction->SetName(Form("fpga_%i_incomplete_events_percent", fpga_id));
    gROOT->Add(missed_event_fraction);
    missed_event_fraction->SetTitle(Form("FPGA %i Incomplete Events Percent", fpga_id));
    missed_event_fraction->GetXaxis()->SetTitle("Time");
    missed_event_fraction->GetXaxis()->SetTimeDisplay(1);
    missed_event_fraction->GetXaxis()->SetTimeFormat("%H:%M:%S");
    missed_event_fraction->GetYaxis()->SetTitle("Incomplete Events Percent");
    missed_event_fraction->SetLineColor(kGreen+2);
    missed_event_fraction->SetLineWidth(2);
    legend->AddEntry(missed_event_fraction, "Incomplete Events Percent", "l");
    missed_event_fraction->Draw("LY+");
    missed_event_fraction->GetYaxis()->SetRangeUser(0, 1);

    legend->Draw();
    
}

event_builder::~event_builder() {
}
 
void event_builder::channel_hit(single_channel_event *single) {
    // Check if there is already an event for this timestamp
    bool found = false;
    for (auto e = events.end(); e != events.begin(); e--) {
        if (e->timestamp == single->timestamps[0]) {
            found = true;
            e->channels_found++;
            e->channels[single->channel] = single;
            if (e->is_complete()) {
                completed_events++;
                // Process the event
                // std::cout << "Found complete event!  " << e->channels_found << " channels found."  << std::endl;
            }
            break;
        }
    }
    if (!found) {
        // Create a new event
        attempted_events++;
        kcu_event e(single->timestamps[0], single->fpga_id);
        e.channels_found++;
        e.channels[single->channel] = single;
        events.push_back(e);
    }
}

void event_builder::update_stats() {
    // std::cout << "FPGA " << fpga_id << " completed " << completed_events << "/" << attempted_events << " events." << std::endl;
    auto time = TDatime();
    events_attempted->SetPoint(events_attempted->GetN(), time.Convert(), attempted_events);
    events_complete->SetPoint(events_complete->GetN(), time.Convert(), completed_events);
    events_attempted->GetYaxis()->SetRangeUser(0, 1.2 * (float)attempted_events);
    missed_event_fraction->SetPoint(missed_event_fraction->GetN(), time.Convert(), 1 - (float)(completed_events) / attempted_events);

}