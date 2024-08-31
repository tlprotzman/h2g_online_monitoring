#pragma once
#include <cstdint>

#include "channel_stream.h"

struct line {
    int32_t asic_id;
    int32_t fpga_id;
    int32_t half_id;
    int32_t line_number;
    uint32_t timestamp;
    uint32_t package[8];
};

class line_stream {
private:
    uint32_t timestamp = 0;
    uint32_t **package;
    uint8_t last_line_received;
    channel_stream_vector channels;
    void decode(int32_t asic_id, int32_t fpga_id, int32_t half_id, uint32_t timestamp);

public:
    line_stream();
    ~line_stream() {};

    void add_line(line &l);
    void associate_channels(channel_stream_vector &channels);
};

typedef std::vector<std::vector<std::vector<line_stream*>>> line_stream_vector;