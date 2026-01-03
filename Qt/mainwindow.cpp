#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "TimelineWidget.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    const char * const vlc_args[] = {
        "--no-xlib",
        "--network-caching=150",
        "--rtsp-tcp"
    };

    vlcInstance = libvlc_new(3, vlc_args);

    libvlc_media_t *media = libvlc_media_new_location(
        vlcInstance,
        "rtsp://192.168.109.25:554"
        );

    mediaPlayer = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);

#ifdef Q_OS_WIN
    libvlc_media_player_set_hwnd(
        mediaPlayer,
        (void *)ui->videoWidget->winId()
        );
#endif

    libvlc_media_player_play(mediaPlayer);
    if (!ui->timelineHolder->layout()) {
        ui->timelineHolder->setLayout(new QVBoxLayout);
    }

    TimelineWidget *timeline = new TimelineWidget(ui->timelineHolder);
    ui->timelineHolder->layout()->addWidget(timeline);
}

MainWindow::~MainWindow()
{
    if (mediaPlayer) {
        libvlc_media_player_stop(mediaPlayer);
        libvlc_media_player_release(mediaPlayer);
    }

    if (vlcInstance) {
        libvlc_release(vlcInstance);
    }
    delete ui;
}
