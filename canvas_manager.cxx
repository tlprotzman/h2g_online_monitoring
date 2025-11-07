#include "canvas_manager.h"

#include <TCanvas.h>
#include <TSystem.h>
#include <TROOT.h>
#include <TFile.h>
#include <TH1.h>

#include <vector>
#include <memory>
#include <cstdint>
#include <iostream>

static canvas_manager instance;

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
            if (p && p->IsA() == TPad::Class()) {
                auto pad = (TPad*)p;
                pad->Modified();
                pad->Update();
            }
        }
    }
    gSystem->ProcessEvents();
}

void canvas_manager::save_all(int run_number, int time) {
    // Make the directory monitoring_plots if it does not exist
    gSystem->mkdir("monitoring_plots", kTRUE);
    gSystem->mkdir(Form("monitoring_plots/run_%03d", run_number), kTRUE);
    std::cout << "saving " << canvases.size() << " canvases" << std::endl;
    for (int i = 0; i < canvases.size(); i++) {
        canvases[i]->SaveAs(Form("monitoring_plots/run_%03d/run_%03d_%s_%d.pdf", run_number, run_number, canvases[i]->GetName(), time));
    }
}

void canvas_manager::clear_all() {
    for (auto c : canvases) {
        auto primatives = c->GetListOfPrimitives();
        for (auto p : *primatives) {
            if (p->InheritsFrom(TH1::Class())) {
            auto hist = (TH1*)p;
            hist->Reset();
            }
        }
    }
}
