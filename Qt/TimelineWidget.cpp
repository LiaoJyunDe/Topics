    #include "TimelineWidget.h"
    #include "TimelineChartView.h"
    #include <QVBoxLayout>
    #include <QJsonDocument>
    #include <QJsonObject>
    #include <QToolTip>
    #include <QCursor>
    #include <QDateTime>
    #include <QJsonArray>
    #include <QTimer>
    #include <QNetworkAccessManager>

    TimelineWidget::TimelineWidget(QWidget *parent)
        : QWidget(parent)
    {
        setupChart();

        manager = new QNetworkAccessManager(this);
        connect(manager, &QNetworkAccessManager::finished,
                this, &TimelineWidget::onFirebaseReply);

        refreshTimer = new QTimer(this);
        refreshTimer->setInterval(3000); // 3 秒刷新一次
        connect(refreshTimer, &QTimer::timeout,
                this, &TimelineWidget::fetchFirebase);

        refreshTimer->start();

        fetchFirebase(); // 開啟時先抓一次
    }

    void TimelineWidget::setupChart()
    {
        chart = new QChart();
        chart->setTitle("訪客時間軸");
        chart->setTitleBrush(QBrush(QColor("#FFFFFF")));
        chart->legend()->setAlignment(Qt::AlignTop);
        chart->legend()->setLabelBrush(QBrush(Qt::white));
        chart->setAnimationOptions(QChart::NoAnimation);
        chart->setAcceptHoverEvents(true);
        chart->setAcceptTouchEvents(true);
        // ===== 三種身分 =====
        ownerSeries   = new QScatterSeries();
        unknownSeries = new QScatterSeries();
        visitorSeries = new QScatterSeries();

        ownerSeries->setName("主人");
        unknownSeries->setName("陌生人");
        visitorSeries->setName("訪客");

        ownerSeries->setColor(QColor("#6FB7B7"));
        unknownSeries->setColor(QColor("#9999CC"));
        visitorSeries->setColor(QColor("#B766AD"));
        ownerSeries->setBorderColor(QColor("#87CEFA"));
        unknownSeries->setBorderColor(QColor("#87CEFA"));
        visitorSeries->setBorderColor(QColor("#87CEFA"));
        ownerSeries->setMarkerSize(11);
        unknownSeries->setMarkerSize(11);
        visitorSeries->setMarkerSize(11);

        chart->addSeries(ownerSeries);
        chart->addSeries(unknownSeries);
        chart->addSeries(visitorSeries);
        chart->setBackgroundBrush(QBrush(QColor("#353941")));
        chart->setBackgroundPen(Qt::NoPen);                     // 去邊框

        QColor axisColor("#FFFFFF");
        // ===== X 軸：時間 =====
        axisX = new QDateTimeAxis();
        axisX->setFormat("HH:mm:ss");
        axisX->setTitleText("時間");
        axisX->setLinePen(QPen(axisColor, 2));
        axisX->setLabelsBrush(axisColor);
        axisX->setTitleBrush(axisColor);
        axisX->setGridLineVisible(false);
        axisX->setTickCount(6);
        QDateTime now = QDateTime::currentDateTime();

        QDateTime viewStart = now.addSecs(-1800); // -30 分鐘
        QDateTime viewEnd   = now.addSecs( 1800); // +30 分鐘

        axisX->setRange(viewStart, viewEnd);
        axisX->setTickCount(5);

        // ===== Y 軸：單一水平線 =====
        axisY = new QValueAxis();
        axisY->setRange(0, 1);
        axisY->setLabelsVisible(false);
        axisY->setLinePen(QPen(axisColor, 2));
        axisY->setLabelsBrush(axisColor);
        axisY->setGridLineVisible(false);

        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);

        for (auto s : {ownerSeries, unknownSeries, visitorSeries}) {
            s->attachAxis(axisX);
            s->attachAxis(axisY);

            connect(s, &QScatterSeries::hovered,
                    this, [=](const QPointF &p, bool state){
                        if (state) {
                            QDateTime t = QDateTime::fromMSecsSinceEpoch((qint64)p.x());
                            QToolTip::showText(
                                QCursor::pos(),
                                s->name() + "\n" + t.toString("yyyy-MM-dd HH:mm:ss")
                                );
                        }
                    });
        }

        chartView = new TimelineChartView(chart);
        chartView->setMouseTracking(true);
        chartView->setDragMode(QGraphicsView::NoDrag);
        chartView->setRubberBand(QChartView::NoRubberBand);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(chartView);
        setLayout(layout);
    }

    void TimelineWidget::fetchFirebase()
    {
        QUrl url(
            "https://my-project-134ab-default-rtdb.asia-southeast1.firebasedatabase.app/timeline.json"
            );

        manager->get(QNetworkRequest(url));
    }

    void TimelineWidget::onFirebaseReply(QNetworkReply *reply)
    {
        QByteArray raw = reply->readAll();
        qDebug() << "RAW JSON =" << raw;

        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject root = doc.object();

        ownerSeries->clear();
        unknownSeries->clear();
        visitorSeries->clear();
        qDebug() << "DOC JSON =" << doc;

        for (auto it = root.begin(); it != root.end(); ++it)
        {
            qint64 tsMs = it.key().toLongLong() * 1000;

            QJsonArray peopleArray =
                it.value().toObject().value("people").toArray();

            qDebug() << "peopleArray size =" << peopleArray.size();

            for (const QJsonValue &v : peopleArray)
            {
                QJsonObject p = v.toObject();
                QString role = p.value("role").toString();

                qDebug() << "role =" << role;

                if (role == "owner")
                    ownerSeries->append(tsMs, 0.6);
                else if (role == "unknown")
                    unknownSeries->append(tsMs, 0.5);
                else
                    visitorSeries->append(tsMs, 0.4);
            }
        }

        updateAxisRange();
    }


    void TimelineWidget::updateAxisRange()
    {
        qint64 minX = LLONG_MAX;
        qint64 maxX = 0;

        auto scan = [&](QScatterSeries *s) {
            for (const QPointF &p : s->points()) {
                minX = qMin(minX, (qint64)p.x());
                maxX = qMax(maxX, (qint64)p.x());
            }
        };

        scan(ownerSeries);
        scan(unknownSeries);
        scan(visitorSeries);

        if (minX == LLONG_MAX)
            return;

        axisY->setRange(0.0, 1.0);
    }
