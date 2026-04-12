#pragma once

#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QString>

class SimulationClient : public QObject
{
    Q_OBJECT

public:
    struct Result {
        bool ok = false;
        QString error;
        QJsonObject payload;
    };

    explicit SimulationClient(QString projectRoot, QObject* parent = nullptr);
    Result runSimulation(const QJsonObject& config) const;
    Result listSystemPresets() const;
    Result listVirtualTests() const;
    Result vetVirtualTest(const QJsonObject& payload) const;
    Result runVirtualTest(const QJsonObject& payload) const;
    bool runSimulationAsync(const QJsonObject& config);
    bool vetVirtualTestAsync(const QJsonObject& payload);
    bool runVirtualTestAsync(const QJsonObject& payload);
    bool isBusy() const;

signals:
    void requestFinished(bool ok, const QString& error, const QJsonObject& payload);

private:
    Result invokeBackend(const QStringList& arguments, const QJsonObject* payload) const;
    bool invokeBackendAsync(const QStringList& arguments, const QJsonObject* payload);
    Result parseBackendResult(
        const QByteArray& output,
        const QByteArray& stderrOutput,
        int exitCode,
        QProcess::ExitStatus exitStatus) const;
    void finishActiveRequest(const Result& result, QProcess* process);
    QString pythonExecutable() const;

    QString m_projectRoot;
    QProcess* m_activeProcess = nullptr;
};
