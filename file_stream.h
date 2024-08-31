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
    uint16_t *current_packet;
    uint32_t *missed_packets;
    uint32_t *total_packets;
    int *canvas_id;
    bool *first_packet;
    TGraph **recieved_packet_graphs;
    TGraph **missed_packet_graphs;
    TGraph **missed_packet_graphs_percent;
    TMultiGraph **mg;

public:
    file_stream(const char *fname);
    ~file_stream();
    bool read_packet(uint8_t *buffer);
    void print_packet_numbers();
};
