#pragma once
#include "Types.h"
#include <QImage>
#include <QString>
#include <cstdint>
#include <vector>

enum class Backend {
    CpuSingle,
    CpuMulti,
    Cuda
};

struct RenderResult {
    QImage  image;
    double  milliseconds = 0.0;
    QString backendName;
    bool    ok = false;
};

// Owns a reusable pixel buffer and dispatches rendering to the chosen back-end.
class FractalRenderer {
public:
    RenderResult render(const FractalParams& p, Backend backend);

    static bool    cudaSupported();
    static QString backendName(Backend b);

private:
    std::vector<uint32_t> m_buffer;

    RenderResult renderCpu(const FractalParams& p, bool multiThreaded);
#ifdef HAVE_CUDA
    RenderResult renderCuda(const FractalParams& p);
#endif
    QImage bufferToImage(const FractalParams& p);
};
