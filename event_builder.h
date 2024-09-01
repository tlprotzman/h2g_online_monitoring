#pragma once

#include <cstdint>

#include <TH1.h>
#include <TH2.h>

class event {
private:
    uint32_t fpga_id;
    uint32_t channel;
    uint32_t event_id;

    uint32_t expected_samples;
    uint32_t found_samples;
    bool complete;

    uint32_t *timestamps;
    uint32_t *samples;

public:
    event(uint32_t fpga_id, uint32_t channel, uint32_t event_id, uint32_t expected_samples);
    ~event();

    bool add_sample(uint32_t timestamp, uint32_t sample);
    bool is_complete() {return this->complete;}
    void fill_waveform(TH2 *waveform, TH1 *max);
};