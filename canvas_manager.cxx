#include "canvas_manager.h"

#include <TCanvas.h>
#include <TSystem.h>

#include <vector>
#include <memory>
#include <cstdint>
#include <iostream>

uint32_t canvas_manager::new_canvas(const char *name, const char *title, int width, int height) {
    auto c = new TCanvas(name, title, width, height);
    canvases.push_back(c);
    return canvases.size() - 1;
}



void canvas_manager::update() {
    for (auto c : canvases) {
            c->Update();
            auto primatives = c->GetListOfPrimitives();
            for (auto p : *primatives) {
                if (p->IsA() == TPad::Class()) {
                    auto pad = (TPad*)p;
                    pad->Modified();
                    pad->Update();
                }
            }
    }
    gSystem->ProcessEvents();
}
