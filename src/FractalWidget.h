#pragma once
#include <QWidget>
#include <QImage>
#include <QPoint>

// Displays the rendered fractal image (1:1 with widget pixels) and reports
// mouse-wheel zoom, drag-pan and resize gestures to the MainWindow.
class FractalWidget : public QWidget {
    Q_OBJECT
public:
    explicit FractalWidget(QWidget* parent = nullptr);
    void setImage(const QImage& img);

signals:
    void zoomRequested(QPointF widgetPos, double factor);
    void panRequested(QPoint deltaPixels);
    void canvasResized(QSize newSize);

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
