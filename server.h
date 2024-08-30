#ifndef SERVER_H
#define SERVER_H

#include "THttpServer.h"

class server {
private:
    server() {s = new THttpServer("http:12345");} // Private constructor to prevent direct instantiation

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

};

/*

even though I've defined the s.tatic member variable outside the class, 
it remains inaccessible directly because it's still declared as private 
within the class. The purpose of defining it outside the class is to 
allocate memory for it, but its access is still controlled by the class's 
access specifiers.
*/

#endif
