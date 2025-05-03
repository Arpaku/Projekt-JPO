#pragma once

#include <QNetworkRequest>
#include <QNetworkReply>

class INetworkManager {
public:
    virtual ~INetworkManager() {}
    virtual QNetworkReply* get(const QNetworkRequest& request) = 0;
    virtual bool isAvailable() = 0;
};

// Rzeczywista implementacja u¿ywaj¹ca QNetworkAccessManager
class RealNetworkManager : public INetworkManager {
private:
    QNetworkAccessManager* manager;
public:
    RealNetworkManager();
    ~RealNetworkManager();
    QNetworkReply* get(const QNetworkRequest& request) override;
    bool isAvailable() override;
};