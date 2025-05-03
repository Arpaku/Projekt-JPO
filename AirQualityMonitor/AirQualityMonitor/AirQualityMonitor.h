/**
 * @file AirQualityMonitor.h
 * @brief Główna klasa aplikacji monitorującej jakość powietrza.
 *
 * Aplikacja pobiera dane pomiarowe ze stacji w Polsce, prezentuje je na mapie,
 * umożliwia analizę danych oraz zapis i odczyt danych lokalnie.
 *
 * @author Twoje Imię
 * @date Maj 2025
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
  * @brief Klasa głównego okna aplikacji monitorującej jakość powietrza.
  *
  * Zarządza interfejsem użytkownika, komunikacją sieciową, zarządzaniem danymi
  * i wizualizacją informacji o jakości powietrza.
  */
class AirQualityMonitor : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Konstruktor klasy AirQualityMonitor.
     * @param parent Wskaźnik na rodzica widgetu (opcjonalny).
     */
    AirQualityMonitor(QWidget* parent = nullptr);

    /**
     * @brief Destruktor klasy AirQualityMonitor.
     */
    ~AirQualityMonitor();

public slots:
    /**
     * @brief Obsługuje kliknięcie w marker na mapie.
     * @param stationName Nazwa stacji, która została kliknięta.
     */
    void onMarkerClicked(const QString& stationName);

    /**
     * @brief Pobiera i zapisuje dane sensorów dla aktualnie wybranej stacji.
     *
     * Pobiera informacje o sensorach z API GIOŚ i zapisuje je lokalnie.
     * Jeśli dane są już dostępne lokalnie, nie pobiera ich ponownie.
     */
    void downloadSensorData();

    /**
     * @brief Pobiera i zapisuje dane pomiarowe dla aktualnie wybranego sensora.
     *
     * Pobiera dane pomiarowe z API GIOŚ dla wybranego sensora i zapisuje je lokalnie.
     * Jeśli nie ma połączenia z internetem, próbuje użyć lokalnie zapisanych danych.
     */
    void downloadMeasurementData();

private slots:
    /**
     * @brief Filtruje listę stacji na podstawie tekstu wyszukiwania.
     * @param text Tekst, według którego filtrowane są stacje.
     */
    void filterStations(const QString& text);

    /**
     * @brief Pokazuje główny widok listy stacji.
     */
    void showStationListView();

    /**
     * @brief Wyświetla szczegóły wybranej stacji.
     * @param item Wybrany element listy stacji.
     */
    void showStationDetails(QListWidgetItem* item);

    /**
     * @brief Wyświetla szczegóły wybranego sensora.
     * @param item Wybrany element listy sensorów.
     */
    void showSensorDetails(QListWidgetItem* item);

    /**
     * @brief Obsługuje zakończenie pobierania danych stacji.
     *
     * Przetwarza dane stacji po zakończeniu żądania sieciowego.
     */
    void onStationsFinished();

    /**
     * @brief Obsługuje zakończenie pobierania danych sensorów.
     *
     * Przetwarza dane sensorów po zakończeniu żądania sieciowego.
     */
    void onSensorsFinished();

    /**
     * @brief Obsługuje zakończenie pobierania danych pomiarowych.
     *
     * Przetwarza dane pomiarowe po zakończeniu żądania sieciowego.
     */
    void onMeasurementDataFinished();

    /**
     * @brief Aktualizuje wyświetlanie wykresu i statystyk pomiarów.
     *
     * Aktualizuje wykres na podstawie wybranego zakresu dat.
     */
    void updateMeasurementDisplay();

    /**
     * @brief Ładuje interfejs mapy.
     *
     * Tworzy i inicjalizuje interfejs OpenStreetMap.
     */
    void loadMap();

    /**
     * @brief Obsługuje kliknięcie przycisku "Szukaj w pobliżu".
     *
     * Wyszukuje stacje w pobliżu określonego adresu w podanym promieniu.
     */
    void onSearchNearbyClicked();

    /**
     * @brief Sprawdza czy połączenie z internetem jest dostępne.
     * @return True jeśli internet jest dostępny, false w przeciwnym razie.
     */
    bool isInternetAvailable();

    /**
     * @brief Obsługuje zakończenie pobierania danych sensorów.
     *
     * Przetwarza dane sensorów po zakończeniu żądania sieciowego i zapisuje je.
     */
    void onSensorsDownloaded();

    /**
     * @brief Obsługuje zakończenie pobierania danych pomiarowych.
     *
     * Przetwarza dane pomiarowe po zakończeniu żądania sieciowego i zapisuje je.
     */
    void onMeasurementsDownloaded();

    /**
     * @brief Ładuje dane sensorów z pliku lokalnego dla stacji.
     * @param stationId ID stacji, dla której ładowane są sensory.
     */
    void onSensorsLoadedFromFile(int stationId);

    /**
     * @brief Aktualizuje interfejs użytkownika danymi sensorów dla stacji.
     * @param sensorsData Tablica JSON z danymi sensorów.
     */
    void updateSensorsList(const QJsonArray& sensorsData);

    /**
     * @brief Aktualizuje lokalny plik sensorów nowymi danymi.
     * @param newSensors Tablica JSON z nowymi danymi sensorów.
     */
    void updateSensorsFile(const QJsonArray& newSensors);

    /**
     * @brief Aktualizuje interfejs użytkownika danymi pomiarowymi.
     * @param measurementData Tablica JSON z danymi pomiarowymi.
     */
    void updateMeasurementsList(const QJsonArray& measurementData);

    /**
     * @brief Aktualizuje lokalny plik pomiarów nowymi danymi.
     * @param sensorId ID sensora, który jest aktualizowany.
     * @param newValues Tablica JSON z nowymi wartościami pomiarów.
     */
    void updateMeasurementsFile(int sensorId, const QJsonArray& newValues);

