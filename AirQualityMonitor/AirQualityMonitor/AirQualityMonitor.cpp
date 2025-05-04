/**
 * @file AirQualityMonitor.cpp
 * @brief Implementacja klasy AirQualityMonitor - Monitor jakości powietrza.
 */

#include "AirQualityMonitor.h"
#include "ui_AirQualityMonitor.h"
#include "Bridge.h"
#include <QTimer>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QQmlContext>
#include <QWebEngineView>
#include <numeric>
#include <algorithm>
#include <QWebChannel>
#include <QtConcurrent/QtConcurrentRun>
#include <QMessageBox>

 // Stałe globalne
constexpr double kEarthRadiusKm = 6371.0;  ///< Promień Ziemi w kilometrach (do obliczeń metodą haversine)
const QString kApiBaseUrl = "https://api.gios.gov.pl/pjp-api/rest/";  ///< Bazowy URL dla API GIOŚ

/**
 * @brief Konstruktor klasy AirQualityMonitor.
 * @param parent Wskaźnik na rodzica widgetu.
 */
AirQualityMonitor::AirQualityMonitor(QWidget* parent)
    : QMainWindow(parent),
    networkManager(new QNetworkAccessManager(this)),
    currentStationId(-1),
    currentSensorId(-1),
    webView(nullptr)
{
    // Konfiguracja UI
    ui.setupUi(this);

    // Ładowanie początkowych danych
    loadStations();

    // Połączenia sygnałów i slotów
    connect(ui.searchBox, &QLineEdit::textChanged, this, &AirQualityMonitor::filterStations);
    connect(ui.stationListWidget, &QListWidget::itemClicked, this, &AirQualityMonitor::showStationDetails);
    connect(ui.stationDetailWidget, &QListWidget::itemClicked, this, &AirQualityMonitor::showSensorDetails);
    connect(ui.backButton, &QPushButton::clicked, this, &AirQualityMonitor::showStationListView);
    connect(ui.startDateEdit, &QDateTimeEdit::dateTimeChanged, this, &AirQualityMonitor::updateMeasurementDisplay);
    connect(ui.endDateEdit, &QDateTimeEdit::dateTimeChanged, this, &AirQualityMonitor::updateMeasurementDisplay);
    connect(ui.showMapButton, &QPushButton::clicked, this, [this]() {
        loadMap();
        ui.confirmButton->setCurrentIndex(3);
        });
    connect(ui.backToListButton, &QPushButton::clicked, this, [this]() {
        ui.confirmButton->setCurrentIndex(0);
        });
    connect(ui.searchNearbyButton, &QPushButton::clicked, this, &AirQualityMonitor::onSearchNearbyClicked);
    connect(ui.showAllStationsButton, &QPushButton::clicked, this, &AirQualityMonitor::showAllStationsOnMap);

    // Przyciski pobierania danych
    connect(ui.downloadStationDetail, &QPushButton::clicked, this, &AirQualityMonitor::downloadSensorData);
    connect(ui.downloadMeasurementButton, &QPushButton::clicked, this, &AirQualityMonitor::downloadMeasurementData);

    // Konfiguracja widoku webowego
    webView = new QWebEngineView(ui.mapPage);
    if (ui.mapLayout) {
        ui.mapLayout->addWidget(webView);
    }
    webView->setContextMenuPolicy(Qt::NoContextMenu);

    // Konfiguracja mostu Qt-JavaScript
    bridge = new Bridge(this);
    channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("bridge"), bridge);
    webView->page()->setWebChannel(channel);

    // Połączenie sygnału kliknięcia markera
    connect(bridge, &Bridge::markerClicked, this, &AirQualityMonitor::onMarkerClicked);
}

/**
 * @brief Destruktor klasy AirQualityMonitor.
 */
AirQualityMonitor::~AirQualityMonitor()
{
    if (webView) {
        delete webView;
        webView = nullptr;
    }
}

/**
 * @brief Sprawdza czy połączenie z internetem jest dostępne.
 * @return True jeśli połączenie jest dostępne, false w przeciwnym razie.
 */
bool AirQualityMonitor::isInternetAvailable()
{
    // Tworzenie dedykowanego managera sieci do tego testu
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(kApiBaseUrl + "station/findAll"));

    // Ustawienie limitu czasu dla żądania
    request.setTransferTimeout(5000); // 5 sekund timeout

    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Również wyjdź z pętli jeśli żądanie przekroczy limit czasu
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);  // 5 sekund timeout

    loop.exec();  // Czekaj na odpowiedź lub timeout

    bool success = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();

    return success;
}

/**
 * @brief Pobiera dane sensorów dla aktualnej stacji i zapisuje do pliku.
 */
void AirQualityMonitor::downloadSensorData()
{
    if (currentStationId == -1) {
        QMessageBox::warning(this, "Ostrzeżenie", "Nie wybrano stacji.", QMessageBox::Ok);
        return;
    }

    // Sprawdź czy dane już istnieją w pliku
    QFile file(QDir::currentPath() + "/sensors.json");
    if (file.exists()) {
        QJsonArray sensorsData = loadSensorsFromFile();
        bool stationExists = false;

        // Sprawdź czy już mamy sensory tej stacji
        for (const QJsonValue& value : sensorsData) {
            QJsonObject obj = value.toObject();
            if (obj.value("stationId").toInt() == currentStationId) {
                stationExists = true;
                break;
            }
        }

        if (stationExists) {
            onSensorsLoadedFromFile(currentStationId);
            QMessageBox::information(this, "Informacja",
                "Dane dla tej stacji są już zapisane.", QMessageBox::Ok);
            return;
        }
    }

    // Dane nie znalezione lokalnie, pobierz z API
    QUrl url(QString(kApiBaseUrl + "station/sensors/%1").arg(currentStationId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onSensorsDownloaded);
}

/**
 * @brief Obsługa zakończenia pobierania danych sensorów.
 */
void AirQualityMonitor::onSensorsDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd sieci:" << reply->errorString();
        onSensorsLoadedFromFile(currentStationId);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isArray()) {
        QJsonArray sensorsData = doc.array();
        QJsonArray enhancedData;

        // Dodaj stationId do każdego obiektu sensora
        for (const QJsonValue& value : sensorsData) {
            QJsonObject sensor = value.toObject();
            sensor.insert("stationId", currentStationId);
            enhancedData.append(sensor);
        }

        updateSensorsFile(enhancedData);
        updateSensorsList(enhancedData);
    }

    reply->deleteLater();
}

