/// @file AirQualityMonitor.cpp
/// @brief Implementacja klasy AirQualityMonitor - Monitor jako�ci powietrza.

#include "AirQualityMonitor.h"
#include "ui_AirQualityMonitor.h"
#include "Bridge.h"

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

/// @brief Konstruktor klasy AirQualityMonitor.
/// @param parent Wska�nik na rodzica widgetu.
AirQualityMonitor::AirQualityMonitor(QWidget* parent)
    : QMainWindow(parent), networkManager(new QNetworkAccessManager(this)), currentStationId(-1), currentSensorId(-1), webView(nullptr)
{
    ui.setupUi(this);
    loadStations();

    connect(ui.searchBox, &QLineEdit::textChanged, this, &AirQualityMonitor::filterStations);
    connect(ui.stationListWidget, &QListWidget::itemClicked, this, &AirQualityMonitor::showStationDetails);
    connect(ui.stationDetailWidget, &QListWidget::itemClicked, this, &AirQualityMonitor::showSensorDetails);
    connect(ui.backButton, &QPushButton::clicked, this, &AirQualityMonitor::showStationListView);
    connect(ui.startDateEdit, &QDateTimeEdit::dateTimeChanged, this, &AirQualityMonitor::updateMeasurementDisplay);
    connect(ui.endDateEdit, &QDateTimeEdit::dateTimeChanged, this, &AirQualityMonitor::updateMeasurementDisplay);
    connect(ui.showMapButton, &QPushButton::clicked, this, [this]() { loadMap(); ui.confirmButton->setCurrentIndex(3); });
    connect(ui.backToListButton, &QPushButton::clicked, this, [this]() { ui.confirmButton->setCurrentIndex(0); });
    connect(ui.searchNearbyButton, &QPushButton::clicked, this, &AirQualityMonitor::onSearchNearbyClicked);
    connect(ui.showAllStationsButton, &QPushButton::clicked, this, &AirQualityMonitor::showAllStationsOnMap);

    //przycisk do pobierania danych sensorow
    connect(ui.downloadStationDetail, &QPushButton::clicked, this, &AirQualityMonitor::downloadSensorData);

    //przycisk do pobierania danych pomiarow

    connect(ui.downloadMeasurementButton, &QPushButton::clicked, this, &AirQualityMonitor::downloadMeasurementData);

   

    

    webView = new QWebEngineView(ui.mapPage);

    if (ui.mapLayout) {
        ui.mapLayout->addWidget(webView);
    }

    webView->setContextMenuPolicy(Qt::NoContextMenu);

    bridge = new Bridge(this);
    channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("bridge"), bridge);
    webView->page()->setWebChannel(channel);

    connect(bridge, &Bridge::markerClicked, this, &AirQualityMonitor::onMarkerClicked);
}

/// @brief Destruktor klasy AirQualityMonitor.
AirQualityMonitor::~AirQualityMonitor()
{
    if (webView) {
        delete webView;
        webView = nullptr;
    }
}

bool AirQualityMonitor::isInternetAvailable()
{
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://www.google.com"));
    QNetworkReply* reply = manager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    loop.exec();  // Czeka na odpowiedź

    if (reply->error() == QNetworkReply::NoError) {
        return true;  // Połączenie jest dostępne
    }
    else {
        return false; // Brak połączenia
    }
}


/// @brief Funkcja do pobierania danych sensor�w i zapisu do pliku.
void AirQualityMonitor::downloadSensorData()
{
    if (currentStationId == -1) {
        return;
    }

    QFile file(QDir::currentPath() + "/sensors.json");
    if (file.exists()) {
        QJsonArray sensorsData = loadSensorsFromFile();
        bool stationExists = false;
        for (const QJsonValue& value : sensorsData) {
            QJsonObject obj = value.toObject();
            if (obj.value("stationId").toInt() == currentStationId) {
                stationExists = true;
                break;
            }
        }
        if (stationExists) {
            onSensorsLoadedFromFile(currentStationId);
            QMessageBox::information(this, "Informacja", "Dane dla tej stacji są już zapisane.", QMessageBox::Ok);

            return;
        }
    }

    QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/station/sensors/%1").arg(currentStationId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onSensorsDownloaded);
}

