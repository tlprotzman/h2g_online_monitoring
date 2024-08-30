#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>

class file_stream {
private:
    std::ifstream file;
    std::streampos current_head;
    uint16_t current_packet = 0;
    uint32_t missed_packets = 0;
    uint32_t total_packets = 0;
    bool first_packet = true;

public:
    file_stream(const char *fname);
    ~file_stream();
    bool read_packet(uint8_t *buffer);
    void print_packet_numbers();
};
