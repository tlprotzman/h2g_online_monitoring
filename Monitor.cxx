/*
LFHCal Test Beam Online Monitoring
Tristan Protzman, 27-08-2024
*/

#include "Monitor.h"

#include "file_stream.h"
#include "line_stream.h"
#include "channel_stream.h"
#include "configuration.h"
#include "canvas_manager.h"
#include "server.h"
#include "event_builder.h"
#include "online_monitor.h"
#include "decoders.h"

#include <TROOT.h>
#include <TH1.h>
#include <TH2.h>
#include <TH2D.h>
#include <TH3.h>
#include <TH3D.h>
#include <TRint.h>
#include <TApplication.h>
#include <TSystem.h>
#include <TStyle.h>
#include <TLatex.h>
#include <TFile.h>
#include <THttpServer.h>

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <csignal>

// catch ctrl-c
bool stop = false;
void signal_handler(int signal) {
    stop = true;
}

void test_reading(int run) {
    // auto server = canvases->get_server();
    auto s = server::get_instance()->get_server();
    auto start_time = std::chrono::high_resolution_clock::now();
    auto m = new online_monitor(run);
    auto line_numbers = new TH1I("line_numbers", "Line Numbers", 5, 0, 5);
    // const char *fname = "run051.txt";

    // Some histograms to check the data rates from each board/asic
    auto data_rates = new TH1D("lines_received", "Lines Received;;#", 16, 0, 16);
    const char *readout_label[16]  = {"F0A0H0","F0A0H1","F0A1H0","F0A1H1",
                                      "F1A0H0","F1A0H1","F1A1H0","F1A1H1",
                                      "F2A0H0","F2A0H1","F2A1H0","F2A1H1",
                                      "F3A0H0","F3A0H1","F3A1H0","F3A1H1"};
    for (int i = 0; i < 16; i++) {
        data_rates->GetXaxis()->SetBinLabel(i + 1, readout_label[i]);
    }
    // data_rates->SetMinimum(0);
    s->Register("/qa_plots/daq/", data_rates);

    auto dir = getenv("DATA_DIRECTORY");
    if (dir == nullptr) {
        std::cerr << "DATA_DIRECTORY not set!  Defaulting to current directory." << std::endl;
        dir = new char[2];
        strcpy(dir, ".");
        return;
    }

    const char *fname = Form("%s/Run%03d.h2g", dir, run);
    // const char *fname = "PhaseScanChannel16Phase2.txt";
    file_stream fs(fname);
    uint8_t buffer[configuration::get_instance()->PACKET_SIZE];
    uint32_t heartbeat_seconds = 0;
    uint32_t heartbeat_milliseconds = 0;
    // for (int iteration = 0; iteration < 50; iteration++) {
    // for (int iteration = 0; iteration < 61100; iteration++) {
    bool all_events_built = false;
    for (int iteration = 0; iteration < 10000000; iteration++) {
        if (stop) {
            break;
        }
        s->ProcessRequests();
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time).count() > 4) {
            std::cout << "Building events...";
            m->build_events();
            std::cout << " done!" << std::endl;
            std::cout << "Updating canvases...";
            fs.print_packet_numbers();
            m->update_builder_graphs();
            m->update_canvases();
            gSystem->ProcessEvents();
            start_time = std::chrono::high_resolution_clock::now();
            // m->make_event_display();
            std::cout << " done!" << std::endl;
            all_events_built = true;
        }
        int good_data = fs.read_packet(buffer);
        if (!good_data) {
            if (all_events_built) {
                std::cout << "All events built, exiting..." << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        all_events_built = false;
        if (good_data == 2) {
            // std::cout << "Heartbeat packet" << std::endl;
            heartbeat_seconds = bit_converter(buffer, 12, false);
            heartbeat_milliseconds = bit_converter(buffer, 16, false);
            // std::cout << "Heartbeat: " << heartbeat_seconds << "." << heartbeat_milliseconds << std::endl;
            continue;
        }
        std::vector<line> lines(36);    // 36 lines per packet
        decode_packet(lines, buffer);
        process_lines(lines, m->line_streams, data_rates);
        for (auto line : lines) {
            line_numbers->Fill(line.line_number);
        }
        m->update_events();

        // for (int i = 0; i < PACKET_SIZE; i++) {
        //     std::cout << setfill('0') << setw(2) << std::hex << (int)buffer[i] << " ";
        // }
        // std::cout << std::endl << std::endl << std::endl;;
    }
    delete m;
    std::cout << "Exiting cleanly, just for you Fredi!" << std::endl;
}

int Monitor(int run, std::string config_file) {
    // register signal handler
    signal(SIGINT, signal_handler);

    gStyle->SetOptStat(0);
    // 
    // test_decoding();
    load_configs(config_file);
    print_configs();
    test_reading(run);
    return 0;
}

int main() {
    TApplication app("app", 0, nullptr);
    Monitor(42, "");  // answer to the universe
    return 0;
}
