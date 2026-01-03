#pragma once

#include <QMainWindow>
#include <vlc/vlc.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    libvlc_instance_t *vlcInstance = nullptr;
    libvlc_media_player_t *mediaPlayer = nullptr;
};
