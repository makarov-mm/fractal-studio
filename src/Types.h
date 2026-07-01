#pragma once
#include <cstdint>

// Plain-old-data types shared between the Qt/host code and the CUDA device code.
// This header must stay free of any Qt includes so it can be compiled by nvcc.

enum class FractalType : int {
    Mandelbrot = 0,
    Julia      = 1
};

struct FractalParams {
    int    width     = 800;
    int    height    = 600;
    double centerX   = -0.5;   // center of the view in the complex plane
    double centerY   = 0.0;
    double spanX     = 3.5;    // horizontal extent of the view (complex units)
    int    maxIter   = 500;

    FractalType type = FractalType::Mandelbrot;
    double juliaCX   = -0.8;   // Julia set constant (used when type == Julia)
    double juliaCY   = 0.156;

    int    colorScheme = 0;    // 0 = electric, 1 = fire, 2 = ocean
};
