#include "RenderWorker.h"
#include <QList>

void RenderWorker::doRender(FractalParams p, Backend backend, bool preview) {
    emit renderDone(m_renderer.render(p, backend), preview);
}

void RenderWorker::doBenchmark(FractalParams p, int repeats) {
    QList<Backend> backends { Backend::CpuSingle, Backend::CpuMulti };
    if (FractalRenderer::cudaSupported())
        backends << Backend::Cuda;

    double ref = -1.0;   // CPU single-threaded best time, speedup reference
    RenderResult last;

    for (Backend b : backends) {
        RenderResult best;
        for (int i = 0; i < repeats; ++i) {
            RenderResult r = m_renderer.render(p, b);
            if (!best.ok || r.milliseconds < best.milliseconds)
                best = r;
        }
        if (b == Backend::CpuSingle) ref = best.milliseconds;
        double sp = (ref > 0.0 && best.milliseconds > 0.0)
                        ? ref / best.milliseconds : 1.0;
        emit benchLine(QString("%1: <b>%2 ms</b> (%3×)")
                           .arg(best.backendName)
                           .arg(QString::number(best.milliseconds, 'f', 2))
                           .arg(QString::number(sp, 'f', 1)));
        last = best;
    }
    emit benchDone(last);
}

void RenderWorker::doExport(FractalParams p, Backend backend, QString path) {
    RenderResult r = m_renderer.render(p, backend);
    bool ok = r.ok && r.image.save(path, "PNG");
    emit exportDone(ok, path, r.milliseconds);
}
