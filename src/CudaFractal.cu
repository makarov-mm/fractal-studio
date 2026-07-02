// CudaFractal.cu
// GPU back-end: one thread per (sub)pixel computes the escape-time fractal and
// writes the final color directly. The per-pixel math lives in FractalMath.h /
// ColorMap.h and is shared verbatim with the CPU path, so the outputs are
// pixel-identical. The kernel is templated on the floating-point type: the
// float instantiation is fast, the double instantiation demonstrates how
// heavily FP64 is throttled on consumer GPUs (typically 1:32 - 1:64).

#include "CudaFractal.h"
#include "FractalMath.h"
#include "ColorMap.h"
#include <cuda_runtime.h>
#include <cstdio>

// Persistent device buffers, grown on demand so we don't cudaMalloc every frame.
static uint32_t* g_devHi       = nullptr;   // supersampled render target
static size_t    g_devHiCap    = 0;
static uint32_t* g_devFinal    = nullptr;   // down-sampled final image
static size_t    g_devFinalCap = 0;

static void ensureCapacity(uint32_t** buf, size_t* cap, size_t count) {
    if (count > *cap) {
        if (*buf) cudaFree(*buf);
        cudaMalloc(buf, count * sizeof(uint32_t));
        *cap = count;
    }
}

template <typename T>
__global__ void fractalKernel(uint32_t* out, int width, int height,
                              double centerX, double centerY, double scale,
                              int maxIter, int type, int power,
                              double juliaCX, double juliaCY,
                              int scheme, float density, float offset) {
    int px = blockIdx.x * blockDim.x + threadIdx.x;
    int py = blockIdx.y * blockDim.y + threadIdx.y;
    if (px >= width || py >= height) return;

    // Map the (sub)pixel to a point in the complex plane.
    T x0 = (T)(centerX + (px - width  * 0.5) * scale);
    T y0 = (T)(centerY + (py - height * 0.5) * scale);

    T zx, zy, cx, cy;
    if (type == (int)FractalType::Julia) {
        zx = x0; zy = y0; cx = (T)juliaCX; cy = (T)juliaCY;
    } else {
        zx = (T)0; zy = (T)0; cx = x0; cy = y0;
    }

    int iter = fr_iterate<T>(type, power, maxIter, cx, cy, zx, zy);
    int pw   = fr_effectivePower(type, power);
    out[py * width + px] = fr_shade(iter, maxIter, (float)zx, (float)zy, pw,
                                    scheme, density, offset);
}

// Box-filter the supersampled buffer down to display resolution on the device,
// so the PCIe transfer stays at 1x regardless of the supersampling factor.
__global__ void downsampleKernel(const uint32_t* hi, uint32_t* out,
                                 int outWidth, int outHeight, int ss) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= outWidth || y >= outHeight) return;
    out[y * outWidth + x] =
        fr_averageBlock(hi, outWidth * ss, x * ss, y * ss, ss);
}

bool cudaAvailable() {
    int n = 0;
    if (cudaGetDeviceCount(&n) != cudaSuccess) return false;
    return n > 0;
}

void cudaRenderFractal(const FractalParams& p, uint32_t* pixels, double* elapsedMs) {
    const int ss  = p.supersample;
    const int hw  = p.width  * ss;
    const int hh  = p.height * ss;
    size_t hiCount    = (size_t)hw * hh;
    size_t finalCount = (size_t)p.width * p.height;
    size_t finalBytes = finalCount * sizeof(uint32_t);

    ensureCapacity(&g_devHi, &g_devHiCap, hiCount);
    uint32_t* renderTarget = g_devHi;
    if (ss > 1)
        ensureCapacity(&g_devFinal, &g_devFinalCap, finalCount);
    uint32_t* finalBuf = (ss > 1) ? g_devFinal : g_devHi;

    double scale = p.spanX / hw;

    dim3 block(16, 16);
    dim3 grid((hw + block.x - 1) / block.x,
              (hh + block.y - 1) / block.y);

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    // Bracket the kernels and the device-to-host copy: the reported time is
    // the honest "time to obtain a displayable image on the host".
    cudaEventRecord(start);

    if (p.doublePrecision)
        fractalKernel<double><<<grid, block>>>(renderTarget, hw, hh,
            p.centerX, p.centerY, scale, p.maxIter, (int)p.type, p.power,
            p.juliaCX, p.juliaCY, p.colorScheme, p.colorDensity, p.colorOffset);
    else
        fractalKernel<float><<<grid, block>>>(renderTarget, hw, hh,
            p.centerX, p.centerY, scale, p.maxIter, (int)p.type, p.power,
            p.juliaCX, p.juliaCY, p.colorScheme, p.colorDensity, p.colorOffset);

    if (ss > 1) {
        dim3 dgrid((p.width  + block.x - 1) / block.x,
                   (p.height + block.y - 1) / block.y);
        downsampleKernel<<<dgrid, block>>>(g_devHi, g_devFinal,
                                           p.width, p.height, ss);
    }

    cudaMemcpy(pixels, finalBuf, finalBytes, cudaMemcpyDeviceToHost);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float ms = 0.0f;
    cudaEventElapsedTime(&ms, start, stop);
    *elapsedMs = (double)ms;

    cudaEventDestroy(start);
    cudaEventDestroy(stop);
}

void cudaShutdown() {
    if (g_devHi)    { cudaFree(g_devHi);    g_devHi    = nullptr; g_devHiCap    = 0; }
    if (g_devFinal) { cudaFree(g_devFinal); g_devFinal = nullptr; g_devFinalCap = 0; }
}
