// CudaFractal.cu
// GPU back-end: one thread per pixel computes the escape-time fractal and
// writes the final color directly, so the device-to-host transfer is just the
// finished image. Uses the same coloring as the CPU path (ColorMap.h) to keep
// the outputs pixel-identical.

#include "CudaFractal.h"
#include "ColorMap.h"
#include <cuda_runtime.h>
#include <cstdio>

// Persistent device buffer, grown on demand so we don't cudaMalloc every frame.
static uint32_t* g_devPixels  = nullptr;
static int       g_devCapacity = 0;

__global__ void fractalKernel(uint32_t* out, int width, int height,
                              double centerX, double centerY, double scale,
                              int maxIter, int type,
                              double juliaCX, double juliaCY, int scheme) {
    int px = blockIdx.x * blockDim.x + threadIdx.x;
    int py = blockIdx.y * blockDim.y + threadIdx.y;
    if (px >= width || py >= height) return;

    // Map the pixel to a point in the complex plane.
    float x0 = (float)(centerX + (px - width  * 0.5) * scale);
    float y0 = (float)(centerY + (py - height * 0.5) * scale);

    float zx, zy, cx, cy;
    if (type == 0) { zx = 0.0f; zy = 0.0f; cx = x0;            cy = y0;            } // Mandelbrot
    else           { zx = x0;   zy = y0;   cx = (float)juliaCX; cy = (float)juliaCY; } // Julia

    const float BAILOUT = 65536.0f;   // large radius -> smoother coloring
    int   iter = 0;
    float mag2 = 0.0f;
    while (iter < maxIter) {
        float nx = zx * zx - zy * zy + cx;
        float ny = 2.0f * zx * zy + cy;
        zx = nx; zy = ny;
        mag2 = zx * zx + zy * zy;
        if (mag2 > BAILOUT) break;
        ++iter;
    }

    uint32_t color;
    if (iter >= maxIter) {
        color = 0xFF000000u;                         // inside the set -> black
    } else {
        float mu = fr_smoothValue(iter, zx, zy);
        float t  = mu * 0.02f; t -= floorf(t);       // cyclic banding
        color = fr_colorize(t, scheme);
    }
    out[py * width + px] = color;
}

bool cudaAvailable() {
    int n = 0;
    if (cudaGetDeviceCount(&n) != cudaSuccess) return false;
    return n > 0;
}

void cudaRenderFractal(const FractalParams& p, uint32_t* pixels, double* elapsedMs) {
    int    count = p.width * p.height;
    size_t bytes = (size_t)count * sizeof(uint32_t);

    if (count > g_devCapacity) {
        if (g_devPixels) cudaFree(g_devPixels);
        cudaMalloc(&g_devPixels, bytes);
        g_devCapacity = count;
    }

    double scale = p.spanX / p.width;

    dim3 block(16, 16);
    dim3 grid((p.width  + block.x - 1) / block.x,
              (p.height + block.y - 1) / block.y);

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    // Bracket both the kernel and the device-to-host copy: the reported time is
    // the honest "time to obtain a displayable image on the host".
    cudaEventRecord(start);
    fractalKernel<<<grid, block>>>(g_devPixels, p.width, p.height,
                                   p.centerX, p.centerY, scale,
                                   p.maxIter, (int)p.type,
                                   p.juliaCX, p.juliaCY, p.colorScheme);
    cudaMemcpy(pixels, g_devPixels, bytes, cudaMemcpyDeviceToHost);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float ms = 0.0f;
    cudaEventElapsedTime(&ms, start, stop);
    *elapsedMs = (double)ms;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
}

void cudaShutdown() {
    if (g_devPixels) {
        cudaFree(g_devPixels);
        g_devPixels = nullptr;
        g_devCapacity = 0;
    }
}
