#include "MainWindow.h"
#include "FractalWidget.h"

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QList>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Fractal Studio — CPU vs CUDA");

    m_suppress = true;

    // ---- Canvas ------------------------------------------------------------
    m_canvas = new FractalWidget(this);

    // ---- Control panel -----------------------------------------------------
    m_typeCombo = new QComboBox;
    m_typeCombo->addItem("Mandelbrot");
    m_typeCombo->addItem("Julia");

    m_juliaCX = new QDoubleSpinBox;
    m_juliaCX->setRange(-2.0, 2.0);
    m_juliaCX->setSingleStep(0.01);
    m_juliaCX->setDecimals(4);
    m_juliaCX->setValue(m_params.juliaCX);
    m_juliaCX->setEnabled(false);

    m_juliaCY = new QDoubleSpinBox;
    m_juliaCY->setRange(-2.0, 2.0);
    m_juliaCY->setSingleStep(0.01);
    m_juliaCY->setDecimals(4);
    m_juliaCY->setValue(m_params.juliaCY);
    m_juliaCY->setEnabled(false);

    m_iterSpin = new QSpinBox;
    m_iterSpin->setRange(10, 5000);
    m_iterSpin->setSingleStep(50);
    m_iterSpin->setValue(m_params.maxIter);

    m_colorCombo = new QComboBox;
    m_colorCombo->addItem("Electric");
    m_colorCombo->addItem("Fire");
    m_colorCombo->addItem("Ocean");

    m_backendCombo = new QComboBox;
    m_backendCombo->addItem("CPU (single-threaded)", (int)Backend::CpuSingle);
    m_backendCombo->addItem("CPU (multi-threaded)",  (int)Backend::CpuMulti);
    if (FractalRenderer::cudaSupported())
        m_backendCombo->addItem("CUDA (GPU)", (int)Backend::Cuda);

    // Prefer CUDA by default when it is available.
    int cudaIdx = m_backendCombo->findData((int)Backend::Cuda);
    if (cudaIdx >= 0) m_backendCombo->setCurrentIndex(cudaIdx);

    auto* renderForm = new QFormLayout;
    renderForm->addRow("Fractal",  m_typeCombo);
    renderForm->addRow("Julia cRe", m_juliaCX);
    renderForm->addRow("Julia cIm", m_juliaCY);
    renderForm->addRow("Max iter", m_iterSpin);
    renderForm->addRow("Palette",  m_colorCombo);
    renderForm->addRow("Backend",  m_backendCombo);

    auto* paramsBox = new QGroupBox("Parameters");
    paramsBox->setLayout(renderForm);

    auto* benchButton = new QPushButton("Benchmark all backends");
    auto* resetButton = new QPushButton("Reset view");

    m_timingLabel = new QLabel("—");
    m_timingLabel->setWordWrap(true);
    m_benchLabel  = new QLabel();
    m_benchLabel->setWordWrap(true);
    m_benchLabel->setTextFormat(Qt::RichText);

    auto* statusBox    = new QGroupBox("Timing");
    auto* statusLayout = new QVBoxLayout;
    statusLayout->addWidget(m_timingLabel);
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    statusLayout->addWidget(line);
    statusLayout->addWidget(m_benchLabel);
    statusBox->setLayout(statusLayout);

    auto* hint = new QLabel("Scroll to zoom · drag to pan");
    hint->setStyleSheet("color: #888;");

    auto* panelLayout = new QVBoxLayout;
    panelLayout->addWidget(paramsBox);
    panelLayout->addWidget(benchButton);
    panelLayout->addWidget(resetButton);
    panelLayout->addWidget(statusBox);
    panelLayout->addStretch();
    panelLayout->addWidget(hint);

    auto* panel = new QWidget;
    panel->setLayout(panelLayout);
    panel->setFixedWidth(300);

    auto* central       = new QWidget;
    auto* centralLayout = new QHBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->addWidget(panel);
    centralLayout->addWidget(m_canvas, 1);
    setCentralWidget(central);
    resize(1200, 760);

    // ---- Signals -----------------------------------------------------------
    connect(m_typeCombo,    &QComboBox::currentIndexChanged, this, &MainWindow::onFractalTypeChanged);
    connect(m_colorCombo,   &QComboBox::currentIndexChanged, this, &MainWindow::onControlsChanged);
    connect(m_backendCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onBackendChanged);
    connect(m_iterSpin,     &QSpinBox::valueChanged,         this, &MainWindow::onControlsChanged);
    connect(m_juliaCX,      &QDoubleSpinBox::valueChanged,   this, &MainWindow::onControlsChanged);
    connect(m_juliaCY,      &QDoubleSpinBox::valueChanged,   this, &MainWindow::onControlsChanged);
    connect(benchButton,    &QPushButton::clicked,           this, &MainWindow::onBenchmark);
    connect(resetButton,    &QPushButton::clicked,           this, &MainWindow::onResetView);

    connect(m_canvas, &FractalWidget::zoomRequested,  this, &MainWindow::onZoom);
    connect(m_canvas, &FractalWidget::panRequested,   this, &MainWindow::onPan);
    connect(m_canvas, &FractalWidget::canvasResized,  this, &MainWindow::onCanvasResized);

    m_suppress = false;
    // The first canvasResized (on show) triggers the initial render.
}

