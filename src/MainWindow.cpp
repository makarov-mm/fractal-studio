#include "MainWindow.h"
#include "FractalWidget.h"
#include "RenderWorker.h"

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileDialog>
#include <QShortcut>
#include <QKeySequence>
#include <QStatusBar>
#include <QDateTime>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Fractal Studio — CPU vs CUDA");

    m_suppress = true;

    // ---- Canvas ------------------------------------------------------------
    m_canvas = new FractalWidget(this);

    // ---- Fractal group -----------------------------------------------------
    m_typeCombo = new QComboBox;
    m_typeCombo->addItem("Mandelbrot",   (int)FractalType::Mandelbrot);
    m_typeCombo->addItem("Julia",        (int)FractalType::Julia);
    m_typeCombo->addItem("Burning Ship", (int)FractalType::BurningShip);
    m_typeCombo->addItem("Tricorn",      (int)FractalType::Tricorn);
    m_typeCombo->addItem("Multibrot",    (int)FractalType::Multibrot);

    m_powerSpin = new QSpinBox;
    m_powerSpin->setRange(3, 8);
    m_powerSpin->setValue(m_params.power);
    m_powerSpin->setToolTip("Exponent n of z\u207F + c (Multibrot only)");
    m_powerSpin->setEnabled(false);

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

    auto* fractalForm = new QFormLayout;
    fractalForm->addRow("Fractal",   m_typeCombo);
    fractalForm->addRow("Power n",   m_powerSpin);
    fractalForm->addRow("Julia cRe", m_juliaCX);
    fractalForm->addRow("Julia cIm", m_juliaCY);
    auto* fractalBox = new QGroupBox("Fractal");
    fractalBox->setLayout(fractalForm);

    // ---- Rendering group ----------------------------------------------------
    m_iterSpin = new QSpinBox;
    m_iterSpin->setRange(10, 20000);
    m_iterSpin->setSingleStep(50);
    m_iterSpin->setValue(m_params.maxIter);

    m_precCombo = new QComboBox;
    m_precCombo->addItem("float (fast)");
    m_precCombo->addItem("double (deep zoom)");
    m_precCombo->setToolTip("float bottoms out around span 1e-5;\n"
                            "double reaches ~1e-13 but is heavily\n"
                            "throttled on consumer GPUs (FP64 \u2248 1:64)");

    m_ssCombo = new QComboBox;
    m_ssCombo->addItem("Off");
    m_ssCombo->addItem("2\u00D72");
    m_ssCombo->setToolTip("Ordered supersampling: renders at 2\u00D7 resolution\n"
                          "and box-filters down (4\u00D7 the work)");

    m_backendCombo = new QComboBox;
    m_backendCombo->addItem("CPU (single-threaded)", (int)Backend::CpuSingle);
    m_backendCombo->addItem("CPU (multi-threaded)",  (int)Backend::CpuMulti);
    if (FractalRenderer::cudaSupported())
        m_backendCombo->addItem("CUDA (GPU)", (int)Backend::Cuda);

    // Prefer CUDA by default when it is available.
    int cudaIdx = m_backendCombo->findData((int)Backend::Cuda);
    if (cudaIdx >= 0) m_backendCombo->setCurrentIndex(cudaIdx);

    auto* renderForm = new QFormLayout;
    renderForm->addRow("Max iter",     m_iterSpin);
    renderForm->addRow("Precision",    m_precCombo);
    renderForm->addRow("Antialiasing", m_ssCombo);
    renderForm->addRow("Backend",      m_backendCombo);
    auto* renderBox = new QGroupBox("Rendering");
    renderBox->setLayout(renderForm);

    // ---- Color group ---------------------------------------------------------
    m_colorCombo = new QComboBox;
    m_colorCombo->addItem("Electric");
    m_colorCombo->addItem("Fire");
    m_colorCombo->addItem("Ocean");
    m_colorCombo->addItem("Twilight");
    m_colorCombo->addItem("Neon");
    m_colorCombo->addItem("Grayscale");

    m_densitySlider = new QSlider(Qt::Horizontal);
    m_densitySlider->setRange(25, 400);            // maps to 0.25 .. 4.0
    m_densitySlider->setValue(100);
    m_densitySlider->setToolTip("Stretches / compresses the palette cycle");

    m_offsetSlider = new QSlider(Qt::Horizontal);
    m_offsetSlider->setRange(0, 99);               // maps to 0.00 .. 0.99
    m_offsetSlider->setValue(0);
    m_offsetSlider->setToolTip("Rotates the palette");

    auto* colorForm = new QFormLayout;
    colorForm->addRow("Palette", m_colorCombo);
    colorForm->addRow("Density", m_densitySlider);
    colorForm->addRow("Offset",  m_offsetSlider);
    auto* colorBox = new QGroupBox("Color");
    colorBox->setLayout(colorForm);

    // ---- Buttons -------------------------------------------------------------
    m_benchButton  = new QPushButton("Benchmark all backends");
    m_benchButton->setToolTip("3 runs per back-end, best time reported (Ctrl+B)");
    auto* resetButton = new QPushButton("Reset view");
    resetButton->setToolTip("Ctrl+R");

    m_exportButton = new QPushButton("Export PNG\u2026");
    m_exportButton->setToolTip("Renders at the chosen multiple of the current\n"
                               "view size with 2\u00D72 supersampling (Ctrl+E)");
    m_exportScale = new QComboBox;
    m_exportScale->addItem("1\u00D7", 1);
    m_exportScale->addItem("2\u00D7", 2);
    m_exportScale->addItem("4\u00D7", 4);
    m_exportScale->setCurrentIndex(1);
    auto* exportRow = new QHBoxLayout;
    exportRow->addWidget(m_exportButton, 1);
    exportRow->addWidget(m_exportScale);

    // ---- Timing box ------------------------------------------------------------
    m_timingLabel = new QLabel("\u2014");
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

    auto* hint = new QLabel("Scroll to zoom \u00B7 drag to pan\n"
                            "Right-click: pick Julia c at cursor");
    hint->setStyleSheet("color: #888;");

    auto* panelLayout = new QVBoxLayout;
    panelLayout->addWidget(fractalBox);
    panelLayout->addWidget(renderBox);
    panelLayout->addWidget(colorBox);
    panelLayout->addWidget(m_benchButton);
    panelLayout->addWidget(resetButton);
    panelLayout->addLayout(exportRow);
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

    // ---- Status bar: live coordinates and zoom factor ------------------------
    m_coordLabel = new QLabel("c = \u2014");
    m_zoomLabel  = new QLabel("zoom 1.0\u00D7");
    statusBar()->addWidget(m_coordLabel, 1);
    statusBar()->addPermanentWidget(m_zoomLabel);

    // ---- Worker thread --------------------------------------------------------
    m_worker = new RenderWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &MainWindow::renderJob,    m_worker, &RenderWorker::doRender);
    connect(this, &MainWindow::benchmarkJob, m_worker, &RenderWorker::doBenchmark);
    connect(this, &MainWindow::exportJob,    m_worker, &RenderWorker::doExport);

    connect(m_worker, &RenderWorker::renderDone, this, &MainWindow::onRenderDone);
    connect(m_worker, &RenderWorker::benchLine,  this, &MainWindow::onBenchLine);
    connect(m_worker, &RenderWorker::benchDone,  this, &MainWindow::onBenchDone);
    connect(m_worker, &RenderWorker::exportDone, this, &MainWindow::onExportDone);

    m_thread.start();

    // ---- Signals ---------------------------------------------------------------
    connect(m_typeCombo,    &QComboBox::currentIndexChanged, this, &MainWindow::onFractalTypeChanged);
    connect(m_powerSpin,    &QSpinBox::valueChanged,         this, &MainWindow::onControlsChanged);
    connect(m_juliaCX,      &QDoubleSpinBox::valueChanged,   this, &MainWindow::onControlsChanged);
    connect(m_juliaCY,      &QDoubleSpinBox::valueChanged,   this, &MainWindow::onControlsChanged);
    connect(m_iterSpin,     &QSpinBox::valueChanged,         this, &MainWindow::onControlsChanged);
    connect(m_precCombo,    &QComboBox::currentIndexChanged, this, &MainWindow::onControlsChanged);
    connect(m_ssCombo,      &QComboBox::currentIndexChanged, this, &MainWindow::onControlsChanged);
    connect(m_backendCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onControlsChanged);
    connect(m_colorCombo,   &QComboBox::currentIndexChanged, this, &MainWindow::onControlsChanged);
    connect(m_densitySlider,&QSlider::valueChanged,          this, &MainWindow::onControlsChanged);
    connect(m_offsetSlider, &QSlider::valueChanged,          this, &MainWindow::onControlsChanged);

    connect(m_benchButton,  &QPushButton::clicked,           this, &MainWindow::onBenchmark);
    connect(resetButton,    &QPushButton::clicked,           this, &MainWindow::onResetView);
    connect(m_exportButton, &QPushButton::clicked,           this, &MainWindow::onExport);

    connect(m_canvas, &FractalWidget::zoomRequested,   this, &MainWindow::onZoom);
    connect(m_canvas, &FractalWidget::panRequested,    this, &MainWindow::onPan);
    connect(m_canvas, &FractalWidget::canvasResized,   this, &MainWindow::onCanvasResized);
    connect(m_canvas, &FractalWidget::hovered,         this, &MainWindow::onHovered);
    connect(m_canvas, &FractalWidget::juliaSeedPicked, this, &MainWindow::onJuliaSeedPicked);

    new QShortcut(QKeySequence("Ctrl+B"), this, this, &MainWindow::onBenchmark);
    new QShortcut(QKeySequence("Ctrl+R"), this, this, &MainWindow::onResetView);
    new QShortcut(QKeySequence("Ctrl+E"), this, this, &MainWindow::onExport);

    m_suppress = false;
    // The first canvasResized (on show) triggers the initial render.
}

