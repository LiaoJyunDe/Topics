#pragma once

#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QMouseEvent>
#include <QDate>
#include <QTime>

class TimelineChartView : public QChartView
{
public:
    explicit TimelineChartView(QChart *chart, QWidget *parent = nullptr)
        : QChartView(chart, parent) {}

protected:
    QPoint lastPos;

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            lastPos = event->pos();

        QChartView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton))
            return;

        auto axisX = qobject_cast<QDateTimeAxis*>(chart()->axisX());
        if (!axisX) return;

        int dx = event->pos().x() - lastPos.x();
        lastPos = event->pos();

        QRectF plotArea = chart()->plotArea();
        if (plotArea.width() <= 0) return;

        qint64 spanMs = axisX->min().msecsTo(axisX->max());
        qint64 deltaMs = -dx * spanMs / plotArea.width();

        axisX->setRange(
            axisX->min().addMSecs(deltaMs),
            axisX->max().addMSecs(deltaMs)
            );
    }

    void wheelEvent(QWheelEvent *event) override
    {
        event->accept();
    }
};
