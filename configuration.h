#pragma once

// Define the run configurations
const int NUM_FPGA = 4;
const int NUM_ASIC = 2;
const int NUM_CHANNELS = 80;
const int NUM_LINES = 5;

const int MAX_SAMPLES = 11;
const int MAX_ADC = 1 << 10;
const int MAX_TOT = 1 << 10;
const int MAX_TOA = 1 << 10;

const int PACKET_SIZE = 1452;