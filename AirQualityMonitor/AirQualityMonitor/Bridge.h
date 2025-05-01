// Bridge.h
#pragma once

#include <QObject>

class Bridge : public QObject
{
    Q_OBJECT
public:
    explicit Bridge(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    void onMarkerClicked(const QString& stationName) {
        emit markerClicked(stationName);
    }

signals:
    void markerClicked(const QString& stationName);
};