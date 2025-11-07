#include "configuration.h"

#include <iostream>
#include <fstream>
#include <string>

void load_configs(std::string config_file, int run) {
    std::cout << "Parsing config file " << config_file << std::endl;
    auto config = configuration::get_instance();
    std::ifstream file(config_file);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            // Process each line of the config file
            std::string key, value;
            std::size_t delimiter_pos = line.find('=');
            if (delimiter_pos != std::string::npos) {
                key = line.substr(0, delimiter_pos);
                value = line.substr(delimiter_pos + 1);
                // Do something with the key-value pair
                std::cout << "Key: " << key << ", Value: " << value << std::endl;
                if (key == "NUM_LINES") {
                    config->NUM_LINES = std::stoi(value);
                } else if (key == "NUM_FPGA") {
                    config->NUM_FPGA = std::stoi(value);
                } else if (key == "NUM_CHANNELS") {
                    config->NUM_CHANNELS = std::stoi(value);
                } else if (key == "NUM_LINES") {
                    config->NUM_LINES = std::stoi(value);
                } else if (key == "MAX_SAMPLES") {
                    config->MAX_SAMPLES = std::stoi(value);
                } else if (key == "MACHINE_GUN_MAX_TIME") {
                    config->MACHINE_GUN_MAX_TIME = std::stoi(value);
                } else if (key == "PACKET_SIZE") {
                config->PACKET_SIZE = std::stoi(value);
                } else if (key == "DETECTOR_ID") {
                    config->DETECTOR_ID = std::stoi(value);
                }
                else {
                    std::cerr << "Unknown key: " << key << std::endl;
                }
            }
        }
        file.close();
    } else {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
    }

    std::cout << "MADE IT HERE" << std::endl;

    // Open the run and parse a few items out of there
    std::string run_str = std::to_string(run);
    if (run_str.length() < 3) run_str = std::string(3 - run_str.length(), '0') + run_str;
    const char* data_dir_env = std::getenv("DATA_DIRECTORY");
    std::string run_file = "Run" + run_str + ".h2g";
    if (data_dir_env && data_dir_env[0] != '\0') {
        std::string dir(data_dir_env);
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir.push_back('/');
        run_file = dir + run_file;
    }

    std::ifstream runfile(run_file);
    bool is_jumbo = false;
    if (runfile.is_open()) {
        // std::cout << "opened run file: " << run_file << std::endl;
        std::string line;
        // Skip two lines
        std::getline(runfile, line);
        std::getline(runfile, line);
        while (std::getline(runfile, line)) {
            // std::cout << "processing line: " << line << std::endl;
            if (line == "##################################################") break;
            // Look for the file version
            std::string key, value;
            std::size_t delimiter_pos = line.find(':');
            if (delimiter_pos != std::string::npos) {
                key = line.substr(0, delimiter_pos);
                value = line.substr(delimiter_pos + 1);
                // Do something with the key-value pair
                if (key == "# File Version") {
                    std::cout << "Run file version: " << value << std::endl;
                    std::size_t dot_pos = value.find('.');
                    if (dot_pos != std::string::npos) {
                        config->FILE_VERSION_MAJOR = std::stoi(value.substr(0, dot_pos));
                        config->FILE_VERSION_MINOR = std::stoi(value.substr(dot_pos + 1));
                    }
                }
                if (key == "# Generator Setting machine_gun") {
                    config->MAX_SAMPLES = std::stoi(value);
                    std::cout << "Setting MAX_SAMPLES to " << config->MAX_SAMPLES << std::endl;
                }
                if (key == "# Generator Setting jumbo_enable") {
                    // 0 if off, 1 if on
                    is_jumbo = std::stoi(value);
                    if (is_jumbo) {
                        config->PACKET_SIZE = 8846;
                    } else {
                        config->PACKET_SIZE = 1358;
                    }
                    std::cout << "Setting PACKET_SIZE to " << config->PACKET_SIZE << std::endl;
                }
            }
        }
        runfile.close();
    } else {
        std::cerr << "Failed to open run file: " << run_file << std::endl;
    }

    std::cout << "\n\n";
    print_configs();
    std::cout << "\n\n";
}

void print_configs() {
    auto config = configuration::get_instance();
    // Print the configuration values
    std::cout << "Configuring for detector ID " << config->DETECTOR_ID << ": ";
    switch (config->DETECTOR_ID) {
        case 0: 
            std::cout << "Generic" << std::endl;
            break;
        case 1:
            std::cout << "LFHCal" << std::endl;
            break;
        case 2:
            std::cout << "EEEMCal" << std::endl;
            break;
        case 3:
            std::cout << "FOCAL" << std::endl;
            break;
        default:
            std::cout << "Unknown" << std::endl;
            throw std::runtime_error("Unknown detector ID");
            break;
    }

    std::cout << "Configuration values:" << std::endl;
    std::cout << "NUM_LINES: " << config->NUM_LINES << std::endl;
    std::cout << "NUM_FPGA: " << config->NUM_FPGA << std::endl;
    std::cout << "NUM_CHANNELS: " << config->NUM_CHANNELS << std::endl;
    std::cout << "NUM_LINES: " << config->NUM_LINES << std::endl;
    std::cout << "MAX_SAMPLES: " << config->MAX_SAMPLES << std::endl;
    std::cout << "MACHINE_GUN_MAX_TIME: " << config->MACHINE_GUN_MAX_TIME << std::endl;
    

    std::cout << "PACKET_SIZE: " << config->PACKET_SIZE << std::endl;

}

configuration *configuration::instance = nullptr;
