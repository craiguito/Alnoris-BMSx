#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace trade_study {

struct ProjectState
{
    int referencePresetIndex = 0;
    bool comparisonActive = false;
    QJsonObject systemPreset;
    QJsonObject simulationConfig;
    QJsonObject baselineConfig;
    QJsonObject baselineResult;
    QJsonObject activeResult;
    QJsonObject reportContext;
    QJsonObject cadDocument;
};

bool saveProjectState(const QString& path, const ProjectState& state, QString* error);
std::optional<ProjectState> loadProjectState(const QString& path, QString* error);

} // namespace trade_study

