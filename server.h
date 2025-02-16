#pragma once

#include "THttpServer.h"

class server {
private:
    server(); // Private constructor to prevent direct instantiation

    server(const server&) = delete; // Prevent copy constructor
    server& operator=(const server&) = delete; // Prevent copy assignment

    static server* instance; // Static pointer to the single instance
    THttpServer *s;

    

public:
    static server* get_instance() {
 // ======================================
 // This is critical section
        if (instance == nullptr) { // race condition can happen here.
            instance = new server(); // Create the instance on first call
        }
 // ======================================
        return instance;
    }

    THttpServer* get_server() {
        return s;
    }

    void kill_server() {
        delete s;
    }

};
