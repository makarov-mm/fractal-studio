#pragma once
#include "Types.h"
#include "FractalRenderer.h"
#include <QObject>

// Lives on a dedicated QThread and owns the only FractalRenderer instance, so
// heavy CPU renders never block the GUI. The MainWindow talks to it purely
// through queued signal/slot connections and keeps at most one job in flight
// (latest-wins coalescing happens on the GUI side).
class RenderWorker : public QObject {
    Q_OBJECT
public slots:
    // Renders `p` with the given back-end. `preview` is echoed back so the
    // GUI can distinguish quick half-resolution previews from final frames.
    void doRender(FractalParams p, Backend backend, bool preview);

    // Runs every back-end `repeats` times on the same parameters and reports
    // the best time per back-end, streaming one line per back-end so the UI
    // updates progressively.
    void doBenchmark(FractalParams p, int repeats);

    // Renders `p` (already scaled to the export resolution) and saves a PNG.
    void doExport(FractalParams p, Backend backend, QString path);

signals:
    void renderDone(RenderResult result, bool preview);
    void benchLine(QString richTextLine);
    void benchDone(RenderResult lastResult);
    void exportDone(bool ok, QString path, double milliseconds);

private:
    FractalRenderer m_renderer;
};
