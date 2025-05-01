/**
 * @file AirQualityMonitor.h
 * @brief G��wna klasa aplikacji monitoruj�cej jako�� powietrza.
 *
 * Aplikacja pobiera dane pomiarowe ze stacji w Polsce, prezentuje je na mapie,
 * umo�liwia analiz� danych oraz zapis i odczyt danych lokalnie.
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
  * @brief Klasa g��wnego okna aplikacji.
  *
  * Zarz�dza interfejsem u�ytkownika, obs�ug� sieci i zapisem danych.
  */
class AirQualityMonitor : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Konstruktor klasy AirQualityMonitor.
     * @param parent Wska�nik na rodzica QWidget.
     */
    AirQualityMonitor(QWidget* parent = nullptr);

    /**
     * @brief Destruktor klasy AirQualityMonitor.
     */
    ~AirQualityMonitor();

public slots:
    /**
     * @brief Obs�uguje klikni�cie w marker na mapie.
     * @param stationName Nazwa stacji.
     */
    void onMarkerClicked(const QString& stationName);
    void downloadSensorData();
    void downloadMeasurementData();

    


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
    bool isInternetAvailable();


    //void downloadSensorData();
    void onMeasurementsDownloaded();
    void onSensorsDownloaded();
    void onSensorsLoadedFromFile(int stationId);
    void updateSensorsList(const QJsonArray& sensorsData);
    void updateSensorsFile(const QJsonArray& newSensors);
    void updateMeasurementsList(const QJsonArray& measurementData);
    void updateMeasurementsFile(int sensorId, const QJsonArray& newValues);

private:
    void loadStations();
    void loadStationsFromApi();
    void loadSensorMeasurements(int sensorId);


    void saveStationsToFile(const QJsonArray& stations);
    QJsonArray loadStationsFromFile();

    void saveSensorsToFile(const QJsonArray& stations);
    QJsonArray loadSensorsFromFile();

    void loadMeasurementData(int sensorId);


    void displayMeasurementData(const QJsonArray& values);

    void saveMeasurementsToFile(const QJsonArray& allMeasurements);
    QJsonArray loadMeasurementsFromFile();
    void onMeasurementsLoadedFromFile(int sensorId);


    
    void geocodeAddress(const QString& address, double radiusKm);
    void findStationsInRadius(double centerLat, double centerLon, double radiusKm);
    void updateMapWithStations(const QVector<QJsonObject>& stations);
    double haversineDistance(double lat1, double lon1, double lat2, double lon2);
    void showAllStationsOnMap();

private:
    Ui::AirQualityMonitorClass ui; ///< Interfejs u�ytkownika.
    QNetworkAccessManager* networkManager; ///< Manager do obs�ugi zapyta� sieciowych.
    QJsonArray cachedStations; ///< Lista za�adowanych stacji.
    int currentStationId; ///< Aktualnie wybrana stacja.
    int currentSensorId; ///< Aktualnie wybrany sensor
    QMap<QString, int> sensorMap; ///< Mapa czujnik�w dla stacji.
    QJsonArray lastMeasurements; ///< Ostatnio pobrane pomiary.
    QWebChannel* channel; ///< Kana� komunikacyjny dla mapy.
    QWebEngineView* webView; ///< Widok mapy.
    Bridge* bridge; ///< Obiekt ��cz�cy map� z aplikacj�.

    QLineEdit* addressSearchBox; ///< Pole tekstowe do wpisania adresu.
    QLineEdit* radiusSearchBox; ///< Pole tekstowe do wpisania promienia.
    QPushButton* searchNearbyButton; ///< Przycisk szukania w pobli�u.
    QPushButton* showAllStationsButton; ///< Przycisk pokazania wszystkich stacji.
    QPushButton* downloadStationDetail; ///< Przycisk pobrania danych czujnik�w.
    QPushButton* downloadMeasurementButton;

};