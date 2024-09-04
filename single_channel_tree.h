#pragma once

#include "TTree.h"

class single_channel_tree {
private:
    single_channel_tree(); // Private constructor to prevent direct instantiation

    single_channel_tree(const single_channel_tree&) = delete; // Prevent copy constructor
    single_channel_tree& operator=(const single_channel_tree&) = delete; // Prevent copy assignment

    static single_channel_tree* instance; // Static pointer to the single instance
    TTree *tree;

    

public:
    static single_channel_tree* get_instance() {
 // ======================================
 // This is critical section
        if (instance == nullptr) { // race condition can happen here.
            instance = new single_channel_tree(); // Create the instance on first call
        }
 // ======================================
        return instance;
    }

    void fill_tree() {tree->Fill();}
    void write_tree() {tree->Write();}


    int current_fpga_id;
    int current_asic_id;
    int current_channel;
    int num_samples;
    int max_sample;
    int pedestal;
    int ToA;
    int ToT;
    int *samples;
};
