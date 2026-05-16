#include "ProjectPersistence.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>

namespace trade_study {

bool saveProjectState(const QString& path, const ProjectState& state, QString* error)
{
    QJsonObject root;
    root.insert("schema_version", 3);
    root.insert("saved_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert("reference_preset_index", state.referencePresetIndex);
    root.insert("comparison_active", state.comparisonActive);
    root.insert("system_preset", state.systemPreset);
    root.insert("simulation_config", state.simulationConfig);
    root.insert("baseline_config", state.baselineConfig);
    root.insert("baseline_result", state.baselineResult);
    root.insert("active_result", state.activeResult);
    root.insert("report_context", state.reportContext);
    root.insert("validation_settings", state.validationSettings);
    root.insert("validation_result", state.validationResult);
    root.insert("cad_document", state.cadDocument);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error != nullptr) {
            *error = QString("Could not write the selected project file: %1").arg(path);
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

std::optional<ProjectState> loadProjectState(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = QString("Could not read the selected project file: %1").arg(path);
        }
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = "The selected file is not a valid project document.";
        }
        return std::nullopt;
    }

    const QJsonObject root = document.object();
    ProjectState state;
    state.referencePresetIndex = root.value("reference_preset_index").toInt(0);
    state.comparisonActive = root.value("comparison_active").toBool(false);
    state.systemPreset = root.value("system_preset").toObject();
    state.simulationConfig = root.value("simulation_config").toObject();
    state.baselineConfig = root.value("baseline_config").toObject();
    state.baselineResult = root.value("baseline_result").toObject();
    state.activeResult = root.value("active_result").toObject();
    state.reportContext = root.value("report_context").toObject();
    state.validationSettings = root.value("validation_settings").toObject();
    state.validationResult = root.value("validation_result").toObject();
    state.cadDocument = root.value("cad_document").toObject();
    return state;
}

} // namespace trade_study
