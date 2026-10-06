#include "camera.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ailistenerthread.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //设置背景图片
    QPalette palette;
    palette.setBrush(QPalette::Background,QBrush(QPixmap(":/img/bg.jpg")));
    this->setPalette(palette);

    //cancel border line when on focus
    this->ui->pushButton->setFocusPolicy(Qt::NoFocus);
    this->ui->pushButton_2->setFocusPolicy(Qt::NoFocus);
    this->ui->pushButton_3->setFocusPolicy(Qt::NoFocus);
    this->ui->pushButton_4->setFocusPolicy(Qt::NoFocus);


     //map = new Map(this);
     //video = new Video(this);
     //music = new Music(this);
    
    // --- 独立且不冲突的倒车影像入口（手动模拟 R 挡） ---
    QPushButton *btn_reverse = new QPushButton("倒车影像", this);
    btn_reverse->setGeometry(20, 20, 150, 50); 
    btn_reverse->setStyleSheet("background-color: rgba(255, 0, 0, 180); color: white; font-weight: bold; font-size: 18px; border-radius: 5px;");
    
    connect(btn_reverse, &QPushButton::clicked, this, [=](){
        Camera *cam = new Camera(this);
        qDebug() << "Simulation: Shift to Reverse Gear!";
        this->hide(); // 隐藏主界面
        cam->show();  // 弹出倒车影像界面
    });

    // --- 新增：AI 语音控制总线接入 ---
    aiThread = new AiListenerThread(this);
    connect(aiThread, &AiListenerThread::hardwareCommandReceived, this, [=](const QString &action){
        qDebug() << ">>> 前台收到 AI 硬件控制指令: " << action;
        
        // 如果 AI 判定用户的语音意思是打开摄像头
        if (action.contains("camera_on")) {
            Camera *cam = new Camera(this);
            qDebug() << "AI Command: Voice triggered Reverse Camera!";
            this->hide(); 
            cam->show();  
        }
    });
    aiThread->start(); // 启动后台监听
}

MainWindow::~MainWindow()
{
    // 告诉 AI 监听线程：准备下班了，别再循环了
    if (aiThread) {
        aiThread->requestInterruption(); 
        aiThread->quit();
        aiThread->wait(); // 等待它把手头最后一次循环走完，安全退出
    }

    delete ui;
}

//weather
void MainWindow::on_pushButton_clicked()
{
    weather = new Weather(this);
    qDebug() << "weather";
    this->hide();
    weather->show();
}

//map
void MainWindow::on_pushButton_2_clicked()
{
    map = new Map(this);
    qDebug() << "map";
    this->hide();
    map->show();
}

//music
void MainWindow::on_pushButton_3_clicked()
{
    music = new Music(this);
    qDebug() << "music";
    this->hide();
    music->show();
}

//video
void MainWindow::on_pushButton_4_clicked()
{
    video = new Video(this);
    qDebug() << "video";
    this->hide();
    video->show();
}
