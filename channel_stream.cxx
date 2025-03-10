#include "channel_stream.h"

#include "configuration.h"
#include "server.h"
#include "event_builder.h"

#include <TH1.h>
#include <TH2.h>
#include <TCanvas.h>
#include <iostream>

channel_stream::channel_stream(int fpga_id, int asic_id, int channel, TH2 *adc_per_channel, TH2 *tot_per_channel, TH2 *toa_per_channel) {
    this->fpga_id = fpga_id;
    this->asic_id = asic_id;
    this->channel = channel;
    auto config = configuration::get_instance();
    packets_attempted = 0;
    packets_complete = 0;
    events = 0;
    current_event = nullptr;
    
		//adc_spectra = new TH1D(Form("adc_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("ADC Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), config->MAX_ADC/2, 0, config->MAX_ADC);
    adc_spectra = new TH1D(Form("adc_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("ADC Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), 300, 0, 300);
    tot_spectra = new TH1D(Form("tot_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("TOT Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), config->MAX_TOT/2, 0, config->MAX_TOT);
    toa_spectra = new TH1D(Form("toa_spectra_%d_%d_%d", fpga_id, asic_id, channel), Form("TOA Spectra FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), config->MAX_TOA/2, 0, config->MAX_TOA);
    adc_spectra->SetTitle("");
    tot_spectra->SetTitle("");
    toa_spectra->SetTitle("");
    
    adc_waveform = new TH2D(Form("adc_waveform_%d_%d_%d", fpga_id, asic_id, channel), Form("ADC Waveform FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), config->MAX_SAMPLES, 0, config->MAX_SAMPLES, 1024, 0, config->MAX_ADC);
    adc_waveform->SetTitle("");

    adc_max = new TH1D(Form("adc_max_%d_%d_%d", fpga_id, asic_id, channel), Form("ADC Max FPGA %d ASIC %d Channel %d", fpga_id, asic_id, channel), 1024, 0, config->MAX_ADC);

    auto s = server::get_instance()->get_server();
    s->Register(Form("/QA Plots/Spectra/individual/fpga%d/adc", fpga_id), adc_spectra);
    s->Register(Form("/QA Plots/Spectra/individual/fpga%d/tot", fpga_id), tot_spectra);
    s->Register(Form("/QA Plots/Spectra/individual/fpga%d/toa", fpga_id), toa_spectra);
    s->Register(Form("/QA Plots/Waveform/fpga%d/adc_waveform", fpga_id), adc_waveform);
    s->Register(Form("/QA Plots/Waveform/fpga%d/adc_max", fpga_id), adc_max);

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
        current_event = new single_channel_event(fpga_id, channel, asic_id, configuration::get_instance()->MAX_SAMPLES);
    }
    auto success = current_event->add_sample(timestamp, adc);
    if (!success) {
        delete current_event;
        current_event = nullptr;
        construct_event(timestamp, adc);  // is this a bad idea?  probably
    }
    if (current_event->is_complete()) {
        current_event->fill_waveform(adc_waveform, adc_max);
        current_event->write_to_tree();
        completed_events.push_back(current_event);
        current_event = nullptr;
        events++;
    }
}

void channel_stream::fill_readouts(uint32_t adc, uint32_t tot, uint32_t toa) {
    adc_spectra->Fill(adc);
    tot_spectra->Fill(tot);
    toa_spectra->Fill(toa);

    adc_per_channel->Fill(72 * asic_id + channel, adc);
    tot_per_channel->Fill(72 * asic_id + channel, tot);
    toa_per_channel->Fill(72 * asic_id + channel, toa);
}

void channel_stream::reset() {
    adc_spectra->Reset("ICESM");
    tot_spectra->Reset("ICESM");
    toa_spectra->Reset("ICESM");
    adc_waveform->Reset("ICESM");
}