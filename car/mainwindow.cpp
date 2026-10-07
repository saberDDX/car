#include "camera.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "inputkeyreader.h"
#include <QStatusBar>

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
    
    connect(btn_reverse, &QPushButton::clicked, this, &MainWindow::openCamera);
    keyReader = new InputKeyReader(this);
    connect(keyReader, &InputKeyReader::cameraRequested, this, &MainWindow::openCamera);
    statusBar()->showMessage("按键未连接；可点击倒车影像打开摄像头");
    connect(keyReader, &InputKeyReader::connectionChanged, this, [this](bool connected) {
        statusBar()->showMessage(connected ? "按键已连接：K1 打开摄像头" : "按键未连接；可点击倒车影像打开摄像头");
    });
    keyReader->start();
}

MainWindow::~MainWindow()
{
    delete cameraWindow.data();
    delete ui;
}

void MainWindow::openCamera()
{
    if (!cameraWindow) {
        cameraWindow = new Camera(this);
        connect(cameraWindow.data(), &Camera::closed, this, [this] {
            cameraWindow = nullptr;
            show();
        });
    }
    hide();
    cameraWindow->show();
    cameraWindow->raise();
    cameraWindow->activateWindow();
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
