#pragma once
#include <cstdint>
#include <TH1.h>
#include <TH2.h>
#include <TCanvas.h>

class channel_stream {
private:
    int fpga_id;
    int asic_id;
    int channel;
    long int packets_attempted;
    long int packets_complete;

    TCanvas *c;

    TH2D *adc_samples;
    TH1D *adc_spectra;
    TH1D *tot_spectra;
    TH1D *toa_spectra;

    TH2 *adc_per_channel;
    TH2 *tot_per_channel;
    TH2 *toa_per_channel;

public:
    channel_stream(int fpga_id, int asic_id, int channel, TH2 *adc_per_channel, TH2 *tot_per_channel, TH2 *toa_per_channel);
    ~channel_stream();
    void fill_readouts(uint32_t adc, uint32_t tot, uint32_t toa);
    void draw_adc() {adc_spectra->Draw();}
    void draw_tot() {tot_spectra->Draw();}
    void draw_toa() {toa_spectra->Draw();}
    int test = 42;
};

typedef std::vector<std::vector<std::vector<channel_stream*>>> channel_stream_vector;