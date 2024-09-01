#pragma once

#include <TCanvas.h>
#include <TSystem.h>
#include <THttpServer.h>

#include <cstdint>
#include <memory>
#include <vector>
#include <iostream>


class canvas_manager {
public:
    static canvas_manager &get_instance() {
        return instance;
    }
private:
    std::vector<TCanvas*> canvases;
    static canvas_manager instance;

public:
    canvas_manager() {};
    canvas_manager(const canvas_manager&) {};
    ~canvas_manager() {};
    void operator=(const canvas_manager&) {};

    uint32_t new_canvas(const char *name, const char *title, int width, int height);
    TCanvas* get_canvas(uint32_t index) { return canvases[index]; }
    void delete_canvas(uint32_t index) { canvases.erase(canvases.begin() + index); }
    void update();
    void save_all(int run_number, int time);
};
