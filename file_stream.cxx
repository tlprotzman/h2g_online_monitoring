#include "file_stream.h"

#include "configuration.h"
#include "canvas_manager.h"
#include "server.h"

#include <TGraph.h>
#include <TMultiGraph.h>
#include <TCanvas.h>
#include <TAxis.h>
#include <TROOT.h>
#include <TDatime.h>
#include <TLegend.h>


file_stream::file_stream(const char *fname) {
    auto config = configuration::get_instance();    
    current_packet = new uint16_t[config->NUM_FPGA];
    missed_packets = new uint32_t[config->NUM_FPGA];
    total_packets = new uint32_t[config->NUM_FPGA];
    canvas_id = new int[config->NUM_FPGA];
    first_packet = new bool[config->NUM_FPGA];
    recieved_packet_graphs = new TGraph*[config->NUM_FPGA];
    missed_packet_graphs = new TGraph*[config->NUM_FPGA];
    missed_packet_graphs_percent = new TGraph*[config->NUM_FPGA];
    mg = new TMultiGraph*[config->NUM_FPGA];



    auto canvases = canvas_manager::get_instance();
    auto s = server::get_instance()->get_server();
    for (int i = 0; i < config->NUM_FPGA; i++) {
        current_packet[i] = 0;
        missed_packets[i] = 0;
        total_packets[i] = 0;
        
        canvas_id[i] = canvases.new_canvas(Form("FPGA_%i_Packets", i), Form("FPGA %i Packets", i), 1200, 800);
        s->Register("/QA Plots/DAQ Performance", canvases.get_canvas(canvas_id[i]));
        TLegend *legend = new TLegend(0.15, 0.75, 0.48, 0.9);
        legend->SetBorderSize(0);
        
        recieved_packet_graphs[i] = new TGraph();
        recieved_packet_graphs[i]->SetName(Form("fpga_%i_packets", i));
        gROOT->Add(recieved_packet_graphs[i]);
        recieved_packet_graphs[i]->SetTitle(Form("FPGA %i Packets", i));
        recieved_packet_graphs[i]->GetXaxis()->SetTitle("Time");
        recieved_packet_graphs[i]->GetXaxis()->SetTimeDisplay(1);
        recieved_packet_graphs[i]->GetXaxis()->SetTimeFormat("%H:%M:%S");
        recieved_packet_graphs[i]->GetYaxis()->SetTitle("Packets");
        recieved_packet_graphs[i]->SetLineColor(kBlue);
        recieved_packet_graphs[i]->SetLineWidth(2);
        recieved_packet_graphs[i]->Draw("AL");
        legend->AddEntry(recieved_packet_graphs[i], "Recieved Packets", "l");
        first_packet[i] = true;

        missed_packet_graphs[i] = new TGraph();
        missed_packet_graphs[i]->SetName(Form("fpga_%i_missed_packets", i));
        gROOT->Add(missed_packet_graphs[i]);
        missed_packet_graphs[i]->SetTitle(Form("FPGA %i Missed Packets", i));
        missed_packet_graphs[i]->GetXaxis()->SetTitle("Time");
        missed_packet_graphs[i]->GetXaxis()->SetTimeDisplay(1);
        missed_packet_graphs[i]->GetXaxis()->SetTimeFormat("%H:%M:%S");
        missed_packet_graphs[i]->GetYaxis()->SetTitle("Missed Packets");
        missed_packet_graphs[i]->SetLineColor(kRed);
        missed_packet_graphs[i]->SetLineWidth(2);
        legend->AddEntry(missed_packet_graphs[i], "Missed Packets", "l");
        missed_packet_graphs[i]->Draw("L");

        legend->Draw();

        missed_packet_graphs_percent[i] = new TGraph();
        missed_packet_graphs_percent[i]->SetName(Form("fpga_%i_missed_packets_percent", i));
        gROOT->Add(missed_packet_graphs_percent[i]);
        missed_packet_graphs_percent[i]->SetTitle(Form("FPGA %i Missed Packets Percent", i));
        missed_packet_graphs_percent[i]->GetXaxis()->SetTitle("Time");
        missed_packet_graphs_percent[i]->GetXaxis()->SetTimeDisplay(1);
        missed_packet_graphs_percent[i]->GetXaxis()->SetTimeFormat("%H:%M:%S");
        missed_packet_graphs_percent[i]->GetYaxis()->SetTitle("Missed Packets Percent");
        missed_packet_graphs_percent[i]->SetLineColor(kGreen+2);
        missed_packet_graphs_percent[i]->SetLineWidth(2);
        legend->AddEntry(missed_packet_graphs_percent[i], "Missed Packets Percent", "l");
        missed_packet_graphs_percent[i]->Draw("LY+");
    }

    std::cout << "Attempting to open file " << fname << std::endl;
    file = std::ifstream(fname, std::ios::in | std::ios::binary);
    if (!file.good()) {
        std::cerr << "Error opening file" << std::endl;
        throw std::runtime_error("Error opening file");
    }
    // read file until newline is found
    char c;
    // skip the first 21 lines... sigh.  this should be better
    for (int i = 0; i < 21; i++) {
        while (file.get(c) && c != '\n');
    }

    // while (file.get(c) && c == '#') {
    //     while (file.get(c) && c != '\n');
    // }
    // file.seekg(-1, std::ios::cur);
    current_head = file.tellg();
    std::cout << "Starting at byte " << current_head << std::endl;
}