/// @brief Funkcja obs�uguj�ca zako�czenie pobierania danych sensor�w.
void AirQualityMonitor::onSensorsDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "B��d sieci:" << reply->errorString();
        onSensorsLoadedFromFile(currentStationId);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isArray()) {
        QJsonArray sensorsData = doc.array();
        QJsonArray enhancedData;
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

// Funkcja do aktualizacji pliku sensors.json
void AirQualityMonitor::updateSensorsFile(const QJsonArray& newSensors)
{
    QJsonArray allSensors;

    // Wczytaj obecne dane, je�li plik istnieje
    QFile readFile(QDir::currentPath() + "/sensors.json");
    if (readFile.exists() && readFile.open(QIODevice::ReadOnly)) {
        QByteArray data = readFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            allSensors = doc.array();
        }
        readFile.close();
    }

    // Usu� stare dane dla tej stacji, je�li istniej�
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


// Funkcja do �adowania danych sensor�w dla konkretnej stacji z pliku
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
    
    // Jeśli nie ma danych dla danej stacji, pokaż pustą tablicę (lub odpowiedni komunikat)
    if (stationSensors.isEmpty()) {
        
        ui.stationDetailWidget->clear();
        // Jeśli brak danych w pliku i brak internetu
        if (!isInternetAvailable()) {
            QMessageBox::critical(this, "Błąd", "Brak danych dla wybranej stacji oraz brak połączenia z internetem.", QMessageBox::Ok);
            return;  // Zakończ funkcję
        }

        // Jeśli brak danych w pliku, a internet jest dostępny, pobierz dane z API
        QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/station/sensors/%1").arg(stationId));
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onSensorsDownloaded);
    }
    else {
        // Zaktualizuj listę sensorów tylko dla tej stacji
            updateSensorsList(stationSensors);
        
        
        }
}



// Funkcja do aktualizacji interfejsu u�ytkownika z danymi sensor�w
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


QJsonArray AirQualityMonitor::loadSensorsFromFile()
{
    QFile file(QDir::currentPath() + "/sensors.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonArray();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.isArray() ? doc.array() : QJsonArray();
}

QJsonArray AirQualityMonitor::loadMeasurementsFromFile()
{
    QFile file(QDir::currentPath() + "/measurements.json");
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) return doc.array();
        file.close();
    }
    return QJsonArray();
}




void AirQualityMonitor::downloadMeasurementData()
{
    qDebug() << "Funkcja downloadMeasurementData została wywołana";  // Dodaj log, aby sprawdzić, czy funkcja jest wywoływana

    if (currentSensorId == -1) {
        qDebug() << "Brak wybranego sensora";
        return;  // Brak wybranego sensora
    }

    QFile file(QDir::currentPath() + "/measurements.json");
    if (file.exists()) {
        qDebug() << "Plik measurements.json istnieje";
        QJsonArray measurementData = loadMeasurementsFromFile();
        bool sensorExists = false;
        for (const QJsonValue& value : measurementData) {
            QJsonObject obj = value.toObject();
            if (obj.value("sensorId").toInt() == currentSensorId) {
                sensorExists = true;
                break;
            }
        }
        if (sensorExists) {
            qDebug() << "Dane dla tego sensora są już zapisane";
            onMeasurementsLoadedFromFile(currentSensorId);
            QMessageBox::information(this, "Informacja", "Dane dla tego sensora są już zapisane.", QMessageBox::Ok);
            return;
        }
    }

    // Pobieramy dane z API, jeśli nie znaleźliśmy ich w pliku
    qDebug() << "Pobieranie danych z API...";
    QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(currentSensorId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementsDownloaded);
}



void AirQualityMonitor::onMeasurementsDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd sieci przy pobieraniu pomiarów:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject()) {
        qDebug() << "Nieprawidłowy format danych z API";
        reply->deleteLater();
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray values = root.value("values").toArray();
    int sensorId = reply->property("sensorId").toInt();  // z setProperty()


    // Zapisz dane
    updateMeasurementsFile(sensorId, values);

    // Zaktualizuj widok
    updateMeasurementsList(values);

    reply->deleteLater();
}



void AirQualityMonitor::onMeasurementsLoadedFromFile(int sensorId)
{
    QJsonArray allMeasurements = loadMeasurementsFromFile();
    QJsonArray sensorMeasurements;

    for (const QJsonValue& value : allMeasurements) {
        QJsonObject obj = value.toObject();
        if (obj.value("id").toInt() == sensorId) {
            sensorMeasurements = obj.value("values").toArray();
            break;
        }
    }

    if (sensorMeasurements.isEmpty()) {
        if (!isInternetAvailable()) {
            QMessageBox::critical(this, "Błąd", "Brak danych pomiarowych oraz brak połączenia z internetem.", QMessageBox::Ok);
            return;
        }

        // Pobieramy z API
        QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId));
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementsDownloaded);
    }
    else {
        updateMeasurementsList(sensorMeasurements);
    }
}

