#include "SimulationClient.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>

SimulationClient::SimulationClient(QString projectRoot)
    : m_projectRoot(std::move(projectRoot))
{
}

SimulationClient::Result SimulationClient::runSimulation(const QJsonObject& config) const
{
    QProcess process;
    process.setWorkingDirectory(m_projectRoot);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    process.start(
        pythonExecutable(),
        {
            "-m",
            "backend.sim_core.cli"
        }
    );

    if (!process.waitForStarted()) {
        return Result{false, "Failed to start Python simulation process.", {}};
    }

    const QByteArray input = QJsonDocument(config).toJson(QJsonDocument::Compact);
    process.write(input);
    process.closeWriteChannel();

    if (!process.waitForFinished()) {
        return Result{false, "Python simulation process did not finish.", {}};
    }

    const QByteArray output = process.readAllStandardOutput();
    const QByteArray stderrOutput = process.readAllStandardError();
    const QJsonDocument document = QJsonDocument::fromJson(output);
    if (!document.isObject()) {
        return Result{false, QString("Invalid simulator output: %1").arg(QString::fromUtf8(stderrOutput)), {}};
    }

    const QJsonObject object = document.object();
    if (!object.value("ok").toBool()) {
        return Result{false, object.value("error").toString("Unknown simulation error."), {}};
    }

    return Result{true, {}, object.value("result").toObject()};
}

QString SimulationClient::pythonExecutable() const
{
    const QString venvPython = QDir(m_projectRoot).filePath(".venv/Scripts/python.exe");
    if (QFileInfo::exists(venvPython)) {
        return venvPython;
    }
    return "python";
}
