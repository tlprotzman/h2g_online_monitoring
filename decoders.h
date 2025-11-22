#pragma once

#include "line_stream.h"
#include "channel_stream.h"

#include <cstdint>
#include <vector> 

#include <TH1.h>



int decode_fpga(int fpga_id);
int encode_fpga(int fpga_id);
int decode_asic(int asic_id);
int encode_asic(int asic_id);
int decode_half(int half_id);
int encode_half(int half_id);
uint32_t bit_converter(uint8_t *buffer, int start, bool big_endian = true);
void decode_line(line &p, uint8_t *buffer);
int decode_packet(std::vector<line> &lines, uint8_t *buffer);
void process_lines(std::vector<line> &lines, line_stream_vector &streams, TH1 *data_rates);
int decode_packet_v012(uint8_t *buffer, line_stream_vector &streams, int debug);

