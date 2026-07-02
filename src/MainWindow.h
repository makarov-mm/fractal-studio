#pragma once
#include <QMainWindow>
#include <QStringList>
#include <QThread>
#include "Types.h"
#include "FractalRenderer.h"

class FractalWidget;
class RenderWorker;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    // Jobs posted to the RenderWorker thread (queued connections).
    void renderJob(FractalParams p, Backend backend, bool preview);
    void benchmarkJob(FractalParams p, int repeats);
    void exportJob(FractalParams p, Backend backend, QString path);

private slots:
    void onControlsChanged();
    void onFractalTypeChanged();
    void onBenchmark();
    void onResetView();
    void onExport();
    void onZoom(QPointF widgetPos, double factor);
    void onPan(QPoint deltaPixels);
    void onCanvasResized(QSize newSize);
    void onHovered(QPointF widgetPos);
    void onJuliaSeedPicked(QPointF widgetPos);

    void onRenderDone(RenderResult result, bool preview);
    void onBenchLine(QString line);
    void onBenchDone(RenderResult lastResult);
    void onExportDone(bool ok, QString path, double milliseconds);

private:
    Backend currentBackend() const;
    void    pullParamsFromControls();
    void    resetViewForType();
    void    scheduleRender(bool preview);   // latest-wins coalescing
    void    dispatch(const FractalParams& p, Backend b, bool preview);
    void    setUiBusy(bool busy);           // for benchmark / export
    QPointF widgetToComplex(QPointF pos) const;

    FractalParams m_params;

    // Worker thread ---------------------------------------------------------
    QThread       m_thread;
    RenderWorker* m_worker = nullptr;
    bool          m_busy         = false;   // a job is in flight
    bool          m_pendingValid = false;   // a newer request is waiting
    FractalParams m_pendingParams;
    Backend       m_pendingBackend = Backend::CpuSingle;
    bool          m_pendingPreview = false;

    // Widgets ----------------------------------------------------------------
    FractalWidget*  m_canvas       = nullptr;
    QComboBox*      m_typeCombo    = nullptr;
    QSpinBox*       m_powerSpin    = nullptr;
    QDoubleSpinBox* m_juliaCX      = nullptr;
    QDoubleSpinBox* m_juliaCY      = nullptr;
    QSpinBox*       m_iterSpin     = nullptr;
    QComboBox*      m_precCombo    = nullptr;
    QComboBox*      m_ssCombo      = nullptr;
    QComboBox*      m_backendCombo = nullptr;
    QComboBox*      m_colorCombo   = nullptr;
    QSlider*        m_densitySlider= nullptr;
    QSlider*        m_offsetSlider = nullptr;
    QPushButton*    m_benchButton  = nullptr;
    QPushButton*    m_exportButton = nullptr;
    QComboBox*      m_exportScale  = nullptr;
    QLabel*         m_timingLabel  = nullptr;
    QLabel*         m_benchLabel   = nullptr;
    QLabel*         m_coordLabel   = nullptr;
    QLabel*         m_zoomLabel    = nullptr;

    QStringList m_benchLines;
    bool m_suppress = false;        // guard against re-entrant renders during setup
};
