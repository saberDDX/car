// weather.h
#ifndef WEATHER_H
#define WEATHER_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDebug>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDate>

namespace Ui {
class Weather;
}

class Weather : public QMainWindow
{
    Q_OBJECT

public:
    explicit Weather(QWidget *parent = 0);
    ~Weather();

public slots:
    void on_pushButton_clicked();
    void replyFinished(QNetworkReply *reply);
    void replyFinished2(QNetworkReply *reply);

private slots:
    void on_comboBox_currentTextChanged(const QString &arg1);

private:
    Ui::Weather *ui;

    QNetworkAccessManager *manager;
    QNetworkAccessManager *manager2;

    QString city;
};

//天气结构体
typedef struct
{
    QString update_time;
    QString weather;
    QString temp;
    QString min_temp;
    QString max_temp;
    QString wind;
    QString wind_speed;
    QString wind_scale;
} weather_t;

#endif // WEATHER_H