MainWindow::~MainWindow() {
    m_thread.quit();
    m_thread.wait();
}

Backend MainWindow::currentBackend() const {
    return static_cast<Backend>(m_backendCombo->currentData().toInt());
}

void MainWindow::pullParamsFromControls() {
    m_params.type         = (FractalType)m_typeCombo->currentData().toInt();
    m_params.power        = m_powerSpin->value();
    m_params.juliaCX      = m_juliaCX->value();
    m_params.juliaCY      = m_juliaCY->value();
    m_params.maxIter      = m_iterSpin->value();
    m_params.doublePrecision = (m_precCombo->currentIndex() == 1);
    m_params.supersample  = (m_ssCombo->currentIndex() == 1) ? 2 : 1;
    m_params.colorScheme  = m_colorCombo->currentIndex();
    m_params.colorDensity = m_densitySlider->value() / 100.0f;
    m_params.colorOffset  = m_offsetSlider->value()  / 100.0f;
}

// ---- Latest-wins job coalescing --------------------------------------------
//
// At most one job is ever in flight on the worker; while it runs, the newest
// request simply overwrites the previous pending one. After a preview frame
// lands and nothing newer is queued, a full-quality frame is scheduled
// automatically, so interaction stays fluid and always settles sharp.

void MainWindow::dispatch(const FractalParams& p, Backend b, bool preview) {
    m_busy = true;
    emit renderJob(p, b, preview);
}

