#include "Monitor.h"
#include <TSystem.h>
#include <THttpServer.h>

void RunMonitoring(int run = 1, int debug = 0, bool isPostAna = false) {
	  gSystem->Load("libRHTTP.so");
    gSystem->Load("libMonitoring.so");
    // Monitor(run, "generic_20sample.cfg");
    // Monitor(run, "eeemcal_20sample.cfg");
    Monitor(run, "lfhcal_10sample.cfg", debug, isPostAna);
}
