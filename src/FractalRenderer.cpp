#include "FractalRenderer.h"
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

// Compute a horizontal band of rows [yStart, yEnd) on the CPU.
// Same math and coloring as the CUDA kernel, so output matches pixel for pixel.
void computeRows(const FractalParams& p, uint32_t* out, int yStart, int yEnd) {
    const double scale   = p.spanX / p.width;
    const float  BAILOUT = 65536.0f;
    const int    type    = (int)p.type;

    for (int py = yStart; py < yEnd; ++py) {
        for (int px = 0; px < p.width; ++px) {
            float x0 = (float)(p.centerX + (px - p.width  * 0.5) * scale);
            float y0 = (float)(p.centerY + (py - p.height * 0.5) * scale);

            float zx, zy, cx, cy;
            if (type == 0) { zx = 0.0f; zy = 0.0f; cx = x0;              cy = y0;              }
            else           { zx = x0;   zy = y0;   cx = (float)p.juliaCX; cy = (float)p.juliaCY; }

            int   iter = 0;
            float mag2 = 0.0f;
            while (iter < p.maxIter) {
                float nx = zx * zx - zy * zy + cx;
                float ny = 2.0f * zx * zy + cy;
                zx = nx; zy = ny;
                mag2 = zx * zx + zy * zy;
                if (mag2 > BAILOUT) break;
                ++iter;
            }

            uint32_t color;
            if (iter >= p.maxIter) {
                color = 0xFF000000u;
            } else {
                float mu = fr_smoothValue(iter, zx, zy);
                float t  = mu * 0.02f; t -= floorf(t);
                color = fr_colorize(t, p.colorScheme);
            }
            out[py * p.width + px] = color;
        }
    }
}

} // namespace

RenderResult FractalRenderer::renderCpu(const FractalParams& p, bool multiThreaded) {
    m_buffer.assign((size_t)p.width * p.height, 0u);

    auto t0 = std::chrono::high_resolution_clock::now();

    if (!multiThreaded) {
        computeRows(p, m_buffer.data(), 0, p.height);
    } else {
        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        int nThreads = (int)hw;

        std::vector<std::thread> pool;
        int rowsPer = (p.height + nThreads - 1) / nThreads;
        for (int t = 0; t < nThreads; ++t) {
            int ys = t * rowsPer;
            int ye = std::min(p.height, ys + rowsPer);
            if (ys >= ye) break;
            pool.emplace_back(computeRows, std::cref(p), m_buffer.data(), ys, ye);
        }
        for (auto& th : pool) th.join();
    }

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
