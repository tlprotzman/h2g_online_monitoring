#pragma once

#include "canvas_manager.h"
#include "line_stream.h"

#include <TROOT.h>
#include <TFile.h>
#include <TH2.h>

class online_monitor {
private:
    int run_number;
    int timestamp; 
    TFile *output;
    canvas_manager canvases;

    std::vector<TH2*> adc_per_channel;
    std::vector<TH2*> tot_per_channel;
    std::vector<TH2*> toa_per_channel;

    event_builder **builders;


public:
    channel_stream_vector channels;
    line_stream_vector line_streams;
    online_monitor(int run_number);
    ~online_monitor();

    // TRint *app;
    void update_canvases() {
        canvases.update();
        gSystem->ProcessEvents();
    }
    void update_builder_graphs();
    void update();
    void update_events();
};