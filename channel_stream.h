#pragma once

#include "event_builder.h"

#include <cstdint>
#include <list>

#include <TH1.h>
#include <TH2.h>
#include <TCanvas.h>
#include <TTree.h>

class channel_stream {
private:
    int fpga_id;
    int asic_id;
    int channel;
    long int packets_attempted;
    long int packets_complete;
    long int events;
    uint32_t last_heartbeat_seconds;
    uint32_t last_heartbeat_milliseconds;

    single_channel_event *current_event;

    TCanvas *c;

    TH2D *adc_samples;
    TH1D *adc_spectra;
    TH1D *tot_spectra;
    TH1D *toa_spectra;

    TH2 *adc_per_channel;
    TH2 *tot_per_channel;
    TH2 *toa_per_channel;

    TH2 *adc_waveform;
    TH1 *adc_max;

    std::list<single_channel_event*> completed_events;

public:
    channel_stream(int fpga_id, int asic_id, int channel, TH2 *adc_per_channel, TH2 *tot_per_channel, TH2 *toa_per_channel);
    ~channel_stream();
    void construct_event(uint32_t timestamp, uint32_t adc);
    void fill_readouts(uint32_t adc, uint32_t tot, uint32_t toa);
    void draw_adc() {adc_spectra->Draw();}
    void draw_tot() {tot_spectra->Draw();}
    void draw_toa() {toa_spectra->Draw();}
    void draw_waveform() {adc_waveform->Draw("col");}
    void draw_max() {adc_max->Draw();}
    int test = 42;

    bool has_events() {return completed_events.size() > 0;}
    int completed_event_size() {return completed_events.size();}
    single_channel_event *get_event() {
        auto e = completed_events.front();
        completed_events.pop_front();
        return e;
    }
};

typedef std::vector<std::vector<std::vector<channel_stream*>>> channel_stream_vector;