#include "FractalWidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <cmath>

FractalWidget::FractalWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 240);
    setCursor(Qt::CrossCursor);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);   // hover coordinates without a button pressed
}

void FractalWidget::setImage(const QImage& img) {
    m_image = img;
    update();
}

void FractalWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_image.isNull()) return;

    if (m_image.size() == size()) {
        p.drawImage(0, 0, m_image);          // 1:1 full-quality frame
    } else {
        // Half-resolution preview (or a stale frame right after a resize):
        // stretch it to fill the widget until the sharp frame arrives.
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(rect(), m_image);
    }
}

void FractalWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    emit canvasResized(e->size());
}

void FractalWidget::wheelEvent(QWheelEvent* e) {
    double steps  = e->angleDelta().y() / 120.0;
    // Wheel up -> zoom in: factor < 1 shrinks the visible span.
    double factor = std::pow(1.2, -steps);
    emit zoomRequested(e->position(), factor);
}

void FractalWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging  = true;
        m_lastMouse = e->pos();
    } else if (e->button() == Qt::RightButton) {
        emit juliaSeedPicked(e->position());
    }
}

void FractalWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging) {
        QPoint delta = e->pos() - m_lastMouse;
        m_lastMouse  = e->pos();
        emit panRequested(delta);
    }
    emit hovered(e->position());
}

void FractalWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        m_dragging = false;
}