/**
 * @brief Aktualizuje plik sensorów nowymi danymi.
 * @param newSensors Tablica JSON z nowymi danymi sensorów.
 */
void AirQualityMonitor::updateSensorsFile(const QJsonArray& newSensors)
{
    QJsonArray allSensors;

    // Wczytaj istniejące dane, jeśli plik istnieje
    QFile readFile(QDir::currentPath() + "/sensors.json");
    if (readFile.exists() && readFile.open(QIODevice::ReadOnly)) {
        QByteArray data = readFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            allSensors = doc.array();
        }
        readFile.close();
    }

    // Usuń stare dane dla tej stacji, jeśli istnieją
    int stationId = -1;
    if (!newSensors.isEmpty()) {
        stationId = newSensors.at(0).toObject().value("stationId").toInt();
    }

    if (stationId != -1) {
        for (int i = allSensors.size() - 1; i >= 0; i--) {
            QJsonObject obj = allSensors.at(i).toObject();
            if (obj.value("stationId").toInt() == stationId) {
                allSensors.removeAt(i);
            }
        }
    }

    // Dodaj nowe dane
    for (const QJsonValue& value : newSensors) {
        allSensors.append(value);
    }

    // Zapisz zaktualizowane dane do pliku
    saveSensorsToFile(allSensors);
}

/**
 * @brief Zapisuje dane sensorów do pliku.
 * @param sensors Tablica JSON z danymi sensorów do zapisania.
 */
void AirQualityMonitor::saveSensorsToFile(const QJsonArray& sensors)
{
    QMessageBox::information(this, "Informacja", "Dane zostały pobrane do pliku", QMessageBox::Ok);

    qDebug() << "Kliknięto przycisk, rozpoczynamy zapis...";
    QFile file(QDir::currentPath() + "/sensors.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(sensors).toJson());
        file.close();
        qDebug() << "Dane zapisane do pliku sensors.json";
    }
    else {
        qDebug() << "Błąd przy otwieraniu pliku do zapisu";
    }
}

/**
 * @brief Ładuje dane sensorów dla konkretnej stacji z pliku.
 * @param stationId ID stacji do załadowania danych.
 */
void AirQualityMonitor::onSensorsLoadedFromFile(int stationId)
{
    // Wczytanie wszystkich sensorów z pliku
    QJsonArray allSensors = loadSensorsFromFile();
    QJsonArray stationSensors;

    // Filtruj sensory tylko dla tej konkretnej stacji
    for (const QJsonValue& value : allSensors) {
        QJsonObject obj = value.toObject();
        // Dodaj tylko te sensory, które mają odpowiedni stationId
        if (obj.value("stationId").toInt() == stationId) {
            stationSensors.append(obj);
        }
    }

    // Jeśli nie ma danych dla danej stacji
    if (stationSensors.isEmpty()) {
        ui.stationDetailWidget->clear();

        // Jeśli brak danych w pliku i brak internetu
        if (!isInternetAvailable()) {
            QMessageBox::critical(this, "Błąd",
                "Brak danych dla wybranej stacji oraz brak połączenia z internetem.", QMessageBox::Ok);
            return;
        }

        // Jeśli brak danych w pliku, a internet jest dostępny, pobierz dane z API
        QUrl url(QString(kApiBaseUrl + "station/sensors/%1").arg(stationId));
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onSensorsDownloaded);
    }
    else {
        // Zaktualizuj listę sensorów tylko dla tej stacji
        updateSensorsList(stationSensors);
    }
}

/**
 * @brief Aktualizuje listę sensorów w interfejsie użytkownika.
 * @param sensorsData Tablica JSON z danymi sensorów.
 */
void AirQualityMonitor::updateSensorsList(const QJsonArray& sensorsData)
{
    ui.stationDetailWidget->clear();  // Wyczyść starą listę
    sensorMap.clear();  // Wyczyść mapę sensorów

    if (sensorsData.isEmpty()) {
        // Jeżeli nie ma danych dla stacji, nie wyświetlaj nic
        return;
    }

    // Przetwórz dane sensorów i zaktualizuj listę
    for (const QJsonValue& value : sensorsData) {
        QJsonObject sensor = value.toObject();
        QString paramName = sensor.value("param").toObject().value("paramName").toString();
        QString paramCode = sensor.value("param").toObject().value("paramCode").toString();
        int sensorId = sensor.value("id").toInt();

        QString sensorDisplay = QString("%1 (%2)").arg(paramName, paramCode);
        ui.stationDetailWidget->addItem(sensorDisplay);
        sensorMap.insert(sensorDisplay, sensorId);
    }
}

/**
 * @brief Pobiera dane pomiarowe dla aktualnego sensora.
 */
void AirQualityMonitor::downloadMeasurementData()
{
    qDebug() << "Funkcja downloadMeasurementData została wywołana";

    if (currentSensorId == -1) {
        QMessageBox::warning(this, "Błąd", "Nie wybrano sensora.", QMessageBox::Ok);
        qDebug() << "Brak wybranego sensora";
        return;
    }

    // Sprawdź czy jesteśmy online
    if (!isInternetAvailable()) {
        QMessageBox::warning(this, "Brak połączenia",
            "Brak połączenia z internetem. Nie można pobrać nowych danych.\n"
            "Sprawdzam dane lokalne...", QMessageBox::Ok);

        // Próba załadowania z lokalnego magazynu
        onMeasurementsLoadedFromFile(currentSensorId);
        return;
    }

    // Mamy połączenie internetowe, kontynuujemy pobieranie
    QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(currentSensorId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);

    // Zapisz ID sensora jako właściwość odpowiedzi
    reply->setProperty("sensorId", currentSensorId);

    // Ustaw timeout dla żądania
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, [=]() {
        if (reply && reply->isRunning()) {
            reply->abort();
            QMessageBox::warning(this, "Timeout",
                "Serwer nie odpowiada w wymaganym czasie. Sprawdzam dane lokalne...",
                QMessageBox::Ok);
            onMeasurementsLoadedFromFile(currentSensorId);
        }
        timer->deleteLater();
        });
    timer->start(10000);  // 10 sekund timeout

    // Połącz z sygnałem zakończenia
    connect(reply, &QNetworkReply::finished, this, [=]() {
        timer->stop();
        timer->deleteLater();
        onMeasurementsDownloaded();
        });
}

