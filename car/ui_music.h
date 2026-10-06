/********************************************************************************
** Form generated from reading UI file 'music.ui'
**
** Created by: Qt User Interface Compiler version 5.15.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MUSIC_H
#define UI_MUSIC_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Music
{
public:
    QWidget *centralwidget;
    QPushButton *pushButton;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *pushButton_6;
    QPushButton *pushButton_4;
    QPushButton *pushButton_2;
    QPushButton *pushButton_3;
    QPushButton *pushButton_5;
    QWidget *layoutWidget_2;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_current_pos;
    QSlider *horizontalSlider_3;
    QLabel *label_total_length;
    QListWidget *listWidget;
    QWidget *layoutWidget_4;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label;
    QSlider *horizontalSlider;
    QLabel *label_volume_percent;
    QLabel *label_lrc;

    void setupUi(QMainWindow *Music)
    {
        if (Music->objectName().isEmpty())
            Music->setObjectName(QString::fromUtf8("Music"));
        Music->resize(800, 480);
        Music->setStyleSheet(QString::fromUtf8(""));
        centralwidget = new QWidget(Music);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        pushButton = new QPushButton(centralwidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));
        pushButton->setGeometry(QRect(730, 410, 64, 64));
        pushButton->setStyleSheet(QString::fromUtf8("border-image: url(:/img/back.png);"));
        layoutWidget = new QWidget(centralwidget);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(290, 410, 441, 66));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        pushButton_6 = new QPushButton(layoutWidget);
        pushButton_6->setObjectName(QString::fromUtf8("pushButton_6"));
        pushButton_6->setMinimumSize(QSize(64, 64));
        pushButton_6->setMaximumSize(QSize(64, 64));
        pushButton_6->setStyleSheet(QString::fromUtf8("border-image: url(:/img/fast-backward.png);"));

        horizontalLayout->addWidget(pushButton_6);

        pushButton_4 = new QPushButton(layoutWidget);
        pushButton_4->setObjectName(QString::fromUtf8("pushButton_4"));
        pushButton_4->setMinimumSize(QSize(64, 64));
        pushButton_4->setMaximumSize(QSize(64, 64));
        pushButton_4->setStyleSheet(QString::fromUtf8("border-image: url(:/img/skip-back.png);"));

        horizontalLayout->addWidget(pushButton_4);

        pushButton_2 = new QPushButton(layoutWidget);
        pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));
        pushButton_2->setMinimumSize(QSize(64, 64));
        pushButton_2->setMaximumSize(QSize(64, 64));
        pushButton_2->setStyleSheet(QString::fromUtf8("border-image: url(:/img/play.png);"));

        horizontalLayout->addWidget(pushButton_2);

        pushButton_3 = new QPushButton(layoutWidget);
        pushButton_3->setObjectName(QString::fromUtf8("pushButton_3"));
        pushButton_3->setMinimumSize(QSize(64, 64));
        pushButton_3->setMaximumSize(QSize(64, 64));
        pushButton_3->setStyleSheet(QString::fromUtf8("border-image: url(:/img/skip-forward.png);"));

        horizontalLayout->addWidget(pushButton_3);

        pushButton_5 = new QPushButton(layoutWidget);
        pushButton_5->setObjectName(QString::fromUtf8("pushButton_5"));
        pushButton_5->setMinimumSize(QSize(64, 64));
        pushButton_5->setMaximumSize(QSize(64, 64));
        pushButton_5->setStyleSheet(QString::fromUtf8("border-image: url(:/img/fast-forward.png);"));

        horizontalLayout->addWidget(pushButton_5);

        layoutWidget_2 = new QWidget(centralwidget);
        layoutWidget_2->setObjectName(QString::fromUtf8("layoutWidget_2"));
        layoutWidget_2->setGeometry(QRect(0, 380, 812, 31));
        horizontalLayout_4 = new QHBoxLayout(layoutWidget_2);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(0, 0, 0, 0);
        label_current_pos = new QLabel(layoutWidget_2);
        label_current_pos->setObjectName(QString::fromUtf8("label_current_pos"));
        label_current_pos->setMinimumSize(QSize(80, 0));
        label_current_pos->setMaximumSize(QSize(80, 16777215));
        label_current_pos->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        label_current_pos->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_4->addWidget(label_current_pos);

        horizontalSlider_3 = new QSlider(layoutWidget_2);
        horizontalSlider_3->setObjectName(QString::fromUtf8("horizontalSlider_3"));
        horizontalSlider_3->setMinimumSize(QSize(620, 0));
        horizontalSlider_3->setMaximumSize(QSize(620, 16777215));
        horizontalSlider_3->setOrientation(Qt::Horizontal);

        horizontalLayout_4->addWidget(horizontalSlider_3);

        label_total_length = new QLabel(layoutWidget_2);
        label_total_length->setObjectName(QString::fromUtf8("label_total_length"));
        label_total_length->setMinimumSize(QSize(80, 0));
        label_total_length->setMaximumSize(QSize(80, 16777215));
        label_total_length->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));

        horizontalLayout_4->addWidget(label_total_length);

        listWidget = new QListWidget(centralwidget);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));
        listWidget->setGeometry(QRect(0, 0, 160, 381));
        listWidget->setMinimumSize(QSize(160, 0));
        listWidget->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));
        layoutWidget_4 = new QWidget(centralwidget);
        layoutWidget_4->setObjectName(QString::fromUtf8("layoutWidget_4"));
        layoutWidget_4->setGeometry(QRect(0, 430, 281, 33));
        horizontalLayout_3 = new QHBoxLayout(layoutWidget_4);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(layoutWidget_4);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(42, 0));
        label->setMaximumSize(QSize(42, 32));
        label->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));

        horizontalLayout_3->addWidget(label);

        horizontalSlider = new QSlider(layoutWidget_4);
        horizontalSlider->setObjectName(QString::fromUtf8("horizontalSlider"));
        horizontalSlider->setMinimumSize(QSize(160, 0));
        horizontalSlider->setMaximumSize(QSize(160, 16777215));
        horizontalSlider->setOrientation(Qt::Horizontal);

        horizontalLayout_3->addWidget(horizontalSlider);

        label_volume_percent = new QLabel(layoutWidget_4);
        label_volume_percent->setObjectName(QString::fromUtf8("label_volume_percent"));
        label_volume_percent->setMinimumSize(QSize(60, 0));
        label_volume_percent->setMaximumSize(QSize(60, 16777215));
        label_volume_percent->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);"));

        horizontalLayout_3->addWidget(label_volume_percent);

        label_lrc = new QLabel(centralwidget);
        label_lrc->setObjectName(QString::fromUtf8("label_lrc"));
        label_lrc->setGeometry(QRect(300, 140, 391, 51));
        QFont font;
        font.setPointSize(12);
        label_lrc->setFont(font);
        label_lrc->setStyleSheet(QString::fromUtf8("color: rgb(255, 0, 0);"));
        label_lrc->setAlignment(Qt::AlignCenter);
        Music->setCentralWidget(centralwidget);

        retranslateUi(Music);

        listWidget->setCurrentRow(-1);


        QMetaObject::connectSlotsByName(Music);
    } // setupUi

    void retranslateUi(QMainWindow *Music)
    {
        Music->setWindowTitle(QCoreApplication::translate("Music", "MainWindow", nullptr));
        pushButton->setText(QString());
        pushButton_6->setText(QString());
        pushButton_4->setText(QString());
        pushButton_2->setText(QString());
        pushButton_3->setText(QString());
        pushButton_5->setText(QString());
        label_current_pos->setText(QCoreApplication::translate("Music", "\347\247\222\346\225\260", nullptr));
        label_total_length->setText(QCoreApplication::translate("Music", "\346\227\266\351\225\277", nullptr));
        label->setText(QCoreApplication::translate("Music", "\351\237\263\351\207\217", nullptr));
        label_volume_percent->setText(QCoreApplication::translate("Music", "100%", nullptr));
        label_lrc->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class Music: public Ui_Music {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MUSIC_H
