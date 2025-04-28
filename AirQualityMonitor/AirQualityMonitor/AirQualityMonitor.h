/**
 * @file AirQualityMonitor.h
 * @brief G³ówna klasa aplikacji monitoruj¹cej jakoœæ powietrza.
 *
 * Aplikacja pobiera dane pomiarowe ze stacji w Polsce, prezentuje je na mapie,
 * umo¿liwia analizê danych oraz zapis i odczyt danych lokalnie.
 */

#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_AirQualityMonitor.h"
#include "Bridge.h"
#include <QNetworkAccessManager>
#include <QJsonArray>
#include <QMap>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QWebChannel>

 /**
  * @class AirQualityMonitor
  * @brief Klasa g³ównego okna aplikacji.
  *
  * Zarz¹dza interfejsem u¿ytkownika, obs³ug¹ sieci i zapisem danych.
  */
class AirQualityMonitor : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Konstruktor klasy AirQualityMonitor.
     * @param parent WskaŸnik na rodzica QWidget.
     */
    AirQualityMonitor(QWidget* parent = nullptr);

    /**
     * @brief Destruktor klasy AirQualityMonitor.
     */
    ~AirQualityMonitor();

public slots:
    /**
     * @brief Obs³uguje klikniêcie w marker na mapie.
     * @param stationName Nazwa stacji.
     */
    void onMarkerClicked(const QString& stationName);

private slots:
    void filterStations(const QString& text);
    void showStationListView();
    void showStationDetails(QListWidgetItem* item);
    void showSensorDetails(QListWidgetItem* item);
    void onStationsFinished();
    void onSensorsFinished();
    void onMeasurementDataFinished();
    void updateMeasurementDisplay();
    void loadMap();
    void onSearchNearbyClicked();

    void downloadSensorData();
    void onSensorsDownloaded();
    void onSensorsLoadedFromFile(int stationId);
    void updateSensorsList(const QJsonArray& sensorsData);
    void updateSensorsFile(const QJsonArray& newSensors);

private:
    void loadStations();
    void loadStationsFromApi();

    void saveStationsToFile(const QJsonArray& stations);
    QJsonArray loadStationsFromFile();

    void saveSensorsToFile(const QJsonArray& stations);
    QJsonArray loadSensorsFromFile();

    void loadMeasurementData(int sensorId);
    void displayMeasurementData(const QJsonArray& values);

    void geocodeAddress(const QString& address, double radiusKm);
    void findStationsInRadius(double centerLat, double centerLon, double radiusKm);
    void updateMapWithStations(const QVector<QJsonObject>& stations);
    double haversineDistance(double lat1, double lon1, double lat2, double lon2);
    void showAllStationsOnMap();

private:
    Ui::AirQualityMonitorClass ui; ///< Interfejs u¿ytkownika.
    QNetworkAccessManager* networkManager; ///< Manager do obs³ugi zapytañ sieciowych.
    QJsonArray cachedStations; ///< Lista za³adowanych stacji.
    int currentStationId; ///< Aktualnie wybrana stacja.
    QMap<QString, int> sensorMap; ///< Mapa czujników dla stacji.
    QJsonArray lastMeasurements; ///< Ostatnio pobrane pomiary.
    QWebChannel* channel; ///< Kana³ komunikacyjny dla mapy.
    QWebEngineView* webView; ///< Widok mapy.
    Bridge* bridge; ///< Obiekt ³¹cz¹cy mapê z aplikacj¹.

    QLineEdit* addressSearchBox; ///< Pole tekstowe do wpisania adresu.
    QLineEdit* radiusSearchBox; ///< Pole tekstowe do wpisania promienia.
    QPushButton* searchNearbyButton; ///< Przycisk szukania w pobli¿u.
    QPushButton* showAllStationsButton; ///< Przycisk pokazania wszystkich stacji.
    QPushButton* downloadStationDetail; ///< Przycisk pobrania danych czujników.
};
