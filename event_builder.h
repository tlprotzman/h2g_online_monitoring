#pragma once

#include "configuration.h"

#include <cstdint>
#include <queue>
#include <list>

#include <TH1.h>
#include <TH2.h>
#include <TGraph.h>


class single_channel_event {
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
    single_channel_event(uint32_t fpga_id, uint32_t channel, uint32_t event_id, uint32_t expected_samples);
    ~single_channel_event();

    int get_fpga_id() {return this->fpga_id;}
    bool add_sample(uint32_t timestamp, uint32_t sample);
    bool is_complete() {return this->complete;}
    void fill_waveform(TH2 *waveform, TH1 *max);

    friend class kcu_event;
    friend class event_builder;
};

class kcu_event {
private:
    uint32_t timestamp;
    uint32_t fpga_id;
    uint32_t event_id;
    int channels_found;

    std::vector<single_channel_event*> channels;

public:
    friend class event_builder;

    kcu_event(uint32_t timestamp, uint32_t fpga_id);
    ~kcu_event();

    bool is_complete() {return channels_found == configuration::get_instance()->NUM_CHANNELS;}

};

class event_builder {
private:
    uint32_t fpga_id;
    uint32_t channels_found;

    std::list<kcu_event> events;
    int attempted_events;
    int completed_events;

    TGraph *events_attempted;
    TGraph *events_complete;
    TGraph *missed_event_fraction;
    
    void reset_event();

public:
    event_builder(uint32_t timestamp);
    ~event_builder();

    void channel_hit(single_channel_event *single);
    void update_stats();
};