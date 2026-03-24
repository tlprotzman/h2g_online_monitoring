#include "Monitor.h"
#include <TSystem.h>
#include <THttpServer.h>

void RunMonitoring(int run = 1, TString configName="lfhcal_10sample_testORNLSumV1_test.cfg", int debug = 0, bool isPostAna = false) {
	  gSystem->Load("libRHTTP.so");
    gSystem->Load("libMonitoring.so");
    // Monitor(run, "generic_20sample.cfg");
    // Monitor(run, "eeemcal_20sample.cfg");
    Monitor(run, configName.Data(), debug, isPostAna);
    // Monitor(run, "lfhcal_10sample.cfg", debug, isPostAna);
}
