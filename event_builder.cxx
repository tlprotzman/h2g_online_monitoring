#include "event_builder.h"

#include "configuration.h"
#include "server.h"
#include "canvas_manager.h"
#include "single_channel_tree.h"

#include <cstdint>
#include <iostream>
#include <iomanip>

#include <TROOT.h>
#include <TGraph.h>
#include <TLegend.h>
#include <TDatime.h>
#include <TAxis.h>

single_channel_event::single_channel_event(uint32_t fpga_id, uint32_t channel, uint32_t asic_id, uint32_t expected_samples) {
    this->fpga_id = fpga_id;
    this->channel = channel;
    this->asic_id = asic_id;
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

int single_channel_event::get_max_sample() {
    int max_sample = 0;
    for (int i = 1; i < configuration::get_instance()->MAX_SAMPLES; i++) {
        // std::cerr << "i is " << i << ", found samples is " << this->found_samples << " and sample is " << this->samples[i] << std::endl;
        if (this->samples[i] > max_sample) {
            max_sample = this->samples[i];
        }
    }
    auto value = max_sample - this->samples[0];
    if (value < 0) {
        return 0;
    }
    return value;
}

void single_channel_event::write_to_tree() {
    // std::cout << "Writing to tree" << std::endl;
    auto tree = single_channel_tree::get_instance();
    tree->current_fpga_id = fpga_id;
    tree->current_channel = channel;
    tree->current_asic_id = asic_id;
    tree->pedestal = samples[0];
    tree->max_sample = samples[0];
    for (int i = 0; i < found_samples; i++) {
        tree->samples[i] = samples[i];
        if (samples[i] > tree->max_sample) {
            tree->max_sample = samples[i];
        }
    }
    tree->fill_tree();
}

kcu_event::kcu_event(uint32_t ts, uint32_t fpga) {
    auto configs = configuration::get_instance();
    timestamp = ts;
    fpga_id = fpga;
    channels_found = 0;
    channels = std::vector<single_channel_event*>(configs->NUM_CHANNELS * configs->NUM_ASIC);
    for (int i = 0; i < configs->NUM_CHANNELS * configs->NUM_ASIC; i++) {
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
    for (auto e = in_progress_event_buffer.end(); e != in_progress_event_buffer.begin(); e--) {
        if (e->timestamp == single->timestamps[0]) {
            found = true;
            e->channels_found++;
            e->channels[single->channel + configuration::get_instance()->NUM_CHANNELS * single->asic_id] = single;
            if (e->is_complete()) {
                completed_events++;
                completed_event_buffer.push_back(*e);
                in_progress_event_buffer.erase(e);
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
        in_progress_event_buffer.push_back(e);
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


event_thunderdome::event_thunderdome(event_builder **b) {
    builders = b;
}

event_thunderdome::~event_thunderdome() {
}

void event_thunderdome::align_events() {
    // return; // don't crash overnight pls
    // how many events are sitting in the buffer?
    uint32_t max_event_buffer = 0;
    // std::cout << std::endl;
    for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
        // std::cout << "FPGA " << i << " has " << builders[i]->completed_event_buffer.size() << " events in the buffer." << std::endl;
        if (builders[i]->completed_event_buffer.size() == 0) {
            return;
        }
        if (builders[i]->completed_event_buffer.size() > max_event_buffer) {
            max_event_buffer = builders[i]->completed_event_buffer.size();
        }        
    }
    if (max_event_buffer == 0) {
        // No events, why did this get called?  Also need to check for the minimim number of events available, but eh
        return;
    }

    // Get the first event from each FPGA to create an offset
    if (first_event) {
        first_event = false;
        for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
            if (builders[i]->completed_event_buffer.size() > 0) {
                auto e = builders[i]->completed_event_buffer.front();
                event_t0.push_back(e.timestamp);
            }
        }
        std::cout << "Event t0s: ";
        for (auto t : event_t0) {
            std::cout << std::setfill('0') << std::setw(2) << t << " ";
        }
    }

    // Now we need to align the events
    bool running = true;
    while (running) {
        // if any buffers are empty, stop
        for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
            // std::cout << "builder " << i << " has " << builders[i]->completed_event_buffer.size() << " events in the buffer." << std::endl;
            if (builders[i]->completed_event_buffer.size() == 0) {
                running = false;
                break;
            }
        }
        std::cout << "\nClock values: ";
        for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
            if (builders[i]->completed_event_buffer.size() > 0) {
                std::cout << std::setfill('0') << std::setw(2) << builders[i]->completed_event_buffer.front().timestamp << " ";
            }
        }
        std::cout << std::endl;
        std::cout << "t0 values: ";
        for (auto t : event_t0) {
            std::cout << std::setfill('0') << std::setw(2) << t << " ";
        }
        std::cout << std::endl;

        
        uint32_t min = 0xFFFFFFFF;
        uint32_t max = 0;
        float mean = 0;
        for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
            auto e = builders[i]->completed_event_buffer.front();
            auto ts = e.timestamp - event_t0[i];
            if (ts < min) {min = ts;}
            if (ts > max) {max = ts;}
            mean += (float)ts;
        }
        mean /= configuration::get_instance()->NUM_FPGA;


        auto range = max - min;
        if (range < configuration::get_instance()->EVENT_ALIGNMENT_TOLERANCE) {
            // We have an event!
            std::cout << "Found an event!  Range is " << range << std::endl;
            std::vector<kcu_event> events;
            for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
                auto e = builders[i]->completed_event_buffer.front();
                event_t0[i] = e.timestamp;
                events.push_back(e);   // Reset our reference frame
                builders[i]->completed_event_buffer.pop_front();
                if (builders[i]->completed_event_buffer.size() == 0) {
                    running = false;
                }
            }
            if (running == false) {
                break;
            }
            // std::cout << "pushing back event with " << events.size() << " FPGAs" << std::endl;
            built_events.push_back(events);


        } else {
            // We don't have an event, so we need to shift the events
            std::cout << "no event found :(  range is " << range << std::endl;

            std::cout << "fpga 0 ts: " << builders[0]->completed_event_buffer.front().timestamp << std::endl;
            std::cout << "fpga 1 ts: " << builders[1]->completed_event_buffer.front().timestamp << std::endl;
            std::cout << "fpga 2 ts: " << builders[2]->completed_event_buffer.front().timestamp << std::endl;
            std::cout << "fpga 3 ts: " << builders[3]->completed_event_buffer.front().timestamp << std::endl;
            
            // Find the event farthest from the mean
            int max_deviation = 0;
            int max_deviation_index = 0;
            for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
                auto e = builders[i]->completed_event_buffer.front();
                while (e.timestamp < event_t0[i]) {
                    builders[i]->completed_event_buffer.pop_front();
                    if (builders[i]->completed_event_buffer.size() == 0) {
                        running = false;
                        break;
                    }
                    e = builders[i]->completed_event_buffer.front();
                }
                auto ts = e.timestamp - event_t0[i];
                if (abs(ts - mean) > max_deviation) {
                    max_deviation = abs(ts - mean);
                    max_deviation_index = i;
                }
                max_deviation = (builders[max_deviation_index]->completed_event_buffer.front().timestamp - event_t0[max_deviation_index]) - mean;
            }

            // std::cout << "max deviation is " << max_deviation << " at index " << max_deviation_index << std::endl;
            // if the max deviation is above the mean, we drop all other channels
            if (max_deviation > 0) {
                for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
                    if (i != max_deviation_index) {
                        // std::cout << "running path a" << std::endl;
                        builders[i]->completed_event_buffer.pop_front();
                        if (builders[i]->completed_event_buffer.size() == 0) {
                            running = false;
                            break;
                        }
                    }
                }
            } else {
                // We need to shift the events
                // std::cout << "running path b" << std::endl;
                builders[max_deviation_index]->completed_event_buffer.pop_front();
                if (builders[max_deviation_index]->completed_event_buffer.size() == 0) {
                    running = false;
                    break;
                }
            }
        }
    }
}
