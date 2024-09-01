
#include "line_stream.h"
#include "configuration.h"

#include <iostream>

line_stream::line_stream() {
    // std::cout << "running constructor" << std::endl;
    uint32_t **test_package;
    auto config = configuration::get_instance();
    package = (uint32_t**)malloc(config->NUM_LINES * sizeof(uint32_t *));
    for (int i = 0; i < config->NUM_LINES; i++) {
        package[i] = (uint32_t*)malloc(8 * sizeof(uint32_t));
        for (int j = 0; j < 8; j++) {
            package[i][j] = 0;
        }
    }
}

void line_stream::associate_channels(channel_stream_vector &c) {
    this->channels = c;
    // std::cout << "address of c: " << &c << std::endl;
    // std::cout << "address of this->channels: " << &(this->channels) << std::endl;
    // std::cout << "address of this: " << this << std::endl;
    // std::cout << "point b: " << channels[0][0][0]->test << std::endl;
}


void line_stream::add_line(line &l) {
    // return;
    // std::cout << "Adding line" << std::endl;
    // std::cout << l.fpga_id << " " << l.asic_id << " " << l.half_id << " " << l.line_number << " " << l.timestamp << std::endl;
    // If we get a new timestamp, reset the package
    if (l.line_number == 0) {
        timestamp = l.timestamp;
        last_line_received = -1;
    } 
    // If we don't get the next line, reset the package
    if (l.line_number != last_line_received + 1) {
        timestamp = 0;
        last_line_received = -1;
    }
    package[l.line_number][0] = l.package[0];
    package[l.line_number][1] = l.package[1];
    package[l.line_number][2] = l.package[2];
    package[l.line_number][3] = l.package[3];
    package[l.line_number][4] = l.package[4];
    package[l.line_number][5] = l.package[5];
    package[l.line_number][6] = l.package[6];
    package[l.line_number][7] = l.package[7];

    last_line_received = l.line_number;
    // If we have a complete package, process it
    if (l.line_number == 4) {
        // Process the package
        // std::cout << "Found complete package!" << std::endl;
        decode(l.asic_id, l.fpga_id, l.half_id, l.timestamp);
        // Reset the package
        timestamp = 0;
        last_line_received = -1;
    }
}

void line_stream::decode(int32_t asic_id, int32_t fpga_id, int32_t half_id, uint32_t timestamp) {
    auto config = configuration::get_instance();
    // once we have received all 5 lines, we need to decode it
    auto header = package[0][0];
    auto cm = package[0][1];
    auto calib = package[3][5];
    auto crc_32 = package[4][7];
    int ch = 0;
    std::vector<uint32_t> channel_vec(36);
    for (int i = 0; i < config->NUM_LINES; i++) {
        for (int j = 0; j < 8; j++) {
            // Skip what we've already defined
            if ((i == 0 && j == 0) | (i == 0 && j == 1) | (i == 3 && j == 5) | (i == 4 && j == 7)) {
                continue;
            }
            channel_vec[ch] = package[i][j];
            ch++;
        }
    }

    // Now we have each channel, we can decode the ADC value out of it
    // [Tc] [Tp][10b ADC][10b TOT] [10b TOA] (case 4 from the data sheet);
    for (int i = 0; i < 36; i++) {
        auto channel = channel_vec[i];
        auto Tc = (channel >> 31) & 0x1;
        auto Tp = (channel >> 30) & 0x1;
        auto ADC = (channel >> 20) & 0x3FF;
        auto TOT = (channel >> 10) & 0x3FF;
        auto TOA = channel & 0x3FF;

        // TOT Decoder
        // TOT is a 12 bit counter, but gets sent as a 10 bit number
        // If the most significant bit is 1, then the lower two bits were dropped
        if (TOT & 0x200) {
            // std::cout << "shifting tot" << std::endl;
            TOT = TOT & 0b0111111111;
            TOT = TOT << 3;
        } else {
            // std::cout << "not shifting tot" << std::endl;
        }
        // if (i == 0) {
        // if (ADC > 1) {
        //     std::cout << "Channel " << i << " Tc: " << Tc << " Tp: " << Tp << " ADC: " << ADC << " TOT: " << TOT << " TOA: " << TOA << std::endl;
        // }
        // std::cerr << "trying to fill channel " << i << std::endl;
        channels[fpga_id][asic_id][i + 36 * half_id]->construct_event(timestamp, ADC);
        channels[fpga_id][asic_id][i + 36 * half_id]->fill_readouts(ADC, TOT, TOA);
            // std::cout << "ADC: " << ADC << std::endl;
        // }
        // std::cout << channels[fpga_id][asic_id][half_id]->test << std::endl;
    }
}