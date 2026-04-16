#include "SimulationClient.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>

SimulationClient::SimulationClient(QString projectRoot, QObject* parent)
    : QObject(parent)
    , m_projectRoot(std::move(projectRoot))
{
}

SimulationClient::Result SimulationClient::runSimulation(const QJsonObject& config) const
{
    return invokeBackend({"-m", "backend.sim_core.cli", "simulate"}, &config);
}

SimulationClient::Result SimulationClient::listSystemPresets() const
{
    return invokeBackend({"-m", "backend.sim_core.cli", "list-presets"}, nullptr);
}

SimulationClient::Result SimulationClient::listVirtualTests() const
{
    return invokeBackend({"-m", "backend.sim_core.cli", "list-tests"}, nullptr);
}

SimulationClient::Result SimulationClient::listTruthDatasets() const
{
    return invokeBackend({"-m", "backend.sim_core.cli", "list-truth-datasets"}, nullptr);
}

SimulationClient::Result SimulationClient::vetVirtualTest(const QJsonObject& payload) const
{
    return invokeBackend({"-m", "backend.sim_core.cli", "vet-test"}, &payload);
}

SimulationClient::Result SimulationClient::runVirtualTest(const QJsonObject& payload) const
{
    return invokeBackend({"-m", "backend.sim_core.cli", "run-test"}, &payload);
}

SimulationClient::Result SimulationClient::invokeBackend(const QStringList& arguments, const QJsonObject* payload) const
{
    QProcess process;
    process.setWorkingDirectory(m_projectRoot);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    process.start(pythonExecutable(), arguments);

    if (!process.waitForStarted()) {
        return Result{false, "Failed to start Python simulation process.", {}};
    }

    if (payload != nullptr) {
        const QByteArray input = QJsonDocument(*payload).toJson(QJsonDocument::Compact);
        process.write(input);
    }
    process.closeWriteChannel();

    if (!process.waitForFinished()) {
        return Result{false, "Python simulation process did not finish.", {}};
    }

    return parseBackendResult(
        process.readAllStandardOutput(),
        process.readAllStandardError(),
        process.exitCode(),
        process.exitStatus());
}

bool SimulationClient::runSimulationAsync(const QJsonObject& config)
{
    return invokeBackendAsync({"-m", "backend.sim_core.cli", "simulate"}, &config);
}

bool SimulationClient::vetVirtualTestAsync(const QJsonObject& payload)
{
    return invokeBackendAsync({"-m", "backend.sim_core.cli", "vet-test"}, &payload);
}

bool SimulationClient::runVirtualTestAsync(const QJsonObject& payload)
{
    return invokeBackendAsync({"-m", "backend.sim_core.cli", "run-test"}, &payload);
}

bool SimulationClient::isBusy() const
{
    return m_activeProcess != nullptr;
}

bool SimulationClient::invokeBackendAsync(const QStringList& arguments, const QJsonObject* payload)
{
    if (m_activeProcess != nullptr) {
        return false;
    }

    auto* process = new QProcess(this);
    m_activeProcess = process;
    process->setWorkingDirectory(m_projectRoot);
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    const QByteArray input = payload != nullptr
        ? QJsonDocument(*payload).toJson(QJsonDocument::Compact)
        : QByteArray{};

    connect(process, &QProcess::started, this, [process, input]() {
        if (!input.isEmpty()) {
            process->write(input);
        }
        process->closeWriteChannel();
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (process != m_activeProcess) {
            return;
        }
        if (error == QProcess::FailedToStart) {
            finishActiveRequest(Result{false, "Failed to start Python simulation process.", {}}, process);
        }
    });

    connect(
        process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
            if (process != m_activeProcess) {
                return;
            }

            finishActiveRequest(
                parseBackendResult(
                    process->readAllStandardOutput(),
                    process->readAllStandardError(),
                    exitCode,
                    exitStatus),
                process);
        });

    process->start(pythonExecutable(), arguments);
    return true;
}

SimulationClient::Result SimulationClient::parseBackendResult(
    const QByteArray& output,
    const QByteArray& stderrOutput,
    int exitCode,
    QProcess::ExitStatus exitStatus) const
{
    if (exitStatus != QProcess::NormalExit) {
        return Result{false, "Python simulation process crashed.", {}};
    }
    if (exitCode != 0 && output.isEmpty()) {
        const QString stderrText = QString::fromUtf8(stderrOutput).trimmed();
        return Result{
            false,
            stderrText.isEmpty() ? "Python simulation process failed." : stderrText,
            {}
        };
    }

    const QJsonDocument document = QJsonDocument::fromJson(output);
    if (!document.isObject()) {
        const QString stderrText = QString::fromUtf8(stderrOutput).trimmed();
        return Result{
            false,
            stderrText.isEmpty()
                ? "Invalid simulator output."
                : QString("Invalid simulator output: %1").arg(stderrText),
            {}
        };
    }

    const QJsonObject object = document.object();
    if (!object.value("ok").toBool()) {
        return Result{false, object.value("error").toString("Unknown simulation error."), {}};
    }

    return Result{true, {}, object.value("result").toObject()};
}

void SimulationClient::finishActiveRequest(const Result& result, QProcess* process)
{
    if (process == nullptr || process != m_activeProcess) {
        return;
    }

    m_activeProcess = nullptr;
    process->deleteLater();
    emit requestFinished(result.ok, result.error, result.payload);
}

QString SimulationClient::pythonExecutable() const
{
    const QString venvPython = QDir(m_projectRoot).filePath(".venv/Scripts/python.exe");
    if (QFileInfo::exists(venvPython)) {
        return venvPython;
    }
    return "python";
}