void MainWindow::scheduleRender(bool preview) {
    if (m_suppress) return;
    if (m_params.width <= 0 || m_params.height <= 0) return;

    FractalParams p = m_params;
    if (preview) {
        // Quick half-resolution frame; the widget stretches it meanwhile.
        p.width       = std::max(1, p.width  / 2);
        p.height      = std::max(1, p.height / 2);
        p.supersample = 1;
    }

    if (m_busy) {
        m_pendingParams  = p;
        m_pendingBackend = currentBackend();
        m_pendingPreview = preview;
        m_pendingValid   = true;
    } else {
        dispatch(p, currentBackend(), preview);
    }
}

void MainWindow::onRenderDone(RenderResult r, bool preview) {
    m_busy = false;
    if (r.ok) {
        m_canvas->setImage(r.image);
        if (!preview)
            m_timingLabel->setText(QString("%1\n%2 ms")
                                       .arg(r.backendName)
                                       .arg(QString::number(r.milliseconds, 'f', 2)));
    }

    if (m_pendingValid) {
        m_pendingValid = false;
        dispatch(m_pendingParams, m_pendingBackend, m_pendingPreview);
    } else if (preview) {
        scheduleRender(false);   // settle to a sharp full-resolution frame
    }
}

// ---- Slots -------------------------------------------------------------------

void MainWindow::onControlsChanged() {
    if (m_suppress) return;
    pullParamsFromControls();
    scheduleRender(false);
}

void MainWindow::resetViewForType() {
    switch (m_params.type) {
    case FractalType::Julia:
        m_params.centerX = 0.0;  m_params.centerY = 0.0;  m_params.spanX = 3.0; break;
    case FractalType::BurningShip:
        // Default view framing the whole ship, antennas up.
        m_params.centerX = -0.4; m_params.centerY = -0.4; m_params.spanX = 3.5; break;
    default:
        m_params.centerX = -0.5; m_params.centerY = 0.0;  m_params.spanX = 3.5; break;
    }
    m_zoomLabel->setText("zoom 1.0\u00D7");
}

void MainWindow::onFractalTypeChanged() {
    if (m_suppress) return;
    pullParamsFromControls();

    bool julia = (m_params.type == FractalType::Julia);
    m_juliaCX->setEnabled(julia);
    m_juliaCY->setEnabled(julia);
    m_powerSpin->setEnabled(m_params.type == FractalType::Multibrot);

    resetViewForType();
    scheduleRender(false);
}

void MainWindow::onResetView() {
    resetViewForType();
    scheduleRender(false);
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

    m_zoomLabel->setText(QString("zoom %1\u00D7")
                             .arg(QString::number(3.5 / m_params.spanX, 'g', 3)));
    scheduleRender(true);
}

