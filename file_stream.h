#pragma once

#include "configuration.h"

#include <TGraph.h>
#include <TMultiGraph.h>

#include <cstdint>
#include <fstream>
#include <iostream>

class file_stream {
private:
    std::ifstream file;
    std::streampos current_head;
    uint16_t current_packet[NUM_FPGA];
    uint32_t missed_packets[NUM_FPGA];
    uint32_t total_packets[NUM_FPGA];
    int canvas_id[NUM_FPGA];
    bool first_packet[NUM_FPGA];
    TGraph *recieved_packet_graphs[NUM_FPGA];
    TGraph *missed_packet_graphs[NUM_FPGA];
    TGraph *missed_packet_graphs_percent[NUM_FPGA];
    TMultiGraph *mg[NUM_FPGA];

public:
    file_stream(const char *fname);
    ~file_stream();
    bool read_packet(uint8_t *buffer);
    void print_packet_numbers();
};