Backend MainWindow::currentBackend() const {
    return static_cast<Backend>(m_backendCombo->currentData().toInt());
}

void MainWindow::render() {
    if (m_suppress) return;
    if (m_params.width <= 0 || m_params.height <= 0) return;

    RenderResult r = m_renderer.render(m_params, currentBackend());
    if (r.ok) {
        m_canvas->setImage(r.image);
        m_timingLabel->setText(QString("%1\n%2 ms")
                                   .arg(r.backendName)
                                   .arg(QString::number(r.milliseconds, 'f', 2)));
    }
}

void MainWindow::onControlsChanged() {
    if (m_suppress) return;
    m_params.maxIter     = m_iterSpin->value();
    m_params.colorScheme = m_colorCombo->currentIndex();
    m_params.juliaCX     = m_juliaCX->value();
    m_params.juliaCY     = m_juliaCY->value();
    render();
}

void MainWindow::onFractalTypeChanged() {
    m_params.type = (m_typeCombo->currentIndex() == 1) ? FractalType::Julia
                                                       : FractalType::Mandelbrot;
    bool julia = (m_params.type == FractalType::Julia);
    m_juliaCX->setEnabled(julia);
    m_juliaCY->setEnabled(julia);

    // Reset the view to a sensible default for the selected fractal.
    if (julia) { m_params.centerX = 0.0;  m_params.centerY = 0.0; m_params.spanX = 3.0; }
    else       { m_params.centerX = -0.5; m_params.centerY = 0.0; m_params.spanX = 3.5; }
    render();
}

void MainWindow::onBackendChanged() {
    render();
}

void MainWindow::onResetView() {
    onFractalTypeChanged();
}

void MainWindow::onZoom(QPointF pos, double factor) {
    double scale = m_params.spanX / m_params.width;
    // Complex coordinate currently under the cursor.
    double cx = m_params.centerX + (pos.x() - m_params.width  * 0.5) * scale;
    double cy = m_params.centerY + (pos.y() - m_params.height * 0.5) * scale;

    m_params.spanX *= factor;
    double ns = m_params.spanX / m_params.width;

    // Keep that same complex point under the cursor after the zoom.
    m_params.centerX = cx - (pos.x() - m_params.width  * 0.5) * ns;
    m_params.centerY = cy - (pos.y() - m_params.height * 0.5) * ns;
    render();
}

void MainWindow::onPan(QPoint delta) {
    double scale = m_params.spanX / m_params.width;
    m_params.centerX -= delta.x() * scale;
    m_params.centerY -= delta.y() * scale;
    render();
}

void MainWindow::onCanvasResized(QSize s) {
    m_params.width  = s.width();
    m_params.height = s.height();
    render();
}

void MainWindow::onBenchmark() {
    QList<Backend> backends { Backend::CpuSingle, Backend::CpuMulti };
    if (FractalRenderer::cudaSupported())
        backends << Backend::Cuda;

    QString text = QString("<b>Benchmark</b> (%1×%2, %3 iter)<br/>")
                       .arg(m_params.width).arg(m_params.height).arg(m_params.maxIter);

    double ref = -1.0;   // CPU single-threaded time, used as speedup reference
    for (Backend b : backends) {
        RenderResult r = m_renderer.render(m_params, b);
        if (b == Backend::CpuSingle) ref = r.milliseconds;
        double sp = (ref > 0.0) ? ref / r.milliseconds : 1.0;
        text += QString("%1: <b>%2 ms</b> (%3×)<br/>")
                    .arg(r.backendName)
                    .arg(QString::number(r.milliseconds, 'f', 2))
                    .arg(QString::number(sp, 'f', 1));
        m_canvas->setImage(r.image);   // leave the last (GPU) image on screen
    }
    m_benchLabel->setText(text);
}
