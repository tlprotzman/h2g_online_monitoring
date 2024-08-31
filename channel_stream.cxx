#include "channel_stream.h"

#include "configuration.h"
#include "server.h"

#include <TH1.h>
#include <TH2.h>
#include <TCanvas.h>
#include <iostream>

channel_stream::channel_stream(int fpga_id, int asic_id, int channel, TH2 *adc_per_channel, TH2 *tot_per_channel, TH2 *toa_per_channel) {
    this->fpga_id = fpga_id;
    this->asic_id = asic_id;
    this->channel = channel;
    packets_attempted = 0;
    packets_complete = 0;
    events = 0;
    current_event = nullptr;
    // std::cout << "and here: " << asic_id << std::endl;
    
    adc_spectra = new TH1D(Form("adc_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("ADC Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), MAX_ADC/2, 0, MAX_ADC);
    tot_spectra = new TH1D(Form("tot_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("TOT Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), MAX_TOT/2, 0, MAX_TOT);
    toa_spectra = new TH1D(Form("toa_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("TOA Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), MAX_TOA/2, 0, MAX_TOA);

    adc_waveform = new TH2D(Form("adc_waveform_%d_%d_%d", fpga_id, asic_id, channel), Form("ADC Waveform FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), MAX_SAMPLES, 0, MAX_SAMPLES, 1024, 0, MAX_ADC);

    auto s = server::get_instance()->get_server();
    s->Register(Form("/qa_plots/graphs/individual/fpga%d/adc", fpga_id), adc_spectra);
    s->Register(Form("/qa_plots/graphs/individual/fpga%d/tot", fpga_id), tot_spectra);
    s->Register(Form("/qa_plots/graphs/individual/fpga%d/toa", fpga_id), toa_spectra);

    // s->Register(Form("/qa_plots/graphs/individual/fpga%d/adc_waveform", fpga_id), adc_waveform);
    s->Register(Form("/qa_plots/test/fpga%d/adc_waveform", fpga_id), adc_waveform);

    this->adc_per_channel = adc_per_channel;
    this->tot_per_channel = tot_per_channel;
    this->toa_per_channel = toa_per_channel;
}

channel_stream::~channel_stream() {
    if (fpga_id == 0 && asic_id == 0 && channel == 0) {
        std::cout << "Total entries: " << adc_spectra->GetEntries() << " Mean Value: " << adc_spectra->GetMean() << std::endl;
    }
}

void channel_stream::construct_event(uint32_t timestamp, uint32_t adc) {
    if (current_event == nullptr) {
        current_event = new event(fpga_id, channel, events, MAX_SAMPLES);
    }
    auto success = current_event->add_sample(timestamp, adc);
    if (!success) {
        delete current_event;
        current_event = nullptr;
        // std::cerr << "going to recurse...." << std::endl;
        construct_event(timestamp, adc);  // is this a bad idea?  probably
    }
    if (current_event->is_complete()) {
        // std::cout << "Event complete!" << std::endl;
        current_event->fill_waveform(adc_waveform);
        delete current_event;
        current_event = nullptr;
        events++;
    }
}

void channel_stream::fill_readouts(uint32_t adc, uint32_t tot, uint32_t toa) {
    // std::cout << "adc: " << adc << " tot: " << tot << " toa: " << toa << std::endl;
    // std::cout << "channel? " << channel << std::endl;
//    std::cout << this << " " << fpga_id << " " << asic_id << " " << channel << std::endl;
    if (adc > 5) {
        adc_spectra->Fill(adc);
        tot_spectra->Fill(tot);
        toa_spectra->Fill(toa);
        // std::cout << "asic: " << asic_id << std::endl;
        adc_per_channel->Fill(72 * asic_id + channel, adc);
        tot_per_channel->Fill(72 * asic_id + channel, tot);
        toa_per_channel->Fill(72 * asic_id + channel, toa);
    }
}
