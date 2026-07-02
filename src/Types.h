#pragma once
#include <cstdint>

// Plain-old-data types shared between the Qt/host code and the CUDA device code.
// This header must stay free of any Qt includes so it can be compiled by nvcc.

enum class FractalType : int {
    Mandelbrot  = 0,
    Julia       = 1,
    BurningShip = 2,
    Tricorn     = 3,
    Multibrot   = 4
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
    int    power     = 3;      // exponent for Multibrot (z^power + c)

    int    colorScheme  = 0;    // index into the palette list (ColorMap.h)
    float  colorDensity = 1.0f; // stretches / compresses the palette cycle
    float  colorOffset  = 0.0f; // rotates the palette, [0, 1)

    bool   doublePrecision = false; // float is fast; double allows deep zoom
    int    supersample     = 1;     // 1 = off, 2 = 2x2 ordered supersampling
};