void AirQualityMonitor::updateMeasurementsList(const QJsonArray& values)
{
    ui.stationParameterListWidget->clear();
    qDebug() << "Liczba wartości:" << values.size();  // Debugging line

    // Jeśli nie ma danych
    if (values.isEmpty()) {
        ui.stationParameterListWidget->addItem("Brak ważnych danych pomiarowych.");
        return;
    }

    // Iteracja po danych
    for (const QJsonValue& val : values) {
        QJsonObject obj = val.toObject();

        // Debugging, sprawdzenie wartości
        qDebug() << "Data:" << obj.value("date").toString();
        qDebug() << "Wartość:" << obj.value("value").toDouble();

        // Sprawdzamy, czy wartość jest null
        if (obj.contains("value")) {
            QString dateStr = obj.value("date").toString();
            QDateTime dateTime = QDateTime::fromString(dateStr, "yyyy-MM-dd HH:mm:ss");
            QJsonValue value = obj.value("value");

            // Jeśli wartość jest null, wyświetlamy "Brak danych"
            if (value.isNull()) {
                ui.stationParameterListWidget->addItem(
                    QString("%1 - Brak danych")
                    .arg(dateTime.toString("dd.MM.yyyy HH:mm"))
                );
            }
            else {
                double actualValue = value.toDouble();
                ui.stationParameterListWidget->addItem(
                    QString("%1 - %2")
                    .arg(dateTime.toString("dd.MM.yyyy HH:mm"))
                    .arg(actualValue, 0, 'f', 1)
                );
            }
        }
    }

    // Jeśli lista jest pusta po dodaniu wartości
    if (ui.stationParameterListWidget->count() == 0) {
        ui.stationParameterListWidget->addItem("Brak ważnych danych pomiarowych.");
    }
}










void AirQualityMonitor::updateMeasurementsFile(int sensorId, const QJsonArray& newValues)
{
    QFile file(QDir::currentPath() + "/measurements.json");
    QJsonArray allMeasurements;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) allMeasurements = doc.array();
        file.close();
    }

    // Usuń stare dane dla sensorId
    for (int i = allMeasurements.size() - 1; i >= 0; --i) {
        if (allMeasurements.at(i).toObject().value("id").toInt() == sensorId) {
            allMeasurements.removeAt(i);
        }
    }

    // Dodaj nowe dane
    QJsonObject newEntry;
    newEntry["id"] = sensorId;
    newEntry["values"] = newValues;
    allMeasurements.append(newEntry);

    // Zapisz do pliku
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(allMeasurements).toJson());
        file.close();
        QMessageBox::information(this, "Informacja", "Dane pomiarowe zostały zapisane do pliku", QMessageBox::Ok);
    }
    else {
        qDebug() << "Błąd zapisu do measurements.json";
    }
}



void AirQualityMonitor::saveMeasurementsToFile(const QJsonArray& allMeasurements)
{
    QFile file(QDir::currentPath() + "/measurements.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(allMeasurements).toJson());
        file.close();
        qDebug() << "Dane zapisane do pliku measurements.json";
    }
    else {
        qDebug() << "Błąd zapisu do pliku measurements.json";
    }
}




