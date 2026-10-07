#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPalette>
#include <QBrush>
#include <QDebug>
#include <QPointer>
#include "music.h"
#include "video.h"
#include "map.h"
#include "weather.h"

class InputKeyReader;
class Camera;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void openCamera();

private slots:
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();

private:
    Ui::MainWindow *ui;

    Weather *weather;
    Map *map;
    Video *video;
    Music *music;

    // Physical input events share the same camera entry as the GUI button.
    InputKeyReader *keyReader;
    QPointer<Camera> cameraWindow;
};

#endif // MAINWINDOW_H
