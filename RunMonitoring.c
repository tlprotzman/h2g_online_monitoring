#include "Monitor.h"
#include <TSystem.h>
#include <THttpServer.h>

void RunMonitoring(int run = 4) {
	  gSystem->Load("libRHTTP.so");
    gSystem->Load("libMonitoring.so");
    Monitor(run, "generic_20sample.cfg");
    // Monitor(run, "lfhcal_10sample.cfg");
}
