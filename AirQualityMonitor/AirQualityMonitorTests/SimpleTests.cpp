#include "pch.h"
#include "SimpleTests.h"

void SimpleTests::testCalculateStatistics()
{
    // Prosty test obliczeń statystycznych - mało prawdopodobny crash
    QVector<double> values = { 10.5, 15.2, 12.8, 9.7, 14.3 };
    double min = values[0];
    double max = values[0];
    double sum = 0.0;

    for (const double& value : values) {
        if (value < min) min = value;
        if (value > max) max = value;
        sum += value;
    }

    double avg = sum / values.size();

    QCOMPARE(min, 9.7);
    QCOMPARE(max, 15.2);
    QVERIFY(qAbs(avg - 12.5) < 0.1);
}

void SimpleTests::testSaveJsonToFile()
{
    // Używamy używamy zmiennej z dłuższym czasem życia
    static QTemporaryDir tempDir;
    // Ustawiamy ręczne usuwanie, aby kontrolować czas życia
    tempDir.setAutoRemove(false);
    QVERIFY(tempDir.isValid());

    // Tworzymy testowy obiekt JSON
    QJsonObject testData;
    testData["name"] = "Test Station";
    testData["id"] = 123;
    testData["value"] = 25.5;

    QJsonArray coordinates;
    coordinates.append(52.4064);
    coordinates.append(16.9252);
    testData["coordinates"] = coordinates;

    // Zapisujemy do pliku
    QString tempPath = tempDir.path();
    QString filePath = tempPath + "/test_data.json";
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));

    QJsonDocument doc(testData);
    QByteArray jsonData = doc.toJson();
    qint64 bytesWritten = file.write(jsonData);
    file.close();

    QVERIFY(bytesWritten > 0);
    QCOMPARE(bytesWritten, jsonData.size());
    QVERIFY(QFile::exists(filePath));
}

void SimpleTests::testReadJsonFromFile()
{
    // Używamy używamy zmiennej z dłuższym czasem życia
    static QTemporaryDir tempDir;
    // Ustawiamy ręczne usuwanie, aby kontrolować czas życia
    tempDir.setAutoRemove(false);
    QVERIFY(tempDir.isValid());

    // Tworzymy testowy plik
    QString filePath = tempDir.path() + "/test_data.json";
    {
        QFile writeFile(filePath);
        QVERIFY(writeFile.open(QIODevice::WriteOnly));

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
    }

    // Odczytujemy plik w osobnym bloku kodu
    {
        QFile readFile(filePath);
        QVERIFY(readFile.open(QIODevice::ReadOnly));

        QByteArray jsonData = readFile.readAll();
        readFile.close();

        QJsonDocument readDoc = QJsonDocument::fromJson(jsonData);
        QVERIFY(!readDoc.isNull());
        QVERIFY(readDoc.isObject());

        QJsonObject readObj = readDoc.object();

        QCOMPARE(readObj["name"].toString(), QString("Test Station"));
        QCOMPARE(readObj["id"].toInt(), 123);
        QCOMPARE(readObj["value"].toDouble(), 25.5);

        QJsonArray readCoordinates = readObj["coordinates"].toArray();
        QCOMPARE(readCoordinates.size(), 2);
        QCOMPARE(readCoordinates[0].toDouble(), 52.4064);
        QCOMPARE(readCoordinates[1].toDouble(), 16.9252);
    }
}

// Używamy QTEST_GUILESS_MAIN zamiast QTEST_MAIN dla lepszej stabilności
QTEST_GUILESS_MAIN(SimpleTests)