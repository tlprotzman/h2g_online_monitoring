#include "file_stream.h"

#include "configuration.h"


file_stream::file_stream(const char *fname) {
    std::cout << "Attempting to open file " << fname << std::endl;
    file = std::ifstream(fname, std::ios::in | std::ios::binary);
    if (!file.good()) {
        std::cerr << "Error opening file" << std::endl;
        throw std::runtime_error("Error opening file");
    }
    // read file until newline is found
    char c;
    while (file.get(c) && c == '#') {
        while (file.get(c) && c != '\n');
    }
    file.seekg(-1, std::ios::cur);
    current_head = file.tellg();
    std::cout << "Starting at " << current_head << std::endl;
}

file_stream::~file_stream() {
    file.close();
    print_packet_numbers();
}

void file_stream::print_packet_numbers() {
    std::cout << "Total packets: " << total_packets << std::endl;
    std::cout << "Missed packets: " << missed_packets << std::endl;
    std::cout << "Missed packet rate: " << (double)missed_packets / total_packets << std::endl;
}

bool file_stream::read_packet(uint8_t *buffer) {
    // Check if PACKET_SIZE bytes are available to read
    // std::cout << "trying to read from " << file.tellg() << std::endl;
    file.seekg(0, std::ios::end);
    if (file.tellg() - current_head < PACKET_SIZE) {
        // std::cerr << "Not enough bytes available to read line" << std::endl;
        file.seekg(current_head, std::ios::beg);
        return false;
    }
    file.seekg(current_head, std::ios::beg);
    // file.seekg(-1 * PACKET_SIZE, std::ios::cur);
    // std::cout << "Reading from " << current_head << std::endl;
    file.read(reinterpret_cast<char*>(buffer), PACKET_SIZE);;
    current_head = file.tellg();
    if (file.rdstate() & std::ifstream::failbit || file.rdstate() & std::ifstream::badbit) {
	if (std::ifstream::failbit) {
            std::cerr << "Error reading line - failbit" << std::endl;
	}
	else if (std::ifstream::badbit) {
            std::cerr << "Error reading line - badbit" << std::endl;
	}
	else if (std::ifstream::eofbit) {
            std::cerr << "Error reading line - eofbit" << std::endl;
	}

	perror("bad read");
        return false;
    }
    uint16_t packet_number = (buffer[2] << 8) + (buffer[3]);
    if ((packet_number != (uint16_t)(current_packet + 1)) && !first_packet) {
        total_packets += packet_number - current_packet - 1;
        missed_packets ++;
    } else if (first_packet) {
        current_packet = packet_number;
        first_packet = false;
    }
    total_packets++;
    current_packet = packet_number;
    return true;
}
