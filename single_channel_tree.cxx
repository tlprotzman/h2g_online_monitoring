#include "single_channel_tree.h"

#include "configuration.h"


single_channel_tree* single_channel_tree::instance = nullptr;

single_channel_tree::single_channel_tree() {
    num_samples = configuration::get_instance()->MAX_SAMPLES + 1;
    samples = new int[num_samples];
    tree = new TTree("single_channel", "Single Channel Data");
    tree->Branch("fpga_id", &current_fpga_id);
    tree->Branch("asic_id", &current_asic_id);
    tree->Branch("channel", &current_channel);
    tree->Branch("num_samples", &num_samples);
    tree->Branch("max_sample", &max_sample);
    tree->Branch("pedestal", &pedestal);
    tree->Branch("ToA", &ToA);
    tree->Branch("ToT", &ToT);
    tree->Branch("samples", samples, "samples[num_samples]/I");
}