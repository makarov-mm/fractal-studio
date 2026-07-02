#pragma once
#include <QWidget>
#include <QImage>
#include <QPoint>

// Displays the rendered fractal image and reports mouse-wheel zoom, drag-pan,
// resize and hover gestures to the MainWindow. Preview frames rendered at a
// reduced resolution are stretched to fill the widget until the full-quality
// frame replaces them.
class FractalWidget : public QWidget {
    Q_OBJECT
public:
    explicit FractalWidget(QWidget* parent = nullptr);
    void setImage(const QImage& img);

signals:
    void zoomRequested(QPointF widgetPos, double factor);
    void panRequested(QPoint deltaPixels);
    void canvasResized(QSize newSize);
    void hovered(QPointF widgetPos);          // for the coordinate read-out
    void juliaSeedPicked(QPointF widgetPos);  // right-click: pick Julia c

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QImage m_image;
    QPoint m_lastMouse;
    bool   m_dragging = false;
};
