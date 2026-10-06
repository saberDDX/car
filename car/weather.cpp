// weather.cpp
#include "weather.h"
#include "ui_weather.h"

// 星期数组，0对应星期日，1-6对应星期一至星期六
QString week[7] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};

weather_t today_weather = {"","","","","","","",""};
weather_t feature1_weather;
weather_t feature2_weather;
weather_t feature3_weather;
weather_t feature4_weather;

// 和风天气 API 配置
const QString API_HOST = "nb3v5bypn9.re.qweatherapi.com";
const QString API_KEY = "5cc23c60c3114a12b82e6a5b15d07019";

// 城市名到 Location ID 的映射
QString getCityId(const QString &cityName) {
    if (cityName == "广州") return "101280101";
    if (cityName == "深圳") return "101280601";
    if (cityName == "东莞") return "101281601";
    return "101280101"; // 默认兜底
}

Weather::Weather(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Weather)
{
    ui->setupUi(this);
    this->ui->pushButton->setFocusPolicy(Qt::NoFocus);

    // 设置背景颜色
    QPalette palette;
    palette.setColor(QPalette::Background, QColor(97, 111, 125));
    this->setPalette(palette);

    // 新建网络请求管理器
    manager  = new QNetworkAccessManager(this);
    manager2 = new QNetworkAccessManager(this);

    // 关联管理器的 finished 信号
    connect(manager, &QNetworkAccessManager::finished, this, &Weather::replyFinished);
    connect(manager2, &QNetworkAccessManager::finished, this, &Weather::replyFinished2);

    // 自动获取系统当前日期并更新顶部星期标签
    QDate current = QDate::currentDate();
    int dayOfWeek = current.dayOfWeek(); // 1=星期一, 7=星期日
    
    ui->label_today->setText(QString("%1月%2日 %3").arg(current.month()).arg(current.day()).arg(week[dayOfWeek % 7]));
    ui->label_day1->setText(week[(dayOfWeek + 1) % 7]);
    ui->label_day2->setText(week[(dayOfWeek + 2) % 7]);
    ui->label_day3->setText(week[(dayOfWeek + 3) % 7]);
    ui->label_day4->setText(week[(dayOfWeek + 4) % 7]);

    // 初始化时主动请求一次当前选中的城市
    on_comboBox_currentTextChanged(ui->comboBox->currentText());
}

Weather::~Weather()
{
    delete ui;
}

// 返回主界面
void Weather::on_pushButton_clicked()
{
    this->hide();
    this->parentWidget()->show();
}

// 下拉框的内容改变了就触发
void Weather::on_comboBox_currentTextChanged(const QString &arg)
{
    city = arg;
    QString locId = getCityId(arg);

    // 1. 请求实时天气 (now)
    QString nowUrl = QString("https://%1/v7/weather/now?location=%2&key=%3")
                        .arg(API_HOST).arg(locId).arg(API_KEY);
    manager->get(QNetworkRequest(QUrl(nowUrl)));

    // 2. 请求7天预报 (7d)
    QString forecastUrl = QString("https://%1/v7/weather/7d?location=%2&key=%3")
                             .arg(API_HOST).arg(locId).arg(API_KEY);
    manager2->get(QNetworkRequest(QUrl(forecastUrl)));
}

// ---------------- 解析实时天气 ----------------
void Weather::replyFinished(QNetworkReply *reply)
{
    QString msg = reply->readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
    
    if (error.error == QJsonParseError::NoError) {
        QJsonObject root = doc.object();
        if (root.value("code").toString() == "200") {
            QJsonObject now = root.value("now").toObject();
            
            today_weather.temp = now.value("temp").toString();
            today_weather.weather = now.value("text").toString();
            today_weather.wind = now.value("windDir").toString();
            today_weather.wind_scale = now.value("windScale").toString();
            today_weather.wind_speed = now.value("windSpeed").toString();
            
            QString obsTime = now.value("obsTime").toString();
            today_weather.update_time = obsTime.mid(11, 5); // 截取 HH:mm 时间部分

            ui->label_temp->setText(QString("温度: %1°C").arg(today_weather.temp));
            ui->label_weather->setText(today_weather.weather);
            ui->label_wind->setText(QString("%1 %2级 %3km/h").arg(today_weather.wind).arg(today_weather.wind_scale).arg(today_weather.wind_speed));
            ui->label_updatetime->setText(QString("更新时间: %1").arg(today_weather.update_time));
        }
    }
    reply->deleteLater();
}

