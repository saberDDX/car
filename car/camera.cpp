#include "camera.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QDebug>
#include <QCloseEvent>

Camera::Camera(QWidget *parent) : QWidget(parent) {
    this->setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("cameraWindow");
    
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setAutoFillBackground(true);
    setPalette(pal);
    
    label_camera = new QLabel(this);
    label_camera->setScaledContents(true);
    label_camera->setMinimumSize(640, 480);
    
    btn_back = new QPushButton("退出倒车影像", this);
    btn_back->setObjectName("cameraBackButton");
    btn_back->setStyleSheet("background-color: #AA0000; color: white; font-size: 20px; padding: 10px; border-radius: 5px;");
    
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(btn_back);
    bottomLayout->addStretch(); // 把按钮推到左边

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(label_camera, 1); 
    layout->addLayout(bottomLayout, 0); 
    setLayout(layout);

    v4l2Thread = new V4L2Thread(this);
    connect(v4l2Thread, &V4L2Thread::frameReady, this, [=](const QImage &img){
        label_camera->setPixmap(QPixmap::fromImage(img));
    });

    connect(btn_back, &QPushButton::clicked, this, &Camera::onBackClicked);
    connect(v4l2Thread, &V4L2Thread::captureError, this, [this](const QString &message) {
        label_camera->setText(message);
        label_camera->setStyleSheet("color: white;");
        qWarning().noquote() << message;
    });
    label_camera->setText("正在打开摄像头…");
    label_camera->setStyleSheet("color: white;");

    v4l2Thread->startCapture();
}

Camera::~Camera() {
    if(v4l2Thread) {
        v4l2Thread->stopCapture();
    }
}

void Camera::onBackClicked() {
    close();
}

void Camera::closeEvent(QCloseEvent *event) {
    v4l2Thread->stopCapture();
    emit closed();
    event->accept();
}
