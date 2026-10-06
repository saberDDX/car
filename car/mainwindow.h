#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPalette>
#include <QBrush>
#include <QDebug>
#include "music.h"
#include "video.h"
#include "map.h"
#include "weather.h"

// 提前声明 AI 监听线程类
class AiListenerThread;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

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

    // 新增：AI 监听线程指针
    AiListenerThread *aiThread = nullptr;
};

#endif // MAINWINDOW_H
