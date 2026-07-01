#pragma once
#include <QMainWindow>
#include "Types.h"
#include "FractalRenderer.h"

class FractalWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onControlsChanged();
    void onFractalTypeChanged();
    void onBackendChanged();
    void onBenchmark();
    void onResetView();
    void onZoom(QPointF widgetPos, double factor);
    void onPan(QPoint deltaPixels);
    void onCanvasResized(QSize newSize);

private:
    Backend currentBackend() const;
    void    render();               // render current params with selected backend

    FractalParams   m_params;
    FractalRenderer m_renderer;

    FractalWidget*  m_canvas      = nullptr;
    QComboBox*      m_typeCombo   = nullptr;
    QDoubleSpinBox* m_juliaCX     = nullptr;
    QDoubleSpinBox* m_juliaCY     = nullptr;
    QSpinBox*       m_iterSpin    = nullptr;
    QComboBox*      m_colorCombo  = nullptr;
    QComboBox*      m_backendCombo= nullptr;
    QLabel*         m_timingLabel = nullptr;
    QLabel*         m_benchLabel  = nullptr;

    bool m_suppress = false;        // guard against re-entrant renders during setup
};
