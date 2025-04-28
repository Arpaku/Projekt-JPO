#include "AirQualityMonitor.h"
#include "station.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AirQualityMonitor w;
    w.show();
    return a.exec();
}
