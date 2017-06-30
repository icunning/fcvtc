#include "mainwindow.h"
#include <QApplication>

double blackLineDistancem = 138.;


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
