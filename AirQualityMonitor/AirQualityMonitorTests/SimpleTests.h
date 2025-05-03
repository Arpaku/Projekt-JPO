// SimpleTests.h
#pragma once
// Nagłówki Qt potrzebne do testów
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Jeśli nadal potrzebujesz nagłówków WinRT w projekcie testowym (co jest mało prawdopodobne),
// możesz je pozostawić, ale prawdopodobnie będą one niepotrzebne:
// #include <winrt/Windows.Foundation.h>
// #include <winrt/Windows.Foundation.Collections.h>



#include <QtTest>

class SimpleTests : public QObject
{
    Q_OBJECT

private slots:
    void testCalculateStatistics();
    void testSaveJsonToFile();
    void testReadJsonFromFile();
};