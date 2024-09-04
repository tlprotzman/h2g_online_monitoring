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
    int MACHINE_GUN_MAX_TIME = 600;
    int MAX_ADC = 1 << 10;
    int MAX_TOT = 1 << 12;
    int MAX_TOA = 1 << 10;

    // Detector ID
    // 1: LFHCal
    // 2: EEEMCal
    // 3: FOCAL
    int DETECTOR_ID = 0;

    int PACKET_SIZE = 1452;

    int EVENT_ALIGNMENT_TOLERANCE = 4;

};


void load_configs(std::string config_file);
void print_configs();
