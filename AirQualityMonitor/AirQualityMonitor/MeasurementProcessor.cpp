#include "MeasurementProcessor.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QList>
#include <numeric>

MeasurementProcessor::MeasurementProcessor(QObject* parent) : QObject(parent) {}

void MeasurementProcessor::displayMeasurementData(const QJsonArray& data)
{
    if (data.isEmpty()) {
        // Przyk³ad obs³ugi braku danych
        return;
    }

    // Obliczanie danych
    double min = calculateMin(data);
    double max = calculateMax(data);
    double avg = calculateAvg(data);
    QString trend = calculateTrend(data);

    // Wyœwietlanie danych, np. w QLabel (zak³adaj¹c, ¿e masz odpowiednie etykiety w UI)
    // np. `ui->minValueLabel->setText(...)` (na przyk³adzie UI z poprzedniego kodu)
    // uzupe³niaj¹c odpowiednie wartoœci w interfejsie u¿ytkownika.
}

double MeasurementProcessor::calculateMin(const QJsonArray& data)
{
    double min = std::numeric_limits<double>::max();
    for (const QJsonValue& value : data) {
        QJsonObject obj = value.toObject();
        double val = obj.value("value").toDouble();
        min = std::min(min, val);
    }
    return min;
}

double MeasurementProcessor::calculateMax(const QJsonArray& data)
{
    double max = std::numeric_limits<double>::lowest();
    for (const QJsonValue& value : data) {
        QJsonObject obj = value.toObject();
        double val = obj.value("value").toDouble();
        max = std::max(max, val);
    }
    return max;
}

double MeasurementProcessor::calculateAvg(const QJsonArray& data)
{
    double sum = 0.0;
    int count = 0;
    for (const QJsonValue& value : data) {
        QJsonObject obj = value.toObject();
        sum += obj.value("value").toDouble();
        ++count;
    }
    return count > 0 ? sum / count : 0.0;
}

QString MeasurementProcessor::calculateTrend(const QJsonArray& data)
{
    QList<double> values;
    for (const QJsonValue& value : data) {
        QJsonObject obj = value.toObject();
        values.append(obj.value("value").toDouble());
    }

    int size = values.size();
    if (size <= 1) {
        return "Brak danych do analizy trendu";
    }

    // Liczymy œredni¹ dla dwóch po³ówek danych
    double sumFirst = 0, sumLast = 0;
    for (int i = 0; i < size / 2; ++i) {
        sumFirst += values[i];
    }
    for (int i = size / 2; i < size; ++i) {
        sumLast += values[i];
    }

    double avgFirst = sumFirst / (size / 2);
    double avgLast = sumLast / (size - size / 2);

    if (avgLast > avgFirst) {
        return "Rosn¹cy";
    }
    else if (avgLast < avgFirst) {
        return "Malej¹cy";
    }
    else {
        return "Stabilny";
    }
}
