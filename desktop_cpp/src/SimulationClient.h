#pragma once

#include <QString>
#include <QJsonObject>

class SimulationClient
{
public:
    struct Result {
        bool ok = false;
        QString error;
        QJsonObject payload;
    };

    explicit SimulationClient(QString projectRoot);
    Result runSimulation(const QJsonObject& config) const;
    Result listSystemPresets() const;
    Result listVirtualTests() const;
    Result vetVirtualTest(const QJsonObject& payload) const;
    Result runVirtualTest(const QJsonObject& payload) const;

private:
    Result invokeBackend(const QStringList& arguments, const QJsonObject* payload) const;
    QString pythonExecutable() const;

    QString m_projectRoot;
};
