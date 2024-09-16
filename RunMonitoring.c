#include "Monitor.h"
#include <TSystem.h>
#include <THttpServer.h>

void RunMonitoring(int run = 4) {
	  gSystem->Load("libRHTTP.so");
    gSystem->Load("libMonitoring.so");
    Monitor(run, "lfhcal_12sample.cfg");
}
