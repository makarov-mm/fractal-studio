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

// Owns reusable pixel buffers and dispatches rendering to the chosen back-end.
// Not thread-safe by itself; in the app a single instance lives on the worker
// thread (RenderWorker) and is never touched from the GUI thread.
class FractalRenderer {
public:
    RenderResult render(const FractalParams& p, Backend backend);

    static bool    cudaSupported();
    static QString backendName(Backend b);

private:
    std::vector<uint32_t> m_buffer;     // final image, width * height
    std::vector<uint32_t> m_hiBuffer;   // supersampled image, (w*ss) * (h*ss)

    RenderResult renderCpu(const FractalParams& p, bool multiThreaded);
#ifdef HAVE_CUDA
    RenderResult renderCuda(const FractalParams& p);
#endif
    void   downsample(const FractalParams& p);
    QImage bufferToImage(const FractalParams& p);
};