file_stream::~file_stream() {
    file.close();
    print_packet_numbers();
    for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
        delete recieved_packet_graphs[i];
        delete missed_packet_graphs[i];
        delete missed_packet_graphs_percent[i];
        delete mg[i];
    }
    delete[] current_packet;
    delete[] missed_packets;
    delete[] total_packets;
    delete[] canvas_id;
    delete[] first_packet;
    delete[] recieved_packet_graphs;
    delete[] missed_packet_graphs;
    delete[] missed_packet_graphs_percent;
    delete[] mg;
}

void file_stream::print_packet_numbers() {
    for (int i = 0; i < configuration::get_instance()->NUM_FPGA; i++) {
        auto time = TDatime();
        recieved_packet_graphs[i]->SetPoint(recieved_packet_graphs[i]->GetN(), time.Convert(), total_packets[i]);
        recieved_packet_graphs[i]->GetYaxis()->SetRangeUser(0, 1.2 * (float)total_packets[i]);
        missed_packet_graphs[i]->SetPoint(missed_packet_graphs[i]->GetN(), time.Convert(), missed_packets[i]);
        // missed_packet_graphs[i]->GetYaxis()->SetRangeUser(0, 1.2 * (float)missed_packets[i]);
        // missed_packet_graphs[i]->GetYaxis()->SetRangeUser(0, 1.2 * (float)total_packets[i]);
        missed_packet_graphs_percent[i]->SetPoint(missed_packet_graphs_percent[i]->GetN(), time.Convert(), (double)missed_packets[i] / total_packets[i]);
        missed_packet_graphs_percent[i]->GetYaxis()->SetRangeUser(0, 1);
    }
}

int file_stream::read_packet(uint8_t *buffer) {
    auto config = configuration::get_instance();
    // Check if PACKET_SIZE bytes are available to read
    file.seekg(0, std::ios::end);
    if (file.tellg() - current_head < config->PACKET_SIZE) {
        file.seekg(current_head, std::ios::beg);
        return 0;
    }
    file.seekg(current_head, std::ios::beg);
    // file.seekg(-1 * PACKET_SIZE, std::ios::cur);
    file.read(reinterpret_cast<char*>(buffer), config->PACKET_SIZE);;
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
        return 0;
    }
    // Check if this is a heartbeat packet
    if (buffer[0] == 0x23 && buffer[1] == 0x23 && buffer[2] == 0x23 && buffer[3] == 0x23) {
        return 2;
    }
    uint16_t packet_number = (buffer[2] << 8) + (buffer[3]);
    uint32_t fpga_id = buffer[13];
    if ((packet_number != (uint16_t)(current_packet[fpga_id] + 1)) && !first_packet[fpga_id]) {
        total_packets[fpga_id] += packet_number - current_packet[fpga_id] - 1;
        missed_packets[fpga_id] ++;
    } else if (first_packet) {
        current_packet[fpga_id] = packet_number;
        first_packet[fpga_id] = false;
    }
    total_packets[fpga_id]++;
    current_packet[fpga_id] = packet_number;
    return 1;
}
