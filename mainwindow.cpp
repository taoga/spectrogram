#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_pCentralWgt = new CCentralWidget( this ); // Должен удалиться при закрытии окна
    setCentralWidget( m_pCentralWgt );
}

MainWindow::~MainWindow()
{
    delete ui;
}