/**
 * @brief Obsługuje zakończenie pobierania danych pomiarowych.
 *
 * Przetwarza odpowiedź z API i aktualizuje interfejs użytkownika oraz
 * zapisuje dane lokalnie.
 */
void AirQualityMonitor::onMeasurementsDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int sensorId = reply->property("sensorId").toInt();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd sieci przy pobieraniu pomiarów:" << reply->errorString();
        QMessageBox::warning(this, "Błąd pobierania",
            QString("Nie udało się pobrać danych z serwera: %1\n"
                "Sprawdzam dane lokalne...")
            .arg(reply->errorString()), QMessageBox::Ok);

        // Próba załadowania danych offline
        onMeasurementsLoadedFromFile(sensorId);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject()) {
        qDebug() << "Nieprawidłowy format danych z API";
        QMessageBox::warning(this, "Błąd formatu",
            "Dane pobrane z serwera mają nieprawidłowy format.\n"
            "Sprawdzam dane lokalne...", QMessageBox::Ok);

        // Próba załadowania danych offline
        onMeasurementsLoadedFromFile(sensorId);
        reply->deleteLater();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray values = root.value("values").toArray();

    // Sprawdź czy otrzymaliśmy jakiekolwiek poprawne dane
    bool hasValidData = false;
    for (const QJsonValue& val : values) {
        QJsonObject obj = val.toObject();
        if (obj.contains("value") && !obj.value("value").isNull()) {
            hasValidData = true;
            break;
        }
    }

    if (!hasValidData) {
        QMessageBox::warning(this, "Brak danych",
            "Serwer nie zwrócił żadnych ważnych danych pomiarowych.\n"
            "Sprawdzam dane lokalne...", QMessageBox::Ok);

        // Próba załadowania danych offline
        onMeasurementsLoadedFromFile(sensorId);
        reply->deleteLater();
        return;
    }

    // Otrzymano poprawne dane, zapisz je
    updateMeasurementsFile(sensorId, values);

    // Aktualizuj wyświetlanie
    updateMeasurementsList(values);

    // Zaktualizuj również wyświetlanie pomiarów za pomocą wykresu
    displayMeasurementData(values);

    reply->deleteLater();

    QMessageBox::information(this, "Sukces",
        "Pomyślnie pobrano najnowsze dane z serwera.", QMessageBox::Ok);
}

/**
 * @brief Wczytuje dane pomiarowe dla sensora z pliku lokalnego.
 * @param sensorId ID sensora, dla którego ładowane są pomiary.
 *
 * Jeśli dane nie są dostępne lokalnie i jest połączenie z internetem,
 * próbuje pobrać dane z API.
 */
void AirQualityMonitor::onMeasurementsLoadedFromFile(int sensorId)
{
    QJsonArray allMeasurements = loadMeasurementsFromFile();
    QJsonArray sensorMeasurements;
    QString lastUpdated = "Nieznany";

    // Znajdź pomiary dla wymaganego sensora
    for (const QJsonValue& value : allMeasurements) {
        QJsonObject obj = value.toObject();
        if (obj.value("id").toInt() == sensorId) {
            sensorMeasurements = obj.value("values").toArray();
            // Pobierz timestamp ostatniej aktualizacji jeśli dostępny
            if (obj.contains("lastUpdated")) {
                lastUpdated = obj.value("lastUpdated").toString();
            }
            break;
        }
    }

    if (sensorMeasurements.isEmpty()) {
        // Spróbuj pobrać dane online jeśli to możliwe
        if (!isInternetAvailable()) {
            QMessageBox::warning(this, "Brak danych",
                "Nie znaleziono zapisanych danych pomiarowych dla tego sensora.\n"
                "Dodatkowo brak połączenia z internetem - nie można pobrać nowych danych.",
                QMessageBox::Ok);
            return;
        }

        // Jeśli mamy połączenie, pobierz nowe dane
        QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId));
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        reply->setProperty("sensorId", sensorId); // Zapisz ID sensora jako właściwość
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementsDownloaded);
    }
    else {
        // Mamy dane offline, używamy ich
        updateMeasurementsList(sensorMeasurements);
        displayMeasurementData(sensorMeasurements);

        // Poinformuj użytkownika, że używamy danych z pamięci podręcznej i kiedy były aktualizowane
        QDateTime updateTime = QDateTime::fromString(lastUpdated, Qt::ISODate);
        QString displayTime = updateTime.toString("dd.MM.yyyy HH:mm");

        QMessageBox::information(this, "Używam danych lokalnych",
            QString("Wyświetlam dane z lokalnej bazy. Ostatnia aktualizacja: %1\n\n"
                "Naciśnij przycisk 'Pobierz dane' aby spróbować pobrać aktualne dane z internetu.")
            .arg(displayTime),
            QMessageBox::Ok);

        // Jeśli internet jest dostępny, zapytaj czy chcą świeżych danych
        if (isInternetAvailable()) {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Połączenie dostępne",
                "Wykryto dostępne połączenie z internetem. Czy chcesz pobrać najnowsze dane?",
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // Pobierz nowe dane
                QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId));
                QNetworkRequest request(url);
                QNetworkReply* netReply = networkManager->get(request);
                netReply->setProperty("sensorId", sensorId);
                connect(netReply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementsDownloaded);
            }
        }
    }
}

/**
 * @brief Aktualizuje listę pomiarów w interfejsie użytkownika.
 * @param values Tablica JSON z wartościami pomiarów.
 *
 * Wyświetla pomiary w liście z kolorowym formatowaniem zależnym od wartości.
 */
