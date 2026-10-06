#include "camera.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QDebug>

Camera::Camera(QWidget *parent) : QWidget(parent) {
    this->setWindowFlags(Qt::Window);
    
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setAutoFillBackground(true);
    setPalette(pal);
    
    label_camera = new QLabel(this);
    label_camera->setScaledContents(true);
    label_camera->setMinimumSize(640, 480);
    
    btn_back = new QPushButton("退出倒车影像", this);
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

    // ================= AI 神经接收端（只听不说） =================
    m_voiceThread = new VoiceThread(this);
    connect(m_voiceThread, &VoiceThread::commandReceived, 
            this, &Camera::onVoiceCommandExecuted, 
            Qt::QueuedConnection);
    m_voiceThread->start(); 
    // ========================================================

    v4l2Thread->startCapture();
}

Camera::~Camera() {
    if(m_voiceThread) {
        m_voiceThread->stop();
        m_voiceThread->wait();
    }
    if(v4l2Thread) {
        v4l2Thread->stopCapture();
    }
}

void Camera::onBackClicked() {
    v4l2Thread->stopCapture();   
    if (this->parentWidget()) {
        this->parentWidget()->show(); 
    }
    this->deleteLater();          
}

void Camera::onVoiceCommandExecuted(QString command) {
    if (command == "open_camera") {
        qDebug() << "[Camera UI] 听到打开摄像头，启动 V4L2...";
        this->show(); 
        v4l2Thread->startCapture(); 
    } else if (command == "close_camera") {
        qDebug() << "[Camera UI] 听到关闭摄像头，执行退出...";
        onBackClicked(); 
    }
}