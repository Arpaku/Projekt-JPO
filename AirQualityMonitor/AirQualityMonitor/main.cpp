#include "AirQualityMonitor.h"
#include <QtWidgets/QApplication>
#include "NetworkTest.cpp"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AirQualityMonitor w;
    w.show();
    return a.exec();
}
