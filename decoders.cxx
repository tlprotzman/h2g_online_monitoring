#include "decoders.h"

#include "line_stream.h"
#include "channel_stream.h"

#include <cstdint>
#include <iostream>

#include <TH1.h>

int decode_fpga(int fpga_id) {
    return fpga_id;
}

int encode_fpga(int fpga_id) {
    return fpga_id;
}

int decode_asic(int asic_id) {
    if (asic_id == 160) {
        return 0;
    }
    else if (asic_id == 161) {
        return 1;
    }
    return -1;
}

int encode_asic(int asic_id) {
    if (asic_id == 0) {
        return 160;
    }
    else if (asic_id == 1) {
        return 161;
    }
    return -1;
}

int decode_half(int half_id) {
    if (half_id == 36) {
        return 0;
    }
    else if (half_id == 37) {
        return 1;
    }
    return -1;
}

int encode_half(int half_id) {
    if (half_id == 0) {
        return 36;
    }
    else if (half_id == 1) {
        return 37;
    }
    return -1;
}

uint32_t bit_converter(uint8_t *buffer, int start, bool big_endian) {
    if (big_endian) {
        return (buffer[start] << 24) + (buffer[start + 1] << 16) + (buffer[start + 2] << 8) + buffer[start + 3];
    }
    return (buffer[start + 3] << 24) + (buffer[start + 2] << 16) + (buffer[start + 1] << 8) + buffer[start];
}

void decode_line(line &p, uint8_t *buffer) {
    p.asic_id = decode_asic(buffer[0]);
    p.fpga_id = decode_fpga(buffer[1]);
    p.half_id = decode_half(buffer[2]);
    p.line_number = buffer[3];
    p.timestamp = bit_converter(buffer, 4);
    for (int i = 0; i < 8; i++) {
        p.package[i] = bit_converter(buffer, i * 4 + 8);
    }
}

int decode_packet(std::vector<line> &lines, uint8_t *buffer) {
    int decode_ptr = 12; // the header is the first 12 bytes
    for (int i = 0; i < 36; i++) {
        decode_line(lines[i], buffer + decode_ptr);
        decode_ptr += 40;
    }
    return 0;
}

void process_lines(std::vector<line> &lines, line_stream_vector &streams, TH1 *data_rates) {
    for (auto line : lines) {
        if (line.fpga_id == -1 || line.asic_id == -1 || line.half_id == -1) {
            continue;
        }
        data_rates->Fill(4 * line.fpga_id + 2 * line.asic_id + line.half_id);
        streams[line.fpga_id][line.asic_id][line.half_id]->add_line(line);
    }
}
