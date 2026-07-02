#include "FractalRenderer.h"
#include "FractalMath.h"
#include "ColorMap.h"

#include <chrono>
#include <thread>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cmath>

#ifdef HAVE_CUDA
#include "CudaFractal.h"
#endif

namespace {

// Compute a horizontal band of rows [yStart, yEnd) on the CPU into a buffer
// of size (p.width * ss) x (p.height * ss). Same math and coloring as the
// CUDA kernel (FractalMath.h / ColorMap.h), so output matches pixel for pixel.
template <typename T>
void computeRows(const FractalParams& p, uint32_t* out, int yStart, int yEnd) {
    const int    ss     = p.supersample;
    const int    width  = p.width  * ss;
    const int    height = p.height * ss;
    const double scale  = p.spanX / width;
    const int    type   = (int)p.type;
    const int    pw     = fr_effectivePower(type, p.power);

    for (int py = yStart; py < yEnd; ++py) {
        for (int px = 0; px < width; ++px) {
            T x0 = (T)(p.centerX + (px - width  * 0.5) * scale);
            T y0 = (T)(p.centerY + (py - height * 0.5) * scale);

            T zx, zy, cx, cy;
            if (type == (int)FractalType::Julia) {
                zx = x0; zy = y0; cx = (T)p.juliaCX; cy = (T)p.juliaCY;
            } else {
                zx = (T)0; zy = (T)0; cx = x0; cy = y0;
            }

            int iter = fr_iterate<T>(type, p.power, p.maxIter, cx, cy, zx, zy);
            out[py * width + px] = fr_shade(iter, p.maxIter,
                                            (float)zx, (float)zy, pw,
                                            p.colorScheme, p.colorDensity,
                                            p.colorOffset);
        }
    }
}

using RowFn = void (*)(const FractalParams&, uint32_t*, int, int);

RowFn pickRowFn(const FractalParams& p) {
    return p.doublePrecision ? &computeRows<double> : &computeRows<float>;
}

} // namespace

RenderResult FractalRenderer::renderCpu(const FractalParams& p, bool multiThreaded) {
    const int ss     = p.supersample;
    const int height = p.height * ss;

    m_buffer.assign((size_t)p.width * p.height, 0u);
    uint32_t* target = m_buffer.data();
    if (ss > 1) {
        m_hiBuffer.assign((size_t)p.width * ss * height, 0u);
        target = m_hiBuffer.data();
    }

    RowFn rows = pickRowFn(p);

    auto t0 = std::chrono::high_resolution_clock::now();

    if (!multiThreaded) {
        rows(p, target, 0, height);
    } else {
        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        int nThreads = (int)hw;

        std::vector<std::thread> pool;
        int rowsPer = (height + nThreads - 1) / nThreads;
        for (int t = 0; t < nThreads; ++t) {
            int ys = t * rowsPer;
            int ye = std::min(height, ys + rowsPer);
            if (ys >= ye) break;
            pool.emplace_back(rows, std::cref(p), target, ys, ye);
        }
        for (auto& th : pool) th.join();
    }

    if (ss > 1)
        downsample(p);   // included in the timing: it is part of the work

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    RenderResult r;
    r.image        = bufferToImage(p);
    r.milliseconds = ms;
    r.backendName  = multiThreaded ? "CPU (multi-threaded)" : "CPU (single-threaded)";
    r.ok           = true;
    return r;
}

#ifdef HAVE_CUDA
RenderResult FractalRenderer::renderCuda(const FractalParams& p) {
    m_buffer.assign((size_t)p.width * p.height, 0u);

    // The GPU path supersamples and down-samples entirely on the device, so
    // the PCIe transfer is always just the final image.
    double ms = 0.0;
    cudaRenderFractal(p, m_buffer.data(), &ms);

    RenderResult r;
    r.image        = bufferToImage(p);
    r.milliseconds = ms;
    r.backendName  = "CUDA (GPU)";
    r.ok           = true;
    return r;
}
#endif

// Box-filter the supersampled buffer down to the display resolution.
void FractalRenderer::downsample(const FractalParams& p) {
    const int ss = p.supersample;
    const int hw = p.width * ss;
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x)
            m_buffer[(size_t)y * p.width + x] =
                fr_averageBlock(m_hiBuffer.data(), hw, x * ss, y * ss, ss);
}

QImage FractalRenderer::bufferToImage(const FractalParams& p) {
    QImage img(p.width, p.height, QImage::Format_RGB32);
    for (int y = 0; y < p.height; ++y) {
        std::memcpy(img.scanLine(y),
                    m_buffer.data() + (size_t)y * p.width,
                    (size_t)p.width * sizeof(uint32_t));
    }
    return img;
}

RenderResult FractalRenderer::render(const FractalParams& p, Backend backend) {
    switch (backend) {
        case Backend::CpuSingle: return renderCpu(p, false);
        case Backend::CpuMulti:  return renderCpu(p, true);
        case Backend::Cuda:
#ifdef HAVE_CUDA
            return renderCuda(p);
#else
            return renderCpu(p, true);   // graceful fallback if built without CUDA
#endif
    }
    return {};
}

bool FractalRenderer::cudaSupported() {
#ifdef HAVE_CUDA
    return cudaAvailable();
#else
    return false;
#endif
}

QString FractalRenderer::backendName(Backend b) {
    switch (b) {
        case Backend::CpuSingle: return "CPU (single-threaded)";
        case Backend::CpuMulti:  return "CPU (multi-threaded)";
        case Backend::Cuda:      return "CUDA (GPU)";
    }
    return "unknown";
}