void AirQualityMonitor::updateMeasurementsList(const QJsonArray& values)
{
    ui.stationParameterListWidget->clear();
    qDebug() << "Liczba wartości:" << values.size();

    // Jeśli nie ma danych
    if (values.isEmpty()) {
        ui.stationParameterListWidget->addItem("Brak ważnych danych pomiarowych.");
        return;
    }

    // Filtruj wartości null dla czystszego wyświetlania
    QList<QListWidgetItem*> validItems;
    QList<QListWidgetItem*> nullItems;

    // Iteruj przez dane
    for (const QJsonValue& val : values) {
        QJsonObject obj = val.toObject();

        if (obj.contains("date")) {
            QString dateStr = obj.value("date").toString();
            QDateTime dateTime = QDateTime::fromString(dateStr, "yyyy-MM-dd HH:mm:ss");
            QJsonValue value = obj.value("value");

            QListWidgetItem* item;

            // Jeśli wartość jest null, dodaj do nullItems
            if (value.isNull()) {
                item = new QListWidgetItem(
                    QString("%1 - Brak danych")
                    .arg(dateTime.toString("dd.MM.yyyy HH:mm"))
                );
                item->setForeground(Qt::gray);
                nullItems.append(item);
            }
            else {
                // Dla poprawnych wartości
                double actualValue = value.toDouble();
                item = new QListWidgetItem(
                    QString("%1 - %2")
                    .arg(dateTime.toString("dd.MM.yyyy HH:mm"))
                    .arg(actualValue, 0, 'f', 1)
                );

                // Dodaj kodowanie kolorami na podstawie wartości (możesz dostosować progi)
                if (actualValue > 50.0) {
                    item->setForeground(Qt::red);
                }
                else if (actualValue > 25.0) {
                    item->setForeground(QColor(255, 165, 0)); // Pomarańczowy
                }
                else {
                    item->setForeground(Qt::green);
                }
                validItems.append(item);
            }
        }
    }

    // Dodaj najpierw poprawne wartości, potem null
    for (QListWidgetItem* item : validItems) {
        ui.stationParameterListWidget->addItem(item);
    }

    for (QListWidgetItem* item : nullItems) {
        ui.stationParameterListWidget->addItem(item);
    }

    // Jeśli po przetworzeniu nie ma elementów, pokaż komunikat
    if (ui.stationParameterListWidget->count() == 0) {
        ui.stationParameterListWidget->addItem("Brak ważnych danych pomiarowych.");
    }
}

/**
 * @brief Aktualizuje plik pomiarów nowymi danymi.
 * @param sensorId ID sensora, którego dane są aktualizowane.
 * @param newValues Tablica JSON z nowymi wartościami pomiarów.
 *
 * Odczytuje istniejące dane, aktualizuje je nowymi wartościami dla danego
 * sensora i zapisuje z powrotem do pliku.
 */
void AirQualityMonitor::updateMeasurementsFile(int sensorId, const QJsonArray& newValues)
{
    // Utwórz timestamp dla danych
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timestamp = currentTime.toString(Qt::ISODate);

    QFile file(QDir::currentPath() + "/measurements.json");
    QJsonArray allMeasurements;

    // Odczytaj istniejące dane jeśli plik istnieje
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) allMeasurements = doc.array();
        file.close();
    }

    // Sprawdź czy mamy już dane dla tego sensora
    bool sensorFound = false;
    for (int i = 0; i < allMeasurements.size(); i++) {
        QJsonObject obj = allMeasurements[i].toObject();
        if (obj.value("id").toInt() == sensorId) {
            // Aktualizuj istniejące dane sensora
            sensorFound = true;

            // Dodaj timestamp wskazujący kiedy dane były ostatnio aktualizowane
            obj["lastUpdated"] = timestamp;
            obj["values"] = newValues;
            allMeasurements[i] = obj;
            break;
        }
    }

    // Jeśli sensor nie istnieje w naszym magazynie, dodaj go
    if (!sensorFound) {
        QJsonObject newEntry;
        newEntry["id"] = sensorId;
        newEntry["values"] = newValues;
        newEntry["lastUpdated"] = timestamp;
        allMeasurements.append(newEntry);
    }

    // Zapisz wszystkie pomiary z powrotem do pliku
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(allMeasurements).toJson());
        file.close();
        QMessageBox::information(this, "Informacja", "Dane pomiarowe zostały zapisane do pliku", QMessageBox::Ok);
    }
    else {
        QMessageBox::warning(this, "Błąd", "Nie udało się zapisać danych do pliku. Sprawdź uprawnienia.", QMessageBox::Ok);
        qDebug() << "Błąd zapisu do measurements.json";
    }
}

/**
 * @brief Wyświetla dane pomiarowe w formie wykresu i statystyk.
 * @param values Tablica JSON z wartościami pomiarów.
 *
 * Tworzy wykres liniowy z danymi pomiarowymi oraz oblicza i wyświetla
 * statystyki: wartość minimalną, maksymalną, średnią i trend.
 */
void AirQualityMonitor::displayMeasurementData(const QJsonArray& values)
{
    if (values.isEmpty())
        return;

    lastMeasurements = values;

    QDateTime minDate = QDateTime::currentDateTime();
    QDateTime maxDate = QDateTime::fromSecsSinceEpoch(0);

    for (const QJsonValue& value : values) {
        QJsonObject obj = value.toObject();
        QDateTime dt = QDateTime::fromString(obj.value("date").toString(), Qt::ISODate);
        if (dt.isValid()) {
            if (dt < minDate) minDate = dt;
            if (dt > maxDate) maxDate = dt;
        }
    }

    ui.startDateEdit->setDateTime(minDate);
    ui.endDateEdit->setDateTime(maxDate);

    updateMeasurementDisplay();
}

/**
 * @brief Aktualizuje wyświetlanie wykresu i statystyk pomiarów.
 *
 * Odświeża wykres i statystyki na podstawie wybranego zakresu dat.
 * Oblicza minimalną, maksymalną i średnią wartość oraz trend danych.
 */
