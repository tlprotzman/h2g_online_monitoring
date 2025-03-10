#include "server.h"

server* server::instance = nullptr;

server::server() {
    auto port = getenv("MONITORING_PORT");
    if (port == nullptr) {
        port = new char[6];
        strcpy(port, "12345");
    }
    s = new THttpServer(Form("http:%s;rw;noglobal", port));
    s->SetItemField("/", "_monitoring", "1000");
    s->SetItemField("/", "_toptitle", "EEEMCal Online Monitor"); 
}
