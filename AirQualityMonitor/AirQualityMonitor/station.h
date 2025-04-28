#ifndef STATION_H
#define STATION_H

#include <QObject>

class Station : public QObject
{
    Q_OBJECT
        Q_PROPERTY(QString name READ name CONSTANT)
        Q_PROPERTY(double latitude READ latitude CONSTANT)
        Q_PROPERTY(double longitude READ longitude CONSTANT)

public:
    Station(const QString& name, double latitude, double longitude, QObject* parent = nullptr)
        : QObject(parent), m_name(name), m_latitude(latitude), m_longitude(longitude) {
    }

    QString name() const { return m_name; }
    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }

private:
    QString m_name;
    double m_latitude;
    double m_longitude;
};

#endif // STATION_H
