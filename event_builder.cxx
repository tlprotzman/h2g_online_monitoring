#include "event_builder.h"
#include "configuration.h"

#include <cstdint>
#include <iostream>

event::event(uint32_t fpga_id, uint32_t channel, uint32_t event_id, uint32_t expected_samples) {
    this->fpga_id = fpga_id;
    this->channel = channel;
    this->event_id = event_id;
    this->expected_samples = expected_samples;
    this->found_samples = 0;
    this->complete = false;

    this->timestamps = new uint32_t[expected_samples];
    this->samples = new uint32_t[expected_samples];
}

event::~event() {
    delete[] this->timestamps;
    delete[] this->samples;
}

bool event::add_sample(uint32_t timestamp, uint32_t sample) {
    // Make sure the event isn't already complete
    if (this->found_samples >= this->expected_samples) {
        return false;
    }

    // Check if this is part of the machine gun, or a new event
    if (this->found_samples > 0) {
        uint32_t last_timestamp = this->timestamps[this->found_samples - 1];
        uint32_t time_diff = timestamp - last_timestamp;

        if (time_diff > MACHINE_GUN_MAX_TIME) {
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

void event::fill_waveform(TH2 *waveform) {
    for (int i = 0; i < this->found_samples; i++) {
        waveform->Fill(i, this->samples[i]);
    }
}