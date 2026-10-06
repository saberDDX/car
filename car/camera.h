#ifndef CAMERA_H
#define CAMERA_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "v4l2thread.h" 
#include "VoiceThread.h"

class Camera : public QWidget {//public QWidget：前台展示的“门面”
    Q_OBJECT
public:
    explicit Camera(QWidget *parent = nullptr);
    ~Camera();

private slots:
    void onBackClicked(); // 退出按钮的槽函数
    void onVoiceCommandExecuted(QString command);

private:
    QLabel *label_camera;
    QPushButton *btn_back;
    V4L2Thread *v4l2Thread;
    VoiceThread *m_voiceThread;
};

#endif // CAMERA_H