#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class SimpleTests : public QObject
{
    Q_OBJECT

private slots:
    // Test 1: Obliczanie statystyk
    void testCalculateStatistics();
    // Test 2: Zapisywanie do pliku JSON
    void testSaveJsonToFile();
    // Test 3: Odczytywanie z pliku JSON
    void testReadJsonFromFile();
};

void SimpleTests::testCalculateStatistics()
{
    // Przygotuj przyk³adowe dane
    QVector<double> values = { 10.5, 15.2, 12.8, 9.7, 14.3 };

    // Oblicz statystyki
    double min = values[0];
    double max = values[0];
    double sum = 0.0;

    for (const double& value : values) {
        if (value < min) min = value;
        if (value > max) max = value;
        sum += value;
    }

    double avg = sum / values.size();

    // Weryfikacja wyników
    QCOMPARE(min, 9.7);
    QCOMPARE(max, 15.2);
    QVERIFY(qAbs(avg - 12.5) < 0.1); // Porównanie z tolerancj¹ 0.1
}

void SimpleTests::testSaveJsonToFile()
{
    // Przygotowanie katalogu tymczasowego
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Przygotuj przyk³adowe dane JSON
    QJsonObject testData;
    testData["name"] = "Test Station";
    testData["id"] = 123;
    testData["value"] = 25.5;

    QJsonArray coordinates;
    coordinates.append(52.4064);
    coordinates.append(16.9252);
    testData["coordinates"] = coordinates;

    // Zapisz do pliku JSON
    QString filePath = tempDir.path() + "/test_data.json";
    QFile file(filePath);
    bool openSuccess = file.open(QIODevice::WriteOnly);
    QVERIFY(openSuccess);

    QJsonDocument doc(testData);
    QByteArray jsonData = doc.toJson();
    qint64 bytesWritten = file.write(jsonData);
    file.close();

    // SprawdŸ czy dane zosta³y poprawnie zapisane
    QVERIFY(bytesWritten > 0);
    QCOMPARE(bytesWritten, jsonData.size());

    // SprawdŸ czy plik istnieje
    QVERIFY(QFile::exists(filePath));
}

void SimpleTests::testReadJsonFromFile()
{
    // Przygotowanie katalogu tymczasowego
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Utwórz testowy plik JSON
    QString filePath = tempDir.path() + "/test_data.json";
    QFile writeFile(filePath);
    QVERIFY(writeFile.open(QIODevice::WriteOnly));

    // Przygotuj przyk³adowe dane JSON
    QJsonObject testData;
    testData["name"] = "Test Station";
    testData["id"] = 123;
    testData["value"] = 25.5;

    QJsonArray coordinates;
    coordinates.append(52.4064);
    coordinates.append(16.9252);
    testData["coordinates"] = coordinates;

    QJsonDocument writeDoc(testData);
    writeFile.write(writeDoc.toJson());
    writeFile.close();

    // Odczytaj dane z pliku JSON
    QFile readFile(filePath);
    QVERIFY(readFile.open(QIODevice::ReadOnly));

    QByteArray jsonData = readFile.readAll();
    readFile.close();

    QJsonDocument readDoc = QJsonDocument::fromJson(jsonData);
    QVERIFY(!readDoc.isNull());
    QVERIFY(readDoc.isObject());

    QJsonObject readObj = readDoc.object();

    // Weryfikacja odczytanych danych
    QCOMPARE(readObj["name"].toString(), QString("Test Station"));
    QCOMPARE(readObj["id"].toInt(), 123);
    QCOMPARE(readObj["value"].toDouble(), 25.5);

    QJsonArray readCoordinates = readObj["coordinates"].toArray();
    QCOMPARE(readCoordinates.size(), 2);
    QCOMPARE(readCoordinates[0].toDouble(), 52.4064);
    QCOMPARE(readCoordinates[1].toDouble(), 16.9252);
}

QTEST_MAIN(SimpleTests)
