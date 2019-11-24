#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QRandomGenerator>

#include "go.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    auto c = new go::Channel<quint32>();
    go::go([](){
        qDebug() << "a > starting";
        for (;;) {
            qDebug() << " a > !";
            go::sleep(2000);
        }
    });

    go::go([](){
        qDebug() << "b > starting";
        for (;;) {
            qDebug() << " b > !";
            go::sleep(3000);
        }
    });

    go::go([c](){
        qDebug() << "c > starting";
        for (;;) {
            auto val = QRandomGenerator::system()->generate();
            qDebug() << " c >" << val;
            *c << val;
            go::sleep(5000);
        }
    });



    go::go([c](){
        qDebug() << "d > starting";
        for (;;) {
            quint32 val;
            *c >> val;
            qDebug() << " d <" << val;
        }
    });

    go::go([c](){
        qDebug() << "e > starting";
        for (;;) {
            auto val = QRandomGenerator::system()->generate();
            qDebug() << " e >" << val;
            *c << val;
            go::sleep(4000);
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