// ---------------- 解析多日预报 ----------------
void Weather::replyFinished2(QNetworkReply *reply)
{
    QString msg = reply->readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
    
    if (error.error == QJsonParseError::NoError) {
        QJsonObject root = doc.object();
        if (root.value("code").toString() == "200") {
            QJsonArray daily = root.value("daily").toArray();
            
            // 索引 0: 今天的数据 (补全今日最低/最高温)
            if (daily.size() > 0) {
                QJsonObject d0 = daily[0].toObject();
                today_weather.min_temp = d0.value("tempMin").toString();
                today_weather.max_temp = d0.value("tempMax").toString();
                ui->label_temprange->setText(QString("%1 ~ %2°C").arg(today_weather.min_temp).arg(today_weather.max_temp));
            }

            // 索引 1: 明天
            if (daily.size() > 1) {
                QJsonObject d1 = daily[1].toObject();
                ui->label_f1_date->setText(d1.value("fxDate").toString().mid(5, 5)); // 截取 MM-dd
                ui->label_f1_range->setText(QString("%1 ~ %2°C").arg(d1.value("tempMin").toString()).arg(d1.value("tempMax").toString()));
                ui->label_f1_weather->setText(d1.value("textDay").toString());
                ui->label_f1_wind->setText(QString("%1 %2级").arg(d1.value("windDirDay").toString()).arg(d1.value("windScaleDay").toString()));
            }

            // 索引 2: 后天
            if (daily.size() > 2) {
                QJsonObject d2 = daily[2].toObject();
                ui->label_f2_date->setText(d2.value("fxDate").toString().mid(5, 5));
                ui->label_f2_range->setText(QString("%1 ~ %2°C").arg(d2.value("tempMin").toString()).arg(d2.value("tempMax").toString()));
                ui->label_f2_weather->setText(d2.value("textDay").toString());
                ui->label_f2_wind->setText(QString("%1 %2级").arg(d2.value("windDirDay").toString()).arg(d2.value("windScaleDay").toString()));
            }

            // 索引 3: 大后天
            if (daily.size() > 3) {
                QJsonObject d3 = daily[3].toObject();
                ui->label_f3_date->setText(d3.value("fxDate").toString().mid(5, 5));
                ui->label_f3_range->setText(QString("%1 ~ %2°C").arg(d3.value("tempMin").toString()).arg(d3.value("tempMax").toString()));
                ui->label_f3_weather->setText(d3.value("textDay").toString());
                ui->label_f3_wind->setText(QString("%1 %2级").arg(d3.value("windDirDay").toString()).arg(d3.value("windScaleDay").toString()));
            }

            // 索引 4: 第四天
            if (daily.size() > 4) {
                QJsonObject d4 = daily[4].toObject();
                ui->label_f4_date->setText(d4.value("fxDate").toString().mid(5, 5));
                ui->label_f4_range->setText(QString("%1 ~ %2°C").arg(d4.value("tempMin").toString()).arg(d4.value("tempMax").toString()));
                ui->label_f4_weather->setText(d4.value("textDay").toString());
                ui->label_f4_wind->setText(QString("%1 %2级").arg(d4.value("windDirDay").toString()).arg(d4.value("windScaleDay").toString()));
            }
        }
    }
    reply->deleteLater();
}