void AirQualityMonitor::loadMeasurementData(int sensorId)
{
    QUrl url(QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId));
    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onMeasurementDataFinished);
}


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
        ui.minValueLabel->setText("Warto�� minimalna\nBrak danych");
        ui.maxValueLabel->setText("Warto�� maksymalna\nBrak danych");
        ui.avgValueLabel->setText("Warto�� �rednia\nBrak danych");
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
        ui.minValueLabel->setText("Wartosc minimalna\nBrak danych");
        ui.maxValueLabel->setText("Wartosc maksymalna\nBrak danych");
        ui.avgValueLabel->setText("Wartosc srednia\nBrak danych");
        ui.trendLabel->setText("Trend wykresu\nBrak danych");
    }
    else {
        double min = *std::min_element(selectedValues.begin(), selectedValues.end());
        double max = *std::max_element(selectedValues.begin(), selectedValues.end());
        double avg = std::accumulate(selectedValues.begin(), selectedValues.end(), 0.0) / selectedValues.size();
        QString trend;

        double sumFirst = 0, sumLast = 0;
        int size = selectedValues.size();

        // Liczymy �redni� z pierwszej po�owy
        for (int i = 0; i < size / 2; ++i) {
            sumFirst += selectedValues[i];
        }

        // Liczymy �redni� z drugiej po�owy
        for (int i = size / 2; i < size; ++i) {
            sumLast += selectedValues[i];
        }

        double avgFirst = sumFirst / (size / 2);
        double avgLast = sumLast / (size - size / 2); // druga po�owa mo�e by� o 1 wi�ksza

        // Okre�lamy trend
        if (avgLast > avgFirst) {
            trend = "Rosnacy";
        }
        else if (avgLast < avgFirst) {
            trend = "Malejacy";
        }
        else {
            trend = "Stabilny";
        }


        // Styl + wy�rodkowanie
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

        // Ustawiamy tekst z podpisem i warto�ci�
        ui.minValueLabel->setText(QString("Wartosc minimalna\n%1").arg(QString::number(min, 'f', 2)));
        ui.maxValueLabel->setText(QString("Wartosc maksymalna\n%1").arg(QString::number(max, 'f', 2)));
        ui.avgValueLabel->setText(QString("Wartosc srednia\n%1").arg(QString::number(avg, 'f', 2)));
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
    axisY->setTitleText("Wartosc");
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
                attribution: '� OpenStreetMap'
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
//pokaz wszystkie stacje przycisk
void AirQualityMonitor::showAllStationsOnMap()
{
    if (!webView)
        return;

    // Wyczy�� stare markery
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


void AirQualityMonitor::onMarkerClicked(const QString& stationName)
{
    qDebug() << "Klikni�to marker:" << stationName;

    // Teraz r�cznie znajdziemy stacj� i poka�emy szczeg�y
    QList<QListWidgetItem*> items = ui.stationListWidget->findItems(stationName, Qt::MatchExactly);
    if (!items.isEmpty()) {
        showStationDetails(items.first());
    }
}
//funkcja do szukania w poblizu na mapie
void AirQualityMonitor::onSearchNearbyClicked()
{
    QString address = ui.addressSearchBox->text();
    QString radiusStr = ui.radiusSearchBox->text();

    if (address.isEmpty() || radiusStr.isEmpty()) {
        qDebug() << "Adres lub promie� pusty!";
        return;
    }

    bool ok;
    double radius = radiusStr.toDouble(&ok);
    if (!ok) {
        qDebug() << "Nieprawidlowy promien!";
        return;
    }

    geocodeAddress(address, radius);
}
//funnkcja do geokodowania adresu
void AirQualityMonitor::geocodeAddress(const QString& address, double radius)
{
    QUrl url(QString("https://nominatim.openstreetmap.org/search?q=%1&format=json&limit=1")
        .arg(QUrl::toPercentEncoding(address)));

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AirQualityMonitorApp");

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, radius]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "B��d geokodowania:" << reply->errorString();
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
// funkcja do szukania stacji w promieniu 
void AirQualityMonitor::findStationsInRadius(double centerLat, double centerLon, double radiusKm)
{
    QVector<QJsonObject> stationsInRadius;

    for (const QJsonValue& value : cachedStations) { // cachedStations -> Twoje wcze�niej za�adowane stacje
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
//funkcja do obliczania dystansu formula haversine
constexpr double kEarthRadiusKm = 6371.0;

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
//aktualizacja mapy
void AirQualityMonitor::updateMapWithStations(const QVector<QJsonObject>& stations)
{
    QString jsCode = "clearMarkers();\n"; // zak�adam, �e masz funkcj� w mapie kt�ra czy�ci stare markery

    for (const auto& station : stations) {
        double lat = station["gegrLat"].toString().toDouble();
        double lon = station["gegrLon"].toString().toDouble();
        QString name = station["stationName"].toString();
        QString popup = QString("%1").arg(name);

        jsCode += QString("addMarker(%1, %2, \"%3\");\n").arg(lat).arg(lon).arg(popup);
    }

    webView->page()->runJavaScript(jsCode);
}


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

void AirQualityMonitor::loadStationsFromApi()
{
    //wielowatkowosc pobiera stacjie z api bo laczy z netem pobiera duzo danych moze potrwac i czasami freezuje gui
    QtConcurrent::run([this]() {
        QUrl url("https://api.gios.gov.pl/pjp-api/rest/station/findAll");
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &AirQualityMonitor::onStationsFinished);
        });
}


void AirQualityMonitor::saveStationsToFile(const QJsonArray& stations)
{
    QFile file(QDir::currentPath() + "/stations.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(stations).toJson());
        file.close();
    }
}

QJsonArray AirQualityMonitor::loadStationsFromFile()
{
    QFile file(QDir::currentPath() + "/stations.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonArray();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.isArray() ? doc.array() : QJsonArray();
}






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

void AirQualityMonitor::showStationListView()
{
    ui.confirmButton->setCurrentIndex(0);
}

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

void AirQualityMonitor::onStationsFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Network error:" << reply->errorString();
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

void AirQualityMonitor::onSensorsFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "B��d sieci:" << reply->errorString();

        // Spr�buj za�adowa� z pliku, je�li dost�pny
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


        // Przekazujemy ID sensora bezpośrednio do downloadMeasurementData
        
    }
    else {
        qDebug() << "Sensor o nazwie" << sensorDisplayName << "nie został znaleziony!";
    }
}

void AirQualityMonitor::onMeasurementDataFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Network error:" << reply->errorString();
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