void AirQualityMonitor::updateMeasurementDisplay()
{
    ui.stationParameterListWidget->clear();

    QLayoutItem* item;
    while ((item = ui.verticalLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            delete item->widget();
        delete item;
    }


    if (lastMeasurements.isEmpty()) {
        ui.minValueLabel->setText("Wartość minimalna\nBrak danych");
        ui.maxValueLabel->setText("Wartość maksymalna\nBrak danych");
        ui.avgValueLabel->setText("Wartość średnia\nBrak danych");
        ui.trendLabel->setText("Trend wykresu\nBrak danych");
        return;
    }

    QLineSeries* series = new QLineSeries();
    QList<double> selectedValues;

    for (const QJsonValue& value : lastMeasurements) {
        QJsonObject obj = value.toObject();
        QDateTime dt = QDateTime::fromString(obj.value("date").toString(), Qt::ISODate);
        if (dt.isValid() && obj.contains("value") && !obj.value("value").isNull()) {
            if (dt >= ui.startDateEdit->dateTime() && dt <= ui.endDateEdit->dateTime()) {
                double val = obj.value("value").toDouble();
                selectedValues.append(val);
                series->append(dt.toMSecsSinceEpoch(), val);
                ui.stationParameterListWidget->addItem(dt.toString("yyyy-MM-dd HH:mm") + ": " + QString::number(val));
            }
        }
    }

    if (selectedValues.isEmpty()) {
        ui.minValueLabel->setText("Wartość minimalna\nBrak danych");
        ui.maxValueLabel->setText("Wartość maksymalna\nBrak danych");
        ui.avgValueLabel->setText("Wartość średnia\nBrak danych");
        ui.trendLabel->setText("Trend wykresu\nBrak danych");
    }
    else {
        double min = *std::min_element(selectedValues.begin(), selectedValues.end());
        double max = *std::max_element(selectedValues.begin(), selectedValues.end());
        double avg = std::accumulate(selectedValues.begin(), selectedValues.end(), 0.0) / selectedValues.size();
        QString trend;

        double sumFirst = 0, sumLast = 0;
        int size = selectedValues.size();

        // Liczymy średnią z pierwszej połowy
        for (int i = 0; i < size / 2; ++i) {
            sumFirst += selectedValues[i];
        }

        // Liczymy średnią z drugiej połowy
        for (int i = size / 2; i < size; ++i) {
            sumLast += selectedValues[i];
        }

        double avgFirst = sumFirst / (size / 2);
        double avgLast = sumLast / (size - size / 2); // druga połowa może być o 1 większa

        // Określamy trend
        if (avgLast > avgFirst) {
            trend = "Rosnący";
        }
        else if (avgLast < avgFirst) {
            trend = "Malejący";
        }
        else {
            trend = "Stabilny";
        }


        // Styl + wyśrodkowanie
        QString labelStyle = "font-size: 18px; font-weight: bold; color: #00FFC6;";
        QString trendStyle = "font-size: 18px; font-weight: bold; color: #00FFC6;";

        ui.minValueLabel->setStyleSheet(labelStyle);
        ui.maxValueLabel->setStyleSheet(labelStyle);
        ui.avgValueLabel->setStyleSheet(labelStyle);
        ui.trendLabel->setStyleSheet(trendStyle);

        ui.minValueLabel->setAlignment(Qt::AlignCenter);
        ui.maxValueLabel->setAlignment(Qt::AlignCenter);
        ui.avgValueLabel->setAlignment(Qt::AlignCenter);
        ui.trendLabel->setAlignment(Qt::AlignCenter);

        // Ustawiamy tekst z podpisem i wartością
        ui.minValueLabel->setText(QString("Wartość minimalna\n%1").arg(QString::number(min, 'f', 2)));
        ui.maxValueLabel->setText(QString("Wartość maksymalna\n%1").arg(QString::number(max, 'f', 2)));
        ui.avgValueLabel->setText(QString("Wartość średnia\n%1").arg(QString::number(avg, 'f', 2)));
        ui.trendLabel->setText(QString("Trend wykresu\n%1").arg(trend));
    }

    // Wykres
    QChart* chart = new QChart();
    chart->legend()->hide();
    chart->addSeries(series);
    chart->setTitle("Pomiary");

    QDateTimeAxis* axisX = new QDateTimeAxis;
    axisX->setFormat("dd-MM HH:mm");
    axisX->setTitleText("Czas");
    axisX->setLabelsAngle(-45);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis* axisY = new QValueAxis;
    axisY->setTitleText("Wartość");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->setBackgroundBrush(QBrush(QColor("#121212")));
    chart->setTitleBrush(QBrush(Qt::white));
    axisX->setLinePenColor(Qt::white);
    axisX->setLabelsBrush(Qt::white);
    axisY->setLinePenColor(Qt::white);
    axisY->setLabelsBrush(Qt::white);
    axisX->setGridLineColor(QColor("#555555"));
    axisY->setGridLineColor(QColor("#555555"));
    series->setColor(QColor("#00c3ff"));

    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    ui.verticalLayout->addWidget(chartView);
}

/**
 * @brief Ładuje interfejs mapy.
 *
 * Tworzy i inicjalizuje interfejs OpenStreetMap do wyświetlania
 * stacji pomiarowych na mapie interaktywnej.
 */
void AirQualityMonitor::loadMap()
{
    QString html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8" />
      <title>Mapa Stacji</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
      <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
      <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
      <script>
        new QWebChannel(qt.webChannelTransport, function(channel) {
            window.bridge = channel.objects.bridge;
        });

        var map;
        var markers = [];

        function addMarker(lat, lon, popupText) {
            var marker = L.marker([lat, lon]).addTo(map);
            marker.bindPopup(popupText);
            marker.on('click', function() {
                bridge.onMarkerClicked(popupText);
            });
            markers.push(marker);
        }

        function clearMarkers() {
            for (var i = 0; i < markers.length; i++) {
                map.removeLayer(markers[i]);
            }
            markers = [];
        }

        window.onload = function() {
            map = L.map('map').setView([52.4064, 16.9252], 12);
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                maxZoom: 19,
                attribution: '© OpenStreetMap'
            }).addTo(map);
        };
      </script>

      <style>
        html, body { height: 100%; margin: 0; }
        #map { height: 100%; }
      </style>
    </head>
    <body>
      <div id="map"></div>
    </body>
    </html>
    )";

    if (webView) {
        webView->setHtml(html);
    }
}

/**
 * @brief Pokazuje wszystkie stacje na mapie.
 *
 * Wyświetla wszystkie dostępne stacje pomiarowe jako markery na mapie.
 */
