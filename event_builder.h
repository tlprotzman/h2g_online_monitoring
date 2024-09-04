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
    uint32_t asic_id;

    uint32_t expected_samples;
    uint32_t found_samples;
    bool complete;

    uint32_t *timestamps;
    uint32_t *samples;

public:
    single_channel_event(uint32_t fpga_id, uint32_t channel, uint32_t asic_id, uint32_t expected_samples);
    ~single_channel_event();

    int get_fpga_id() {return this->fpga_id;}
    bool add_sample(uint32_t timestamp, uint32_t sample);
    bool is_complete() {return this->complete;}
    void fill_waveform(TH2 *waveform, TH1 *max);
    int get_max_sample();
    void write_to_tree();

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
    friend class event_thunderdome;

    kcu_event(uint32_t timestamp, uint32_t fpga_id);
    ~kcu_event();

    bool is_complete() {return channels_found == 2 * configuration::get_instance()->NUM_CHANNELS;}
    uint32_t get_fpga_id() {return fpga_id;}
    single_channel_event* get_channel(int channel) {return channels[channel];}

};

class event_builder {
private:
    uint32_t fpga_id;
    uint32_t channels_found;

    std::list<kcu_event> in_progress_event_buffer;
    std::list<kcu_event> completed_event_buffer;
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

    friend class event_thunderdome;
};

// 4 events enter, only 1 leaves alive
class event_thunderdome {
private:
    event_builder **builders;
    std::vector<std::vector<kcu_event>> built_events;
    std::vector<std::vector<uint32_t>> event_timestamps;
    bool first_event = true;
    std::vector<uint32_t> event_t0;

public:
    event_thunderdome(event_builder **b);
    ~event_thunderdome();

    void align_events();

    uint32_t get_num_events(){return built_events.size();}
    std::vector<kcu_event> get_event(int n) {return built_events[n];}
    int get_num_events(int n) {return built_events[n].size();}
};