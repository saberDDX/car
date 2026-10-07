#ifndef CAMERA_H
#define CAMERA_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "v4l2thread.h" 
class QCloseEvent;

class Camera : public QWidget {//public QWidget：前台展示的“门面”
    Q_OBJECT
public:
    explicit Camera(QWidget *parent = nullptr);
    ~Camera();

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onBackClicked(); // 退出按钮的槽函数

private:
    QLabel *label_camera;
    QPushButton *btn_back;
    V4L2Thread *v4l2Thread;
};

#endif // CAMERA_H
