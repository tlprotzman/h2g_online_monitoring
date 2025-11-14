#include "decoders.h"

#include "line_stream.h"
#include "channel_stream.h"
#include "configuration.h"


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

uint64_t bit_converter_64(uint8_t *buffer, int start, bool big_endian) {
    if (big_endian) {
        return ((uint64_t)buffer[start] << 56) + ((uint64_t)buffer[start + 1] << 48) + ((uint64_t)buffer[start + 2] << 40) + ((uint64_t)buffer[start + 3] << 32) +
               ((uint64_t)buffer[start + 4] << 24) + ((uint64_t)buffer[start + 5] << 16) + ((uint64_t)buffer[start + 6] << 8) + (uint64_t)buffer[start + 7];
    }
    return ((uint64_t)buffer[start + 7] << 56) + ((uint64_t)buffer[start + 6] << 48) + ((uint64_t)buffer[start + 5] << 40) + ((uint64_t)buffer[start + 4] << 32) +
           ((uint64_t)buffer[start + 3] << 24) + ((uint64_t)buffer[start + 2] << 16) + ((uint64_t)buffer[start + 1] << 8) + (uint64_t)buffer[start];
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

int decode_packet_v012(uint8_t *buffer, line_stream_vector &streams) {
    auto config = configuration::get_instance();
    int decode_ptr = 0;
    // Increment pointer until we find 0xAA5A
    while (decode_ptr < config->PACKET_SIZE - 4) {
        if (buffer[decode_ptr] == 0xAA && buffer[decode_ptr + 1] == 0x5A) {
            break;
        }
        decode_ptr++;
    }
    std::cout << "Found header at byte " << decode_ptr << std::endl;

    // The next 32*6=192 bytes are the data for 6 "lines"
    // Make sure we can actully read 192 bytes
    if (decode_ptr + 192 > config->PACKET_SIZE) {
        std::cerr << "Not enough data for full packet!" << std::endl;
        return -1;
    }
    // Get the asic, fpga, half, etc from the header
    int asic_id = buffer[decode_ptr + 2] & 0x01111; // lower 4 bits
    int fpga_id = (buffer[decode_ptr + 2] >> 4);     // upper 4 bits
    int half = decode_half(buffer[decode_ptr + 3]);
    if (half == -1) {
        std::cerr << "Invalid half ID in packet!" << std::endl;
        return -1;
    }
    std::cout << "Decoding packet for FPGA " << fpga_id << ", ASIC " << asic_id << ", half " << half << std::endl;
    int trg_in_ctr = bit_converter(buffer, decode_ptr + 4, false);
    int trg_out_ctr = bit_converter(buffer, decode_ptr + 8, false);
    int event_ctr = bit_converter(buffer, decode_ptr + 12, false);
    uint64_t timestamp = bit_converter_64(buffer, decode_ptr + 16, false);
    // the last 8 bits are spare for now
    decode_ptr += 32;

    // Make a fake line stream to process this group of data
    for (int line_num = 0; line_num < 5; line_num++) {
        line l;
        l.asic_id = asic_id;
        l.fpga_id = fpga_id;
        l.half_id = half;
        l.line_number = line_num;
        l.timestamp = timestamp & 0xFFFFFFFF; // lower 32 bits
        // Each line has 32 bytes of data (8 words)
        for (int word = 0; word < 8; word++) {
            l.package[word] = bit_converter(buffer, decode_ptr + word * 4, false);
        }
        streams[l.fpga_id][l.asic_id][l.half_id]->add_line(l);
        decode_ptr += 32;
    }
    // On the last pass through, the line stream should have processed the full package?
    // From there, everything should just work like before

    return 0;
}