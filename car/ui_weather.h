/********************************************************************************
** Form generated from reading UI file 'weather.ui'
**
** Created by: Qt User Interface Compiler version 5.15.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WEATHER_H
#define UI_WEATHER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Weather
{
public:
    QWidget *centralwidget;
    QPushButton *pushButton;
    QWidget *layoutWidget_4;
    QVBoxLayout *verticalLayout_5;
    QLabel *label_27;
    QLabel *label_temp;
    QLabel *label_temprange;
    QLabel *label_weather;
    QLabel *label_wind;
    QComboBox *comboBox;
    QLabel *label_updatetime;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QFrame *line_4;
    QVBoxLayout *verticalLayout;
    QLabel *label_day1;
    QLabel *label_f1_date;
    QLabel *label;
    QLabel *label_f1_range;
    QLabel *label_f1_weather;
    QLabel *label_f1_wind;
    QFrame *line_3;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_day2;
    QLabel *label_f2_date;
    QLabel *label_9;
    QLabel *label_f2_range;
    QLabel *label_f2_weather;
    QLabel *label_f2_wind;
    QFrame *line_2;
    QVBoxLayout *verticalLayout_3;
    QLabel *label_day3;
    QLabel *label_f3_date;
    QLabel *label_15;
    QLabel *label_f3_range;
    QLabel *label_f3_weather;
    QLabel *label_f3_wind;
    QFrame *line;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_day4;
    QLabel *label_f4_date;
    QLabel *label_21;
    QLabel *label_f4_range;
    QLabel *label_f4_weather;
    QLabel *label_f4_wind;
    QLabel *label_today;

    void setupUi(QMainWindow *Weather)
    {
        if (Weather->objectName().isEmpty())
            Weather->setObjectName(QString::fromUtf8("Weather"));
        Weather->resize(800, 480);
        centralwidget = new QWidget(Weather);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        pushButton = new QPushButton(centralwidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        pushButton->setGeometry(QRect(730, 420, 64, 64));
        pushButton->setStyleSheet(QString::fromUtf8("border-image: url(:/img/back.png);"));
        layoutWidget_4 = new QWidget(centralwidget);
        layoutWidget_4->setObjectName(QString::fromUtf8("layoutWidget_4"));
        layoutWidget_4->setGeometry(QRect(20, 70, 130, 400));
        verticalLayout_5 = new QVBoxLayout(layoutWidget_4);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        label_27 = new QLabel(layoutWidget_4);
        label_27->setObjectName(QString::fromUtf8("label_27"));
        label_27->setMinimumSize(QSize(128, 128));
        label_27->setStyleSheet(QString::fromUtf8("border-image: url(:/img/sunset.png);"));
        label_27->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_27);

        label_temp = new QLabel(layoutWidget_4);
        label_temp->setObjectName(QString::fromUtf8("label_temp"));
        label_temp->setMinimumSize(QSize(128, 32));
        QFont font;
        font.setPointSize(15);
        label_temp->setFont(font);
        label_temp->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_temp->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_temp);

        label_temprange = new QLabel(layoutWidget_4);
        label_temprange->setObjectName(QString::fromUtf8("label_temprange"));
        label_temprange->setMinimumSize(QSize(128, 32));
        label_temprange->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_temprange->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_temprange);

        label_weather = new QLabel(layoutWidget_4);
        label_weather->setObjectName(QString::fromUtf8("label_weather"));
        label_weather->setMinimumSize(QSize(128, 32));
        label_weather->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_weather->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_weather);

        label_wind = new QLabel(layoutWidget_4);
        label_wind->setObjectName(QString::fromUtf8("label_wind"));
        label_wind->setMinimumSize(QSize(128, 32));
        QFont font1;
        font1.setPointSize(8);
        label_wind->setFont(font1);
        label_wind->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_wind->setAlignment(Qt::AlignCenter);

        verticalLayout_5->addWidget(label_wind);

        comboBox = new QComboBox(centralwidget);
        comboBox->addItem(QString());
        comboBox->addItem(QString());
        comboBox->addItem(QString());
        comboBox->setObjectName(QString::fromUtf8("comboBox"));
        comboBox->setGeometry(QRect(20, 0, 99, 33));
        label_updatetime = new QLabel(centralwidget);
        label_updatetime->setObjectName(QString::fromUtf8("label_updatetime"));
        label_updatetime->setGeometry(QRect(200, 430, 331, 32));
        label_updatetime->setMinimumSize(QSize(128, 32));
        QFont font2;
        font2.setPointSize(11);
        label_updatetime->setFont(font2);
        label_updatetime->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_updatetime->setAlignment(Qt::AlignCenter);
        layoutWidget = new QWidget(centralwidget);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(210, 20, 590, 402));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        line_4 = new QFrame(layoutWidget);
        line_4->setObjectName(QString::fromUtf8("line_4"));
        line_4->setMinimumSize(QSize(0, 400));
        line_4->setFrameShape(QFrame::VLine);
        line_4->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_4);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label_day1 = new QLabel(layoutWidget);
        label_day1->setObjectName(QString::fromUtf8("label_day1"));
        label_day1->setMinimumSize(QSize(128, 32));
        label_day1->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_day1->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_day1);

        label_f1_date = new QLabel(layoutWidget);
        label_f1_date->setObjectName(QString::fromUtf8("label_f1_date"));
        label_f1_date->setMinimumSize(QSize(128, 32));
        label_f1_date->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f1_date->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_f1_date);

        label = new QLabel(layoutWidget);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(128, 128));
        label->setStyleSheet(QString::fromUtf8("border-image: url(:/img/sunny.png);"));
        label->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label);

        label_f1_range = new QLabel(layoutWidget);
        label_f1_range->setObjectName(QString::fromUtf8("label_f1_range"));
        label_f1_range->setMinimumSize(QSize(128, 32));
        label_f1_range->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f1_range->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_f1_range);

        label_f1_weather = new QLabel(layoutWidget);
        label_f1_weather->setObjectName(QString::fromUtf8("label_f1_weather"));
        label_f1_weather->setMinimumSize(QSize(128, 32));
        label_f1_weather->setFont(font1);
        label_f1_weather->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f1_weather->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_f1_weather);

        label_f1_wind = new QLabel(layoutWidget);
        label_f1_wind->setObjectName(QString::fromUtf8("label_f1_wind"));
        label_f1_wind->setMinimumSize(QSize(128, 32));
        label_f1_wind->setFont(font1);
        label_f1_wind->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f1_wind->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(label_f1_wind);


        horizontalLayout->addLayout(verticalLayout);

        line_3 = new QFrame(layoutWidget);
        line_3->setObjectName(QString::fromUtf8("line_3"));
        line_3->setMinimumSize(QSize(0, 400));
        line_3->setFrameShape(QFrame::VLine);
        line_3->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_3);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        label_day2 = new QLabel(layoutWidget);
        label_day2->setObjectName(QString::fromUtf8("label_day2"));
        label_day2->setMinimumSize(QSize(128, 32));
        label_day2->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_day2->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_day2);

        label_f2_date = new QLabel(layoutWidget);
        label_f2_date->setObjectName(QString::fromUtf8("label_f2_date"));
        label_f2_date->setMinimumSize(QSize(128, 32));
        label_f2_date->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f2_date->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_f2_date);

        label_9 = new QLabel(layoutWidget);
        label_9->setObjectName(QString::fromUtf8("label_9"));
        label_9->setMinimumSize(QSize(128, 128));
        label_9->setStyleSheet(QString::fromUtf8("border-image: url(:/img/cloudy_sun.png);"));
        label_9->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_9);

        label_f2_range = new QLabel(layoutWidget);
        label_f2_range->setObjectName(QString::fromUtf8("label_f2_range"));
        label_f2_range->setMinimumSize(QSize(128, 32));
        label_f2_range->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f2_range->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_f2_range);

        label_f2_weather = new QLabel(layoutWidget);
        label_f2_weather->setObjectName(QString::fromUtf8("label_f2_weather"));
        label_f2_weather->setMinimumSize(QSize(128, 32));
        label_f2_weather->setFont(font1);
        label_f2_weather->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f2_weather->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_f2_weather);

        label_f2_wind = new QLabel(layoutWidget);
        label_f2_wind->setObjectName(QString::fromUtf8("label_f2_wind"));
        label_f2_wind->setMinimumSize(QSize(128, 32));
        label_f2_wind->setFont(font1);
        label_f2_wind->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f2_wind->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label_f2_wind);


        horizontalLayout->addLayout(verticalLayout_2);

        line_2 = new QFrame(layoutWidget);
        line_2->setObjectName(QString::fromUtf8("line_2"));
        line_2->setMinimumSize(QSize(0, 400));
        line_2->setFrameShape(QFrame::VLine);
        line_2->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_2);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        label_day3 = new QLabel(layoutWidget);
        label_day3->setObjectName(QString::fromUtf8("label_day3"));
        label_day3->setMinimumSize(QSize(128, 32));
        label_day3->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_day3->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_day3);

        label_f3_date = new QLabel(layoutWidget);
        label_f3_date->setObjectName(QString::fromUtf8("label_f3_date"));
        label_f3_date->setMinimumSize(QSize(128, 32));
        label_f3_date->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f3_date->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_f3_date);

        label_15 = new QLabel(layoutWidget);
        label_15->setObjectName(QString::fromUtf8("label_15"));
        label_15->setMinimumSize(QSize(128, 128));
        label_15->setStyleSheet(QString::fromUtf8("border-image: url(:/img/cloud.png);"));
        label_15->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_15);

        label_f3_range = new QLabel(layoutWidget);
        label_f3_range->setObjectName(QString::fromUtf8("label_f3_range"));
        label_f3_range->setMinimumSize(QSize(128, 32));
        label_f3_range->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f3_range->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_f3_range);

        label_f3_weather = new QLabel(layoutWidget);
        label_f3_weather->setObjectName(QString::fromUtf8("label_f3_weather"));
        label_f3_weather->setMinimumSize(QSize(128, 32));
        label_f3_weather->setFont(font1);
        label_f3_weather->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f3_weather->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_f3_weather);

        label_f3_wind = new QLabel(layoutWidget);
        label_f3_wind->setObjectName(QString::fromUtf8("label_f3_wind"));
        label_f3_wind->setMinimumSize(QSize(128, 32));
        label_f3_wind->setFont(font1);
        label_f3_wind->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f3_wind->setAlignment(Qt::AlignCenter);

        verticalLayout_3->addWidget(label_f3_wind);


        horizontalLayout->addLayout(verticalLayout_3);

        line = new QFrame(layoutWidget);
        line->setObjectName(QString::fromUtf8("line"));
        line->setMinimumSize(QSize(0, 400));
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line);

        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        label_day4 = new QLabel(layoutWidget);
        label_day4->setObjectName(QString::fromUtf8("label_day4"));
        label_day4->setMinimumSize(QSize(128, 32));
        label_day4->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_day4->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_day4);

        label_f4_date = new QLabel(layoutWidget);
        label_f4_date->setObjectName(QString::fromUtf8("label_f4_date"));
        label_f4_date->setMinimumSize(QSize(128, 32));
        label_f4_date->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f4_date->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_f4_date);

        label_21 = new QLabel(layoutWidget);
        label_21->setObjectName(QString::fromUtf8("label_21"));
        label_21->setMinimumSize(QSize(128, 128));
        label_21->setStyleSheet(QString::fromUtf8("border-image: url(:/img/clouds_storm.png);"));
        label_21->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_21);

        label_f4_range = new QLabel(layoutWidget);
        label_f4_range->setObjectName(QString::fromUtf8("label_f4_range"));
        label_f4_range->setMinimumSize(QSize(128, 32));
        label_f4_range->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f4_range->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_f4_range);

        label_f4_weather = new QLabel(layoutWidget);
        label_f4_weather->setObjectName(QString::fromUtf8("label_f4_weather"));
        label_f4_weather->setMinimumSize(QSize(128, 32));
        label_f4_weather->setFont(font1);
        label_f4_weather->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f4_weather->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_f4_weather);

        label_f4_wind = new QLabel(layoutWidget);
        label_f4_wind->setObjectName(QString::fromUtf8("label_f4_wind"));
        label_f4_wind->setMinimumSize(QSize(128, 32));
        label_f4_wind->setFont(font1);
        label_f4_wind->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_f4_wind->setAlignment(Qt::AlignCenter);

        verticalLayout_4->addWidget(label_f4_wind);


        horizontalLayout->addLayout(verticalLayout_4);

        label_today = new QLabel(centralwidget);
        label_today->setObjectName(QString::fromUtf8("label_today"));
        label_today->setGeometry(QRect(10, 40, 201, 31));
        QFont font3;
        font3.setPointSize(7);
        label_today->setFont(font3);
        label_today->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        Weather->setCentralWidget(centralwidget);

        retranslateUi(Weather);

        QMetaObject::connectSlotsByName(Weather);
    } // setupUi

    void retranslateUi(QMainWindow *Weather)
    {
        Weather->setWindowTitle(QCoreApplication::translate("Weather", "MainWindow", nullptr));
        pushButton->setText(QString());
        label_27->setText(QString());
        label_temp->setText(QCoreApplication::translate("Weather", "\346\270\251\345\272\246: --", nullptr));
        label_temprange->setText(QCoreApplication::translate("Weather", " \346\270\251\345\272\246\350\214\203\345\233\264 --", nullptr));
        label_weather->setText(QCoreApplication::translate("Weather", "\345\244\251\346\260\224 --", nullptr));
        label_wind->setText(QCoreApplication::translate("Weather", "\351\243\216\351\200\237 --", nullptr));
        comboBox->setItemText(0, QCoreApplication::translate("Weather", "\345\271\277\345\267\236", nullptr));
        comboBox->setItemText(1, QCoreApplication::translate("Weather", "\346\267\261\345\234\263", nullptr));
        comboBox->setItemText(2, QCoreApplication::translate("Weather", "\344\270\234\350\216\236", nullptr));

        label_updatetime->setText(QCoreApplication::translate("Weather", "\346\233\264\346\226\260\346\227\266\351\227\264:  --", nullptr));
        label_day1->setText(QCoreApplication::translate("Weather", "\345\221\250 --", nullptr));
        label_f1_date->setText(QCoreApplication::translate("Weather", "\346\227\245\346\234\237 --", nullptr));
        label->setText(QString());
        label_f1_range->setText(QCoreApplication::translate("Weather", " \346\270\251\345\272\246\350\214\203\345\233\264 --", nullptr));
        label_f1_weather->setText(QCoreApplication::translate("Weather", "\345\244\251\346\260\224 --", nullptr));
        label_f1_wind->setText(QCoreApplication::translate("Weather", "\351\243\216\351\200\237 --", nullptr));
        label_day2->setText(QCoreApplication::translate("Weather", "\345\221\250 --", nullptr));
        label_f2_date->setText(QCoreApplication::translate("Weather", "\346\227\245\346\234\237 --", nullptr));
        label_9->setText(QString());
        label_f2_range->setText(QCoreApplication::translate("Weather", " \346\270\251\345\272\246\350\214\203\345\233\264 --", nullptr));
        label_f2_weather->setText(QCoreApplication::translate("Weather", "\345\244\251\346\260\224 --", nullptr));
        label_f2_wind->setText(QCoreApplication::translate("Weather", "\351\243\216\351\200\237 --", nullptr));
        label_day3->setText(QCoreApplication::translate("Weather", "\345\221\250 --", nullptr));
        label_f3_date->setText(QCoreApplication::translate("Weather", "\346\227\245\346\234\237 --", nullptr));
        label_15->setText(QString());
        label_f3_range->setText(QCoreApplication::translate("Weather", " \346\270\251\345\272\246\350\214\203\345\233\264 --", nullptr));
        label_f3_weather->setText(QCoreApplication::translate("Weather", "\345\244\251\346\260\224 --", nullptr));
        label_f3_wind->setText(QCoreApplication::translate("Weather", "\351\243\216\351\200\237 --", nullptr));
        label_day4->setText(QCoreApplication::translate("Weather", "\345\221\250 --", nullptr));
        label_f4_date->setText(QCoreApplication::translate("Weather", "\346\227\245\346\234\237 --", nullptr));
        label_21->setText(QString());
        label_f4_range->setText(QCoreApplication::translate("Weather", " \346\270\251\345\272\246\350\214\203\345\233\264 --", nullptr));
        label_f4_weather->setText(QCoreApplication::translate("Weather", "\345\244\251\346\260\224 --", nullptr));
        label_f4_wind->setText(QCoreApplication::translate("Weather", "\351\243\216\351\200\237 --", nullptr));
        label_today->setText(QCoreApplication::translate("Weather", "\346\226\260/\345\206\234\345\216\206", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Weather: public Ui_Weather {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WEATHER_H
