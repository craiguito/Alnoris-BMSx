#pragma once

#include "SimulationResultModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace trade_study {

struct ReportContext
{
    QString archetypeId;
    QString archetypeName;
    QString workflowId;
    QString workflowName;
    QString recommendationSummary;
    QStringList recommendationDetails;
    QJsonObject validationSummary;
    QJsonArray validationScorecards;
};

struct RunSummary
{
    bool valid = false;
    QString archetypeId;
    QString archetypeName;
    QString workflowId;
    QString workflowName;
    double packNominalVoltageV = 0.0;
    double packCapacityAh = 0.0;
    double deliveredEnergyWh = 0.0;
    double peakPackPowerW = 0.0;
    double maxCoreTempC = 0.0;
    double maxPackTempC = 0.0;
    double socSpread = 0.0;
    double runtimeS = 0.0;
    QString weakestGroup;
    QString hottestGroup;
    QString terminationReason;
    QString recommendationSummary;
    QStringList recommendationDetails;
    QStringList warnings;
    QJsonObject validationSummary;
    QJsonArray validationScorecards;
};

struct ComparisonSummary
{
    bool valid = false;
    bool hasBaseline = false;
    RunSummary candidate;
    std::optional<RunSummary> baseline;
    double deliveredEnergyDeltaWh = 0.0;
    double nominalVoltageDeltaV = 0.0;
    double maxCoreTempDeltaC = 0.0;
    double socSpreadDelta = 0.0;
    QString recommendationSummary;
    QStringList recommendationDetails;
};

ReportContext buildPackSimulationReportContext(
    const QString& archetypeId,
    const QString& archetypeName,
    const QString& workflowName);

ReportContext buildVirtualTestReportContext(
    const QString& archetypeId,
    const QString& archetypeName,
    const QJsonObject& payload);

RunSummary buildRunSummary(
    const desktop::SimulationResultModel& result,
    const ReportContext& context);

ComparisonSummary buildComparisonSummary(
    const desktop::SimulationResultModel& candidate,
    const std::optional<desktop::SimulationResultModel>& baseline,
    const ReportContext& context);

QString formatRunSummaryText(const RunSummary& summary);
QString formatComparisonSummaryText(const ComparisonSummary& summary);

QJsonObject reportContextToJson(const ReportContext& context);
ReportContext reportContextFromJson(const QJsonObject& object);

QJsonObject runSummaryToJson(const RunSummary& summary);
QJsonObject comparisonSummaryToJson(const ComparisonSummary& summary);

} // namespace trade_study
