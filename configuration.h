#pragma once

#include <string>

class configuration {
private:
    configuration() {};
    configuration(const configuration&) = delete;
    configuration& operator=(const configuration&) = delete;

    static configuration *instance;

public:
    static configuration* get_instance() {
        if (instance == nullptr) {
            instance = new configuration();
        }
        return instance;
    }

    int NUM_FPGA = 4;
    int NUM_ASIC = 2;
    int NUM_CHANNELS = 72;
    int NUM_LINES = 5;

    int MAX_SAMPLES = 20;
    int MACHINE_GUN_MAX_TIME = 1000;  // TODO What is the actual timing supposed to be 
    int MAX_ADC = 1 << 10;
    int MAX_TOT = 1 << 12;
    int MAX_TOA = 1 << 10;

    int PACKET_SIZE = 1452;

};


void load_configs(std::string config_file);
void print_configs();