#pragma once

// Define the run configurations
const int NUM_FPGA = 4;
const int NUM_ASIC = 2;
const int NUM_CHANNELS = 80;
const int NUM_LINES = 5;

const int MAX_SAMPLES = 20;
const int MACHINE_GUN_MAX_TIME = 1000;  // TODO What is the actual timing supposed to be 
const int MAX_ADC = 1 << 10;
const int MAX_TOT = 1 << 10;
const int MAX_TOA = 1 << 10;

const int PACKET_SIZE = 1452;