#pragma once

#include <QObject>
#include <QJsonArray>

class MeasurementProcessor : public QObject
{
    Q_OBJECT

public:
    explicit MeasurementProcessor(QObject* parent = nullptr);
    void displayMeasurementData(const QJsonArray& data); // Funkcja wyœwietlania danych pomiarowych

    // Pomocnicze metody do analizy danych pomiarowych
    double calculateMin(const QJsonArray& data);
    double calculateMax(const QJsonArray& data);
    double calculateAvg(const QJsonArray& data);
    QString calculateTrend(const QJsonArray& data);
};
