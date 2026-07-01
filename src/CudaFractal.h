#pragma once
#include "Types.h"
#include <cstdint>

// Host-side interface to the CUDA back-end. Implemented in CudaFractal.cu.
// Kept Qt-free so it can be included from both host and device translation units.

// Returns true if at least one CUDA-capable device is present.
bool cudaAvailable();

// Renders the fractal on the GPU into the host buffer `pixels`
// (must hold p.width * p.height uint32_t values, format 0xAARRGGBB).
// The elapsed GPU time (kernel + device-to-host copy) in milliseconds is
// written to *elapsedMs.
void cudaRenderFractal(const FractalParams& p, uint32_t* pixels, double* elapsedMs);

// Releases the persistent device buffer (optional; called at shutdown).
void cudaShutdown();
