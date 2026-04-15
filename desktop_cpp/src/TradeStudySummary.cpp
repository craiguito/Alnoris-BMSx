#include "TradeStudySummary.h"

#include <QJsonArray>

#include <algorithm>

namespace trade_study {
namespace {

QString formatNumber(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

double peakPackPowerW(const desktop::SimulationResultModel& result)
{
    double peakValue = 0.0;
    for (const desktop::SimulationTracePoint& point : result.points) {
        peakValue = std::max(peakValue, point.pack_power_w);
    }
    return peakValue;
}

QString groupNameOrFallback(const desktop::SimulationResultModel& result, int index)
{
    return index >= 0 ? result.groupLabel(index) : QString("--");
}

QStringList warningsFromResult(const desktop::SimulationResultModel& result)
{
    QStringList warnings;
    for (const desktop::SimulationWarningModel& warning : result.summary.warnings) {
        warnings << QString("[%1] %2").arg(warning.severity, warning.message);
    }
    return warnings;
}

QString defaultCandidateRecommendation(const RunSummary& summary)
{
    if (summary.maxCoreTempC <= 45.0 && summary.socSpread <= 0.03) {
        return "Candidate stays inside a stable thermal and balance window.";
    }
    if (summary.maxCoreTempC > 52.0) {
        return "Candidate should be reviewed for thermal headroom before recommending it.";
    }
    if (summary.socSpread > 0.05) {
        return "Candidate should be reviewed for balance spread before recommending it.";
    }
    return "Candidate is ready for baseline comparison.";
}

QString comparisonRecommendation(
    double deliveredEnergyDeltaWh,
    double maxCoreTempDeltaC,
    double socSpreadDelta)
{
    if (deliveredEnergyDeltaWh >= 0.0 && maxCoreTempDeltaC <= 0.0 && socSpreadDelta <= 0.0) {
        return "Candidate improves delivered energy without worsening thermal or balance risk.";
    }
    if (deliveredEnergyDeltaWh < 0.0 && maxCoreTempDeltaC > 0.0) {
        return "Baseline remains stronger on both delivered energy and thermal headroom.";
    }
    if (maxCoreTempDeltaC > 1.0 || socSpreadDelta > 0.01) {
        return "Candidate trades performance for added thermal or balance risk and should be reviewed carefully.";
    }
    if (deliveredEnergyDeltaWh > 0.0) {
        return "Candidate adds delivered energy with manageable trade-offs.";
    }
    return "Candidate stays close to the baseline and needs workflow-specific judgment.";
}

QJsonArray toJsonArray(const QStringList& items)
{
    QJsonArray array;
    for (const QString& item : items) {
        array.append(item);
    }
    return array;
}

} // namespace

ReportContext buildPackSimulationReportContext(
    const QString& archetypeId,
    const QString& archetypeName,
    const QString& workflowName)
{
    ReportContext context;
    context.archetypeId = archetypeId;
    context.archetypeName = archetypeName;
    context.workflowId = "pack_simulation";
    context.workflowName = workflowName;
    return context;
}

ReportContext buildVirtualTestReportContext(
    const QString& archetypeId,
    const QString& archetypeName,
    const QJsonObject& payload)
{
    ReportContext context;
    context.archetypeId = archetypeId;
    context.archetypeName = archetypeName;
    context.workflowId = payload.value("test_id").toString();
    context.workflowName = payload.value("test_name").toString("Flagship Virtual Test");

    const QJsonObject passFail = payload.value("pass_fail_indicators").toObject();
    int totalChecks = 0;
    int flaggedChecks = 0;
    for (auto it = passFail.begin(); it != passFail.end(); ++it) {
        ++totalChecks;
        const bool passed = it.value().toBool();
        if (!passed) {
            ++flaggedChecks;
        }
        context.recommendationDetails << QString("%1: %2").arg(it.key(), passed ? "pass" : "flagged");
    }

    const QJsonArray vettingWarnings = payload.value("vetting_result").toObject().value("warnings").toArray();
    for (const QJsonValue& value : vettingWarnings) {
        context.recommendationDetails << QString("Vetting warning: %1").arg(value.toString());
    }

    const QJsonArray runtimeWarnings = payload.value("warnings").toArray();
    for (const QJsonValue& value : runtimeWarnings) {
        context.recommendationDetails << QString("Runtime warning: %1").arg(value.toString());
    }

    if (totalChecks == 0) {
        context.recommendationSummary = context.recommendationDetails.isEmpty()
            ? "Flagship workflow completed without explicit pass/fail gates."
            : "Flagship workflow completed with review notes.";
    } else if (flaggedChecks == 0) {
        context.recommendationSummary = "Flagship workflow checks passed.";
    } else {
        context.recommendationSummary = QString("%1 of %2 flagship checks were flagged.")
            .arg(flaggedChecks)
            .arg(totalChecks);
    }

    return context;
}

RunSummary buildRunSummary(
    const desktop::SimulationResultModel& result,
    const ReportContext& context)
{
    RunSummary summary;
    summary.valid = result.valid;
    summary.archetypeId = context.archetypeId;
    summary.archetypeName = context.archetypeName;
    summary.workflowId = context.workflowId;
    summary.workflowName = context.workflowName;
    summary.packNominalVoltageV = result.pack_nominal_voltage_v;
    summary.packCapacityAh = result.pack_capacity_ah;
    summary.deliveredEnergyWh = result.summary.delivered_energy_wh;
    summary.peakPackPowerW = peakPackPowerW(result);
    summary.maxCoreTempC = result.summary.max_core_temp_c;
    summary.maxPackTempC = result.summary.max_group_temp_c;
    summary.socSpread = result.summary.soc_spread;
    summary.runtimeS = result.summary.runtime_s;
    summary.weakestGroup = groupNameOrFallback(result, result.summary.weakest_group_index);
    summary.hottestGroup = groupNameOrFallback(result, result.summary.hottest_group_index);
    summary.terminationReason = result.summary.termination_reason;
    summary.warnings = warningsFromResult(result);
    summary.recommendationSummary = context.recommendationSummary.isEmpty()
        ? defaultCandidateRecommendation(summary)
        : context.recommendationSummary;
    summary.recommendationDetails = context.recommendationDetails;
    return summary;
}

ComparisonSummary buildComparisonSummary(
    const desktop::SimulationResultModel& candidate,
    const std::optional<desktop::SimulationResultModel>& baseline,
    const ReportContext& context)
{
    ComparisonSummary summary;
    summary.valid = candidate.valid;
    summary.candidate = buildRunSummary(candidate, context);
    summary.hasBaseline = baseline.has_value() && baseline->valid;

    if (summary.hasBaseline) {
        summary.baseline = buildRunSummary(
            *baseline,
            buildPackSimulationReportContext(context.archetypeId, context.archetypeName, "Baseline Reference"));
        summary.deliveredEnergyDeltaWh = summary.candidate.deliveredEnergyWh - summary.baseline->deliveredEnergyWh;
        summary.nominalVoltageDeltaV = summary.candidate.packNominalVoltageV - summary.baseline->packNominalVoltageV;
        summary.maxCoreTempDeltaC = summary.candidate.maxCoreTempC - summary.baseline->maxCoreTempC;
        summary.socSpreadDelta = summary.candidate.socSpread - summary.baseline->socSpread;
        summary.recommendationSummary = context.recommendationSummary.isEmpty()
            ? comparisonRecommendation(summary.deliveredEnergyDeltaWh, summary.maxCoreTempDeltaC, summary.socSpreadDelta)
            : context.recommendationSummary;
    } else {
        summary.recommendationSummary = context.recommendationSummary.isEmpty()
            ? summary.candidate.recommendationSummary
            : context.recommendationSummary;
    }

    summary.recommendationDetails = context.recommendationDetails;
    if (summary.hasBaseline) {
        summary.recommendationDetails << QString("Delivered energy delta: %1 Wh").arg(formatNumber(summary.deliveredEnergyDeltaWh, 2));
        summary.recommendationDetails << QString("Nominal voltage delta: %1 V").arg(formatNumber(summary.nominalVoltageDeltaV, 2));
        summary.recommendationDetails << QString("Max core temp delta: %1 C").arg(formatNumber(summary.maxCoreTempDeltaC, 2));
        summary.recommendationDetails << QString("SOC spread delta: %1").arg(formatNumber(summary.socSpreadDelta, 4));
    }
    return summary;
}

QString formatRunSummaryText(const RunSummary& summary)
{
    QStringList lines;
    if (!summary.archetypeName.isEmpty()) {
        lines << QString("Archetype: %1").arg(summary.archetypeName);
    }
    if (!summary.workflowName.isEmpty()) {
        lines << QString("Workflow: %1").arg(summary.workflowName);
    }
    lines << QString("Pack nominal voltage: %1 V").arg(formatNumber(summary.packNominalVoltageV, 2));
    lines << QString("Pack capacity: %1 Ah").arg(formatNumber(summary.packCapacityAh, 2));
    lines << QString("Delivered energy: %1 Wh").arg(formatNumber(summary.deliveredEnergyWh, 2));
    lines << QString("Peak pack power: %1 W").arg(formatNumber(summary.peakPackPowerW, 1));
    lines << QString("Max core temperature: %1 C").arg(formatNumber(summary.maxCoreTempC, 2));
    lines << QString("Max pack temperature: %1 C").arg(formatNumber(summary.maxPackTempC, 2));
    lines << QString("SOC spread: %1").arg(formatNumber(summary.socSpread, 4));
    lines << QString("Weakest group: %1").arg(summary.weakestGroup);
    lines << QString("Hottest group: %1").arg(summary.hottestGroup);
    lines << QString("Runtime: %1 s").arg(formatNumber(summary.runtimeS, 0));
    lines << QString("Termination: %1").arg(summary.terminationReason.isEmpty() ? "n/a" : summary.terminationReason);
    lines << QString("Recommendation: %1").arg(summary.recommendationSummary);
    if (!summary.recommendationDetails.isEmpty()) {
        lines << "Review notes:";
        for (const QString& detail : summary.recommendationDetails) {
            lines << QString("- %1").arg(detail);
        }
    }
    if (!summary.warnings.isEmpty()) {
        lines << "Warnings:";
        for (const QString& warning : summary.warnings) {
            lines << QString("- %1").arg(warning);
        }
    }
    return lines.join('\n');
}

QString formatComparisonSummaryText(const ComparisonSummary& summary)
{
    if (!summary.hasBaseline) {
        return formatRunSummaryText(summary.candidate);
    }

    QStringList lines;
    lines << QString("Archetype: %1").arg(summary.candidate.archetypeName);
    lines << QString("Workflow: %1").arg(summary.candidate.workflowName);
    lines << "Candidate vs Baseline";
    lines << QString("Nominal voltage: %1 V vs %2 V")
                 .arg(formatNumber(summary.candidate.packNominalVoltageV, 2),
                      formatNumber(summary.baseline->packNominalVoltageV, 2));
    lines << QString("Delivered energy: %1 Wh vs %2 Wh")
                 .arg(formatNumber(summary.candidate.deliveredEnergyWh, 2),
                      formatNumber(summary.baseline->deliveredEnergyWh, 2));
    lines << QString("Max core temperature: %1 C vs %2 C")
                 .arg(formatNumber(summary.candidate.maxCoreTempC, 2),
                      formatNumber(summary.baseline->maxCoreTempC, 2));
    lines << QString("SOC spread: %1 vs %2")
                 .arg(formatNumber(summary.candidate.socSpread, 4),
                      formatNumber(summary.baseline->socSpread, 4));
    lines << QString("Weakest group: %1 | Baseline: %2")
                 .arg(summary.candidate.weakestGroup, summary.baseline->weakestGroup);
    lines << QString("Hottest group: %1 | Baseline: %2")
                 .arg(summary.candidate.hottestGroup, summary.baseline->hottestGroup);
    lines << QString("Delivered energy delta: %1 Wh").arg(formatNumber(summary.deliveredEnergyDeltaWh, 2));
    lines << QString("Nominal voltage delta: %1 V").arg(formatNumber(summary.nominalVoltageDeltaV, 2));
    lines << QString("Max core temp delta: %1 C").arg(formatNumber(summary.maxCoreTempDeltaC, 2));
    lines << QString("SOC spread delta: %1").arg(formatNumber(summary.socSpreadDelta, 4));
    lines << QString("Recommendation: %1").arg(summary.recommendationSummary);
    if (!summary.recommendationDetails.isEmpty()) {
        lines << "Review notes:";
        for (const QString& detail : summary.recommendationDetails) {
            lines << QString("- %1").arg(detail);
        }
    }
    if (!summary.candidate.warnings.isEmpty()) {
        lines << "Candidate warnings:";
        for (const QString& warning : summary.candidate.warnings) {
            lines << QString("- %1").arg(warning);
        }
    }
    if (summary.baseline.has_value() && !summary.baseline->warnings.isEmpty()) {
        lines << "Baseline warnings:";
        for (const QString& warning : summary.baseline->warnings) {
            lines << QString("- %1").arg(warning);
        }
    }
    return lines.join('\n');
}

QJsonObject reportContextToJson(const ReportContext& context)
{
    QJsonArray details;
    for (const QString& detail : context.recommendationDetails) {
        details.append(detail);
    }

    return QJsonObject{
        {"archetype_id", context.archetypeId},
        {"archetype_name", context.archetypeName},
        {"workflow_id", context.workflowId},
        {"workflow_name", context.workflowName},
        {"recommendation_summary", context.recommendationSummary},
        {"recommendation_details", details},
    };
}

ReportContext reportContextFromJson(const QJsonObject& object)
{
    ReportContext context;
    context.archetypeId = object.value("archetype_id").toString();
    context.archetypeName = object.value("archetype_name").toString();
    context.workflowId = object.value("workflow_id").toString();
    context.workflowName = object.value("workflow_name").toString();
    context.recommendationSummary = object.value("recommendation_summary").toString();
    for (const QJsonValue& value : object.value("recommendation_details").toArray()) {
        context.recommendationDetails << value.toString();
    }
    return context;
}

QJsonObject runSummaryToJson(const RunSummary& summary)
{
    QJsonArray details;
    for (const QString& detail : summary.recommendationDetails) {
        details.append(detail);
    }
    QJsonArray warnings;
    for (const QString& warning : summary.warnings) {
        warnings.append(warning);
    }

    return QJsonObject{
        {"valid", summary.valid},
        {"archetype_id", summary.archetypeId},
        {"archetype_name", summary.archetypeName},
        {"workflow_id", summary.workflowId},
        {"workflow_name", summary.workflowName},
        {"pack_nominal_voltage_v", summary.packNominalVoltageV},
        {"pack_capacity_ah", summary.packCapacityAh},
        {"delivered_energy_wh", summary.deliveredEnergyWh},
        {"peak_pack_power_w", summary.peakPackPowerW},
        {"max_core_temp_c", summary.maxCoreTempC},
        {"max_pack_temp_c", summary.maxPackTempC},
        {"soc_spread", summary.socSpread},
        {"runtime_s", summary.runtimeS},
        {"weakest_group", summary.weakestGroup},
        {"hottest_group", summary.hottestGroup},
        {"termination_reason", summary.terminationReason},
        {"recommendation_summary", summary.recommendationSummary},
        {"recommendation_details", details},
        {"warnings", warnings},
    };
}

QJsonObject comparisonSummaryToJson(const ComparisonSummary& summary)
{
    QJsonArray details;
    for (const QString& detail : summary.recommendationDetails) {
        details.append(detail);
    }

    QJsonObject object{
        {"valid", summary.valid},
        {"has_baseline", summary.hasBaseline},
        {"candidate", runSummaryToJson(summary.candidate)},
        {"delivered_energy_delta_wh", summary.deliveredEnergyDeltaWh},
        {"nominal_voltage_delta_v", summary.nominalVoltageDeltaV},
        {"max_core_temp_delta_c", summary.maxCoreTempDeltaC},
        {"soc_spread_delta", summary.socSpreadDelta},
        {"recommendation_summary", summary.recommendationSummary},
        {"recommendation_details", details},
    };
    if (summary.baseline.has_value()) {
        object.insert("baseline", runSummaryToJson(*summary.baseline));
    }
    return object;
}

} // namespace trade_study
