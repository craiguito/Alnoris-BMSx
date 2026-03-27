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

private:
    QString pythonExecutable() const;

    QString m_projectRoot;
};