void MainWindow::onPan(QPoint delta) {
    double scale = m_params.spanX / m_params.width;
    m_params.centerX -= delta.x() * scale;
    m_params.centerY -= delta.y() * scale;
    scheduleRender(true);
}

void MainWindow::onCanvasResized(QSize s) {
    m_params.width  = s.width();
    m_params.height = s.height();
    scheduleRender(true);
}

QPointF MainWindow::widgetToComplex(QPointF pos) const {
    double scale = m_params.spanX / m_params.width;
    return { m_params.centerX + (pos.x() - m_params.width  * 0.5) * scale,
             m_params.centerY + (pos.y() - m_params.height * 0.5) * scale };
}

void MainWindow::onHovered(QPointF pos) {
    QPointF c = widgetToComplex(pos);
    m_coordLabel->setText(QString("c = %1 %2 %3i \u00B7 span %4")
                              .arg(QString::number(c.x(), 'f', 10))
                              .arg(c.y() < 0 ? "\u2212" : "+")
                              .arg(QString::number(std::abs(c.y()), 'f', 10))
                              .arg(QString::number(m_params.spanX, 'g', 3)));
}

void MainWindow::onJuliaSeedPicked(QPointF pos) {
    QPointF c = widgetToComplex(pos);

    m_suppress = true;
    m_juliaCX->setValue(std::clamp(c.x(), -2.0, 2.0));
    m_juliaCY->setValue(std::clamp(c.y(), -2.0, 2.0));
    int juliaIdx = m_typeCombo->findData((int)FractalType::Julia);
    m_typeCombo->setCurrentIndex(juliaIdx);
    m_suppress = false;

    onFractalTypeChanged();   // enables the spins, resets the view, renders
}

// ---- Benchmark ---------------------------------------------------------------

void MainWindow::setUiBusy(bool busy) {
    m_benchButton->setEnabled(!busy);
    m_exportButton->setEnabled(!busy);
}

void MainWindow::onBenchmark() {
    if (m_busy) return;                 // a render is in flight; click again
    pullParamsFromControls();

    m_benchLines.clear();
    m_benchLines << QString("<b>Benchmark</b> (%1\u00D7%2, %3 iter, %4%5)")
                        .arg(m_params.width).arg(m_params.height)
                        .arg(m_params.maxIter)
                        .arg(m_params.doublePrecision ? "double" : "float")
                        .arg(m_params.supersample > 1 ? ", 2\u00D72 AA" : "");
    m_benchLabel->setText(m_benchLines.join("<br/>") + "<br/><i>running\u2026</i>");

    setUiBusy(true);
    m_busy = true;
    emit benchmarkJob(m_params, 3);
}

void MainWindow::onBenchLine(QString line) {
    m_benchLines << line;
    m_benchLabel->setText(m_benchLines.join("<br/>") + "<br/><i>running\u2026</i>");
}

void MainWindow::onBenchDone(RenderResult last) {
    m_benchLabel->setText(m_benchLines.join("<br/>"));
    if (last.ok)
        m_canvas->setImage(last.image);   // leave the last (GPU) image on screen
    setUiBusy(false);
    m_busy = false;
    if (m_pendingValid) {
        m_pendingValid = false;
        dispatch(m_pendingParams, m_pendingBackend, m_pendingPreview);
    }
}

// ---- PNG export ----------------------------------------------------------------

void MainWindow::onExport() {
    if (m_busy) return;
    pullParamsFromControls();

    QString suggested = QString("fractal_%1.png")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString path = QFileDialog::getSaveFileName(this, "Export PNG", suggested,
                                                "PNG images (*.png)");
    if (path.isEmpty()) return;

    int scale = m_exportScale->currentData().toInt();
    FractalParams p = m_params;
    p.width      *= scale;
    p.height     *= scale;
    p.supersample = 2;                    // exports always get antialiasing

    statusBar()->showMessage(QString("Exporting %1\u00D7%2\u2026")
                                 .arg(p.width).arg(p.height));
    setUiBusy(true);
    m_busy = true;
    emit exportJob(p, currentBackend(), path);
}

void MainWindow::onExportDone(bool ok, QString path, double ms) {
    statusBar()->showMessage(ok ? QString("Saved %1 (%2 ms)")
                                      .arg(path)
                                      .arg(QString::number(ms, 'f', 0))
                                : QString("Export failed: %1").arg(path),
                             6000);
    setUiBusy(false);
    m_busy = false;
    if (m_pendingValid) {
        m_pendingValid = false;
        dispatch(m_pendingParams, m_pendingBackend, m_pendingPreview);
    }
}