void AirQualityMonitor::showAllStationsOnMap()
{
    if (!webView)
        return;

    // Wyczyść stare markery
    webView->page()->runJavaScript("clearMarkers();");

    // Dodaj wszystkie stacje
    for (const QJsonValue& value : cachedStations) {
        QJsonObject obj = value.toObject();
        double lat = obj.value("gegrLat").toString().toDouble();
        double lon = obj.value("gegrLon").toString().toDouble();
        QString name = obj.value("stationName").toString().replace("'", "\\'");
        QString js = QString("addMarker(%1, %2, '%3');").arg(lat).arg(lon).arg(name);
        webView->page()->runJavaScript(js);
    }
}

/**
 * @brief Obsługuje kliknięcie markera na mapie.
 * @param stationName Nazwa stacji, która została kliknięta.
 *
 * Znajduje stację o podanej nazwie i wyświetla jej szczegóły.
 */
void AirQualityMonitor::onMarkerClicked(const QString& stationName)
{
    qDebug() << "Kliknięto marker:" << stationName;

    // Ręcznie znajdź stację i pokaż szczegóły
    QList<QListWidgetItem*> items = ui.stationListWidget->findItems(stationName, Qt::MatchExactly);
    if (!items.isEmpty()) {
        showStationDetails(items.first());
    }
}

/**
 * @brief Obsługuje kliknięcie przycisku wyszukiwania w pobliżu.
 *
 * Pobiera adres i promień z pól tekstowych i inicjuje wyszukiwanie
 * stacji w podanym promieniu od wskazanego adresu.
 */
void AirQualityMonitor::onSearchNearbyClicked()
{
    QString address = ui.addressSearchBox->text();
    QString radiusStr = ui.radiusSearchBox->text();

    if (address.isEmpty() || radiusStr.isEmpty()) {
        qDebug() << "Adres lub promień pusty!";
        return;
    }

    bool ok;
    double radius = radiusStr.toDouble(&ok);
    if (!ok) {
        qDebug() << "Nieprawidłowy promień!";
        return;
    }

    geocodeAddress(address, radius);
}

/**
 * @brief Konwertuje adres tekstowy na współrzędne geograficzne.
 * @param address Adres do geokodowania.
 * @param radius Promień wyszukiwania w kilometrach.
 *
 * Wykorzystuje usługę Nominatim OpenStreetMap do geokodowania adresu,
 * a następnie wywołuje funkcję wyszukiwania stacji w promieniu.
 */
void AirQualityMonitor::geocodeAddress(const QString& address, double radius)
{
    QUrl url(QString("https://nominatim.openstreetmap.org/search?q=%1&format=json&limit=1")
        .arg(QUrl::toPercentEncoding(address)));

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AirQualityMonitorApp");

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, radius]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Błąd geokodowania:" << reply->errorString();
            reply->deleteLater();
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray() && !doc.array().isEmpty()) {
            QJsonObject obj = doc.array().first().toObject();
            double lat = obj.value("lat").toString().toDouble();
            double lon = obj.value("lon").toString().toDouble();
            qDebug() << "Adres znaleziony: " << lat << lon;

            findStationsInRadius(lat, lon, radius);
        }
        else {
            qDebug() << "Nie znaleziono adresu.";
        }

        reply->deleteLater();
        });
}

/**
 * @brief Znajduje stacje w promieniu od określonych współrzędnych.
 * @param centerLat Szerokość geograficzna środka obszaru wyszukiwania.
 * @param centerLon Długość geograficzna środka obszaru wyszukiwania.
 * @param radiusKm Promień wyszukiwania w kilometrach.
 *
 * Przeszukuje buforowane stacje i wybiera te, które znajdują się
 * w określonym promieniu od podanych współrzędnych.
 */
void AirQualityMonitor::findStationsInRadius(double centerLat, double centerLon, double radiusKm)
{
    QVector<QJsonObject> stationsInRadius;

    for (const QJsonValue& value : cachedStations) {
        QJsonObject obj = value.toObject();
        double lat = obj.value("gegrLat").toString().toDouble();
        double lon = obj.value("gegrLon").toString().toDouble();

        double distance = haversineDistance(centerLat, centerLon, lat, lon);
        if (distance <= radiusKm) {
            stationsInRadius.append(obj);
        }
    }

    updateMapWithStations(stationsInRadius);
}

/**
 * @brief Oblicza odległość między dwoma punktami geograficznymi używając formuły Haversine.
 * @param lat1 Szerokość geograficzna pierwszego punktu.
 * @param lon1 Długość geograficzna pierwszego punktu.
 * @param lat2 Szerokość geograficzna drugiego punktu.
 * @param lon2 Długość geograficzna drugiego punktu.
 * @return Odległość w kilometrach.
 *
 * Implementacja formuły Haversine do obliczania odległości między punktami
 * na powierzchni Ziemi, uwzględniając jej krzywiznę.
 */
double AirQualityMonitor::haversineDistance(double lat1, double lon1, double lat2, double lon2)
{
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);

    double a = sin(dLat / 2) * sin(dLat / 2) +
        cos(qDegreesToRadians(lat1)) * cos(qDegreesToRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);

    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return kEarthRadiusKm * c;
}

/**
 * @brief Aktualizuje mapę znacznikami stacji.
 * @param stations Wektor obiektów JSON z danymi stacji.
 *
 * Generuje kod JavaScript do aktualizacji mapy znacznikami
 * dla podanych stacji. Każdy znacznik zawiera nazwę stacji.
 */
void AirQualityMonitor::updateMapWithStations(const QVector<QJsonObject>& stations)
{
    QString jsCode = "clearMarkers();\n"; // zakładamy, że masz funkcję w mapie która czyści stare markery

    for (const auto& station : stations) {
        double lat = station["gegrLat"].toString().toDouble();
        double lon = station["gegrLon"].toString().toDouble();
        QString name = station["stationName"].toString();
        QString popup = QString("%1").arg(name);

        jsCode += QString("addMarker(%1, %2, \"%3\");\n").arg(lat).arg(lon).arg(popup);
    }

    webView->page()->runJavaScript(jsCode);
}

/**
 * @brief Ładuje dane stacji z API lub pliku lokalnego.
 *
 * Sprawdza najpierw, czy plik ze stacjami istnieje lokalnie.
 * Jeśli tak, ładuje dane z pliku, w przeciwnym razie pobiera z API.
 */
