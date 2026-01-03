#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include <QTimer>
#include <QNetworkAccessManager>
class TimelineChartView;
class TimelineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget *parent = nullptr);

private slots:
    void fetchFirebase();
    void onFirebaseReply(QNetworkReply *reply);

private:
    void setupChart();
    void updateAxisRange();

    QChart *chart;
    TimelineChartView *chartView;

    QScatterSeries *ownerSeries;
    QScatterSeries *unknownSeries;
    QScatterSeries *visitorSeries;

    QDateTimeAxis *axisX;
    QValueAxis *axisY;

    QNetworkAccessManager *manager;
    QTimer *refreshTimer;
};
#endif