private:
    // ===== FUNKCJE INICJALIZACYJNE I PODSTAWOWE =====

    /**
     * @brief Łączy sygnały interfejsu użytkownika z odpowiednimi slotami.
     *
     * Konfiguruje połączenia między elementami UI a funkcjami obsługi.
     */
    void connectSignalsAndSlots();

    /**
     * @brief Konfiguruje widok webowy dla mapy.
     *
     * Inicjalizuje komponent QWebEngineView i most komunikacyjny z JavaScript.
     */
    void setupWebView();

    // ===== FUNKCJE ZARZĄDZANIA STACJAMI =====

    /**
     * @brief Ładuje dane stacji z pliku lokalnego lub API.
     *
     * Sprawdza najpierw pliki lokalne, a jeśli potrzeba, pobiera z API.
     */
    void loadStations();

    /**
     * @brief Ładuje dane stacji z API GIOŚ.
     *
     * Używa wielowątkowości, aby uniknąć blokowania interfejsu użytkownika.
     */
    void loadStationsFromApi();

    /**
     * @brief Zapisuje dane stacji do lokalnego pliku JSON.
     * @param stations Tablica JSON z danymi stacji do zapisania.
     */
    void saveStationsToFile(const QJsonArray& stations);

    /**
     * @brief Ładuje dane stacji z lokalnego pliku JSON.
     * @return Tablica JSON z danymi stacji.
     */
    QJsonArray loadStationsFromFile();

    // ===== FUNKCJE ZARZĄDZANIA SENSORAMI =====

    /**
     * @brief Zapisuje dane sensorów do lokalnego pliku JSON.
     * @param sensors Tablica JSON z danymi sensorów do zapisania.
     */
    void saveSensorsToFile(const QJsonArray& sensors);

    /**
     * @brief Ładuje dane sensorów z lokalnego pliku JSON.
     * @return Tablica JSON z danymi sensorów.
     */
    QJsonArray loadSensorsFromFile();

    // ===== FUNKCJE ZARZĄDZANIA POMIARAMI =====

    /**
     * @brief Ładuje dane pomiarowe dla określonego sensora.
     * @param sensorId ID sensora, dla którego ładowane są dane.
     */
    void loadMeasurementData(int sensorId);

    /**
     * @brief Ładuje dane pomiarowe dla konkretnego sensora.
     * @param sensorId ID sensora, dla którego ładowane są dane.
     */
    void loadSensorMeasurements(int sensorId);

    /**
     * @brief Wyświetla dane pomiarowe w interfejsie użytkownika.
     * @param values Tablica JSON z wartościami pomiarów.
     */
    void displayMeasurementData(const QJsonArray& values);

    /**
     * @brief Zapisuje dane pomiarowe do lokalnego pliku JSON.
     * @param allMeasurements Tablica JSON ze wszystkimi danymi pomiarowymi.
     */
    void saveMeasurementsToFile(const QJsonArray& allMeasurements);

    /**
     * @brief Ładuje dane pomiarowe z lokalnego pliku JSON.
     * @return Tablica JSON z danymi pomiarowymi.
     */
    QJsonArray loadMeasurementsFromFile();

    /**
     * @brief Ładuje dane pomiarowe dla sensora z pliku lokalnego.
     * @param sensorId ID sensora, dla którego ładowane są dane.
     */
    void onMeasurementsLoadedFromFile(int sensorId);

    // ===== FUNKCJE KOPII ZAPASOWYCH I BEZPIECZEŃSTWA =====

    /**
     * @brief Tworzy kopię zapasową pliku JSON.
     * @param filename Nazwa pliku do wykonania kopii.
     */
    void backupJsonData(const QString& filename);

    // ===== FUNKCJE GEOLOKALIZACJI I MAPY =====

    /**
     * @brief Konwertuje adres na współrzędne geograficzne.
     * @param address Adres do geokodowania.
     * @param radiusKm Promień wyszukiwania w kilometrach.
     */
    void geocodeAddress(const QString& address, double radiusKm);

    /**
     * @brief Znajduje stacje w promieniu od współrzędnych.
     * @param centerLat Szerokość geograficzna środka.
     * @param centerLon Długość geograficzna środka.
     * @param radiusKm Promień w kilometrach.
     */
    void findStationsInRadius(double centerLat, double centerLon, double radiusKm);

    /**
     * @brief Aktualizuje mapę znacznikami stacji.
     * @param stations Wektor danych stacji do wyświetlenia.
     */
    void updateMapWithStations(const QVector<QJsonObject>& stations);

    /**
     * @brief Oblicza odległość między dwoma punktami geograficznymi.
     * @param lat1 Szerokość geograficzna pierwszego punktu.
     * @param lon1 Długość geograficzna pierwszego punktu.
     * @param lat2 Szerokość geograficzna drugiego punktu.
     * @param lon2 Długość geograficzna drugiego punktu.
     * @return Odległość w kilometrach.
     */
    double haversineDistance(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Pokazuje wszystkie stacje na mapie.
     */
    void showAllStationsOnMap();

private:
    Ui::AirQualityMonitorClass ui;              ///< Komponenty interfejsu użytkownika
    QNetworkAccessManager* networkManager;      ///< Manager żądań sieciowych
    QJsonArray cachedStations;                  ///< Lokalnie buforowane dane stacji
    int currentStationId;                       ///< ID aktualnie wybranej stacji
    int currentSensorId;                        ///< ID aktualnie wybranego sensora
    QMap<QString, int> sensorMap;               ///< Mapuje nazwy wyświetlane sensorów na ID
    QJsonArray lastMeasurements;                ///< Ostatnio pobrane pomiary
    QWebChannel* channel;                       ///< Kanał webowy do komunikacji z mapą
    QWebEngineView* webView;                    ///< Widok webowy do wyświetlania mapy
    Bridge* bridge;                             ///< Most między JS a Qt

    // Komponenty UI
    QLineEdit* addressSearchBox;                ///< Pole wprowadzania adresu do wyszukiwania
    QLineEdit* radiusSearchBox;                 ///< Pole wprowadzania promienia wyszukiwania
    QPushButton* searchNearbyButton;            ///< Przycisk wyszukiwania
    QPushButton* showAllStationsButton;         ///< Przycisk pokazania wszystkich stacji
    QPushButton* downloadStationDetail;         ///< Przycisk pobierania danych sensorów
    QPushButton* downloadMeasurementButton;     ///< Przycisk pobierania danych pomiarowych
};