void AirQualityMonitor::loadStations()
{
    QFile file(QDir::currentPath() + "/stations.json");
    if (file.exists()) {
        cachedStations = loadStationsFromFile();
        filterStations(ui.searchBox->text());
    }
    else {
        loadStationsFromApi();
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Pobiera dane stacji z API GIOŚ.
 *
 * Używa wielowątkowości do pobrania danych stacji z API,
 * aby nie blokować interfejsu użytkownika podczas pobierania.
 */
void AirQualityMonitor::loadStationsFromApi()
{
    // Wielowątkowość - pobiera stacje z API, bo łączy z netem, pobiera dużo danych,
    // może potrwać i czasami zamraża GUI
    QtConcurrent::run([this]() {
        QUrl url("https://api.gios.gov.pl/pjp-api/rest/station/findAll");
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onStationsFinished);
        });
}
/////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * @brief Obsługuje zakończenie pobierania danych stacji z API.
 *
 * Przetwarza odpowiedź API, zapisuje dane do pliku lokalnego
 * i aktualizuje interfejs użytkownika.
 */
void AirQualityMonitor::onStationsFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd sieci:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isArray()) {
        cachedStations = doc.array();
        saveStationsToFile(cachedStations);
        filterStations(ui.searchBox->text());
    }

    reply->deleteLater();
}

/**
 * @brief Filtruje listę stacji na podstawie tekstu wyszukiwania.
 * @param text Tekst filtrujący nazwy stacji.
 *
 * Wyświetla w liście stacji tylko te, których nazwy zawierają
 * podany tekst (bez uwzględniania wielkości liter).
 */
void AirQualityMonitor::filterStations(const QString& text)
{
    ui.stationListWidget->clear();
    for (const QJsonValue& value : cachedStations) {
        QJsonObject obj = value.toObject();
        QString stationName = obj.value("stationName").toString();
        if (stationName.contains(text, Qt::CaseInsensitive)) {
            ui.stationListWidget->addItem(stationName);
        }
    }
}

/**
 * @brief Wyświetla widok listy stacji.
 *
 * Przełącza interfejs użytkownika z powrotem do widoku listy stacji.
 */
void AirQualityMonitor::showStationListView()
{
    ui.confirmButton->setCurrentIndex(0);
}

/**
 * @brief Wyświetla szczegóły wybranej stacji.
 * @param item Element listy reprezentujący wybraną stację.
 *
 * Przełącza widok na panel szczegółów stacji i ładuje dane o sensorach
 * dla wybranej stacji.
 */
void AirQualityMonitor::showStationDetails(QListWidgetItem* item)
{
    if (!item) return;

    ui.confirmButton->setCurrentIndex(1);

    QString stationName = item->text();
    int stationId = -1;
    for (const QJsonValue& value : cachedStations) {
        QJsonObject obj = value.toObject();
        if (obj.value("stationName").toString() == stationName) {
            stationId = obj.value("id").toInt();
            break;
        }
    }

    if (stationId != -1) {
        currentStationId = stationId;
        QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/station/sensors/%1").arg(stationId));
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onSensorsFinished);
    }
}

/**
 * @brief Wyświetla szczegóły wybranego sensora.
 * @param item Element listy reprezentujący wybrany sensor.
 *
 * Przełącza widok na panel szczegółów sensora i ładuje dane pomiarowe
 * dla wybranego sensora.
 */
void AirQualityMonitor::showSensorDetails(QListWidgetItem* item)
{
    if (!item) return;

    ui.confirmButton->setCurrentIndex(2);

    QString sensorDisplayName = item->text();
    if (sensorMap.contains(sensorDisplayName)) {
        int sensorId = sensorMap.value(sensorDisplayName);
        currentSensorId = sensorMap.value(sensorDisplayName);
        qDebug() << "Ładowanie danych pomiarowych dla sensora o ID:" << sensorId;
        loadMeasurementData(sensorId);
    }
    else {
        qDebug() << "Sensor o nazwie" << sensorDisplayName << "nie został znaleziony!";
    }
}



//////////////////////

/**
 * @brief Ładuje dane stacji z pliku JSON.
 * @return Tablica JSON z danymi stacji.
 */
QJsonArray AirQualityMonitor::loadStationsFromFile()
{
    QFile file(QDir::currentPath() + "/stations.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Nie można otworzyć pliku stations.json: " << file.errorString();
        return QJsonArray();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Błąd parsowania JSON: " << parseError.errorString();
        return QJsonArray();
    }

    if (!doc.isArray()) {
        qDebug() << "Dokument JSON nie zawiera tablicy";
        return QJsonArray();
    }

    return doc.array();
}

/**
 * @brief Zapisuje dane stacji do pliku JSON.
 * @param stations Tablica JSON z danymi stacji.
 */
void AirQualityMonitor::saveStationsToFile(const QJsonArray& stations)
{
    QFile file(QDir::currentPath() + "/stations.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(stations).toJson());
        file.close();
        qDebug() << "Dane zapisane do pliku stations.json";
    }
    else {
        qDebug() << "Nie można otworzyć pliku stations.json do zapisu: " << file.errorString();
    }
}

/**
 * @brief Konfiguruje widok webowy dla mapy.
 */
void AirQualityMonitor::setupWebView()
{
    webView = new QWebEngineView(ui.mapPage);

    if (ui.mapLayout) {
        ui.mapLayout->addWidget(webView);
    }

    webView->setContextMenuPolicy(Qt::NoContextMenu);

    // Konfiguracja mostu Qt-JavaScript
    bridge = new Bridge(this);
    channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("bridge"), bridge);
    webView->page()->setWebChannel(channel);

    // Połączenie sygnału kliknięcia markera
    connect(bridge, &Bridge::markerClicked, this, &AirQualityMonitor::onMarkerClicked);
}

/**
 * @brief Łączy sygnały interfejsu użytkownika z odpowiednimi slotami.
 */
void AirQualityMonitor::connectSignalsAndSlots()
{
    // Główna nawigacja
    connect(ui.searchBox, &QLineEdit::textChanged, this, &AirQualityMonitor::filterStations);
    connect(ui.stationListWidget, &QListWidget::itemClicked, this, &AirQualityMonitor::showStationDetails);
    connect(ui.stationDetailWidget, &QListWidget::itemClicked, this, &AirQualityMonitor::showSensorDetails);
    connect(ui.backButton, &QPushButton::clicked, this, &AirQualityMonitor::showStationListView);

    // Wybór zakresu dat
    connect(ui.startDateEdit, &QDateTimeEdit::dateTimeChanged, this, &AirQualityMonitor::updateMeasurementDisplay);
    connect(ui.endDateEdit, &QDateTimeEdit::dateTimeChanged, this, &AirQualityMonitor::updateMeasurementDisplay);

    // Nawigacja mapy
    connect(ui.showMapButton, &QPushButton::clicked, this, [this]() {
        loadMap();
        ui.confirmButton->setCurrentIndex(3);
        });
    connect(ui.backToListButton, &QPushButton::clicked, this, [this]() {
        ui.confirmButton->setCurrentIndex(0);
        });
    connect(ui.searchNearbyButton, &QPushButton::clicked, this, &AirQualityMonitor::onSearchNearbyClicked);
    connect(ui.showAllStationsButton, &QPushButton::clicked, this, &AirQualityMonitor::showAllStationsOnMap);

    // Przyciski pobierania danych
    connect(ui.downloadStationDetail, &QPushButton::clicked, this, &AirQualityMonitor::downloadSensorData);
    connect(ui.downloadMeasurementButton, &QPushButton::clicked, this, &AirQualityMonitor::downloadMeasurementData);
}

/**
 * @brief Obsługuje zakończenie pobierania danych pomiarowych dla sensora.
 */
void AirQualityMonitor::onMeasurementDataFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd sieci:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isObject()) {
        QJsonArray values = doc.object().value("values").toArray();
        displayMeasurementData(values);
    }

    reply->deleteLater();
}

/**
 * @brief Obsługuje zakończenie pobierania danych sensorów dla stacji.
 */
void AirQualityMonitor::onSensorsFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd sieci:" << reply->errorString();

        // Spróbuj załadować z pliku, jeśli dostępny
        onSensorsLoadedFromFile(currentStationId);

        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isArray()) {
        QJsonArray sensorsData = doc.array();
        updateSensorsList(sensorsData);
    }

    reply->deleteLater();
}

/**
 * @brief Ładuje dane pomiarowe dla określonego sensora.
 * @param sensorId ID sensora, dla którego ładowane są pomiary.
 */
void AirQualityMonitor::loadMeasurementData(int sensorId)
{
    QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementDataFinished);
}

/**
 * @brief Ładuje dane pomiarowe dla określonego sensora.
 * @param sensorId ID sensora, dla którego ładowane są dane.
 */
void AirQualityMonitor::loadSensorMeasurements(int sensorId)
{
    QString filePath = QString("measurements_%1.json").arg(sensorId);
    QFile file(filePath);

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray fileData = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(fileData);
        if (doc.isObject() && doc.object().contains("values")) {
            QJsonArray values = doc.object().value("values").toArray();
            qDebug() << "Wczytano dane z pliku dla sensora" << sensorId;
            updateMeasurementsList(values);
            return;
        }
    }

    // Jeżeli pliku brak lub nie da się sparsować – wyślij żądanie HTTP
    qDebug() << "Plik nie istnieje lub nieczytelny, pobieranie z serwera...";
    QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    reply->setProperty("sensorId", sensorId);
    connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementsDownloaded);
}

/**
 * @brief Zapisuje dane pomiarowe do pliku JSON.
 * @param allMeasurements Tablica JSON z danymi pomiarowymi.
 */
void AirQualityMonitor::saveMeasurementsToFile(const QJsonArray& allMeasurements)
{
    QFile file(QDir::currentPath() + "/measurements.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(allMeasurements).toJson());
        file.close();
        qDebug() << "Dane zapisane do pliku measurements.json";
    }
    else {
        qDebug() << "Błąd zapisu do pliku measurements.json: " << file.errorString();
    }
}

/**
 * @brief Ładuje dane pomiarowe z pliku JSON.
 * @return Tablica JSON z danymi pomiarowymi.
 */
QJsonArray AirQualityMonitor::loadMeasurementsFromFile()
{
    QFile file(QDir::currentPath() + "/measurements.json");
    if (!file.exists()) {
        qDebug() << "Plik measurements.json nie istnieje.";
        return QJsonArray();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Nie można otworzyć pliku measurements.json:" << file.errorString();
        return QJsonArray();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Błąd parsowania JSON:" << parseError.errorString();
        return QJsonArray();
    }

    if (!doc.isArray()) {
        qDebug() << "Plik JSON nie zawiera tablicy jako głównego elementu.";
        return QJsonArray();
    }

    return doc.array();
}

/**
 * @brief Ładuje dane sensorów z pliku JSON.
 * @return Tablica JSON z danymi sensorów.
 */
QJsonArray AirQualityMonitor::loadSensorsFromFile()
{
    QFile file(QDir::currentPath() + "/sensors.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Nie można otworzyć pliku sensors.json: " << file.errorString();
        return QJsonArray();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Błąd parsowania JSON:" << parseError.errorString();
        return QJsonArray();
    }

    if (!doc.isArray()) {
        qDebug() << "Dokument JSON nie zawiera tablicy";
        return QJsonArray();
    }

    return doc.array();
}

/**
 * @brief Tworzy kopię zapasową pliku JSON.
 * @param filename Nazwa pliku do wykonania kopii.
 */
void AirQualityMonitor::backupJsonData(const QString& filename)
{
    QFile file(QDir::currentPath() + "/" + filename);
    if (!file.exists()) {
        return; // Nic do backupu
    }

    // Utwórz katalog backupu jeśli nie istnieje
    QDir dir(QDir::currentPath() + "/backups");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Utwórz nazwę pliku backupu z timestampem
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupFilename = QString("backups/%1_%2").arg(timestamp).arg(filename);

    // Kopiuj plik
    if (file.copy(QDir::currentPath() + "/" + backupFilename)) {
        qDebug() << "Backup utworzony:" << backupFilename;
    }
    else {
        qDebug() << "Nie udało się utworzyć backupu pliku" << filename;
    }
}