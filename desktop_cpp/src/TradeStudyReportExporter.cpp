#include "TradeStudyReportExporter.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace trade_study {
namespace {

QString formatMetricRow(const QString& metric, const QString& candidate, const QString& baseline, const QString& delta)
{
    return QString("| %1 | %2 | %3 | %4 |").arg(metric, candidate, baseline, delta);
}

QString formatValidationMetricRow(
    const QString& dataset,
    const QString& metric,
    const QString& value,
    const QString& threshold,
    const QString& status)
{
    return QString("| %1 | %2 | %3 | %4 | %5 |").arg(dataset, metric, value, threshold, status);
}

QString formatDelta(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

} // namespace

QString buildMarkdownReport(const ComparisonSummary& summary)
{
    QStringList lines;
    lines << "# Alnoris Trade-Study Report";
    lines << "";
    lines << QString("- Generated: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << QString("- Archetype: %1").arg(summary.candidate.archetypeName.isEmpty() ? "Unspecified" : summary.candidate.archetypeName);
    lines << QString("- Workflow: %1").arg(summary.candidate.workflowName.isEmpty() ? "Candidate Pack Study" : summary.candidate.workflowName);
    lines << "";
    lines << "## Recommendation";
    lines << "";
    lines << summary.recommendationSummary;
    if (!summary.recommendationDetails.isEmpty()) {
        lines << "";
        for (const QString& detail : summary.recommendationDetails) {
            lines << QString("- %1").arg(detail);
        }
    }

    lines << "";
    lines << "## Candidate vs Baseline";
    lines << "";
    lines << "| Metric | Candidate | Baseline | Delta |";
    lines << "| --- | --- | --- | --- |";
    lines << formatMetricRow(
        "Nominal voltage (V)",
        QString::number(summary.candidate.packNominalVoltageV, 'f', 2),
        summary.hasBaseline && summary.baseline.has_value() ? QString::number(summary.baseline->packNominalVoltageV, 'f', 2) : "Not captured",
        summary.hasBaseline ? formatDelta(summary.nominalVoltageDeltaV, 2) : "n/a");
    lines << formatMetricRow(
        "Delivered energy (Wh)",
        QString::number(summary.candidate.deliveredEnergyWh, 'f', 2),
        summary.hasBaseline && summary.baseline.has_value() ? QString::number(summary.baseline->deliveredEnergyWh, 'f', 2) : "Not captured",
        summary.hasBaseline ? formatDelta(summary.deliveredEnergyDeltaWh, 2) : "n/a");
    lines << formatMetricRow(
        "Pack capacity (Ah)",
        QString::number(summary.candidate.packCapacityAh, 'f', 2),
        summary.hasBaseline && summary.baseline.has_value() ? QString::number(summary.baseline->packCapacityAh, 'f', 2) : "Not captured",
        "n/a");
    lines << formatMetricRow(
        "Peak pack power (W)",
        QString::number(summary.candidate.peakPackPowerW, 'f', 1),
        summary.hasBaseline && summary.baseline.has_value() ? QString::number(summary.baseline->peakPackPowerW, 'f', 1) : "Not captured",
        "n/a");
    lines << formatMetricRow(
        "Max core temp (C)",
        QString::number(summary.candidate.maxCoreTempC, 'f', 2),
        summary.hasBaseline && summary.baseline.has_value() ? QString::number(summary.baseline->maxCoreTempC, 'f', 2) : "Not captured",
        summary.hasBaseline ? formatDelta(summary.maxCoreTempDeltaC, 2) : "n/a");
    lines << formatMetricRow(
        "SOC spread",
        QString::number(summary.candidate.socSpread, 'f', 4),
        summary.hasBaseline && summary.baseline.has_value() ? QString::number(summary.baseline->socSpread, 'f', 4) : "Not captured",
        summary.hasBaseline ? formatDelta(summary.socSpreadDelta, 4) : "n/a");
    lines << formatMetricRow(
        "Weakest group",
        summary.candidate.weakestGroup,
        summary.hasBaseline && summary.baseline.has_value() ? summary.baseline->weakestGroup : "Not captured",
        "n/a");
    lines << formatMetricRow(
        "Hottest group",
        summary.candidate.hottestGroup,
        summary.hasBaseline && summary.baseline.has_value() ? summary.baseline->hottestGroup : "Not captured",
        "n/a");
    lines << formatMetricRow(
        "Termination",
        summary.candidate.terminationReason,
        summary.hasBaseline && summary.baseline.has_value() ? summary.baseline->terminationReason : "Not captured",
        "n/a");

    if (!summary.candidate.validationSummary.isEmpty()) {
        lines << "";
        lines << "## Validation";
        lines << "";
        lines << QString("- Overall status: %1").arg(summary.candidate.validationSummary.value("overall_status").toString().toUpper());
        lines << QString("- Datasets evaluated: %1").arg(summary.candidate.validationSummary.value("dataset_count").toInt());
        lines << QString("- Passed datasets: %1").arg(summary.candidate.validationSummary.value("passed_count").toInt());
        lines << QString("- Failed datasets: %1").arg(summary.candidate.validationSummary.value("failed_count").toInt());
        const QString recommendation = summary.candidate.validationSummary.value("recommendation_text").toString();
        if (!recommendation.isEmpty()) {
            lines << QString("- Recommendation: %1").arg(recommendation);
        }
        lines << QString("- Strongest dataset: %1").arg(summary.candidate.validationSummary.value("strongest_dataset_display_name").toString("n/a"));
        lines << QString("- Weakest dataset: %1").arg(summary.candidate.validationSummary.value("weakest_dataset_display_name").toString("n/a"));
        lines << "";
        lines << "| Dataset | Metric | Value | Threshold | Status |";
        lines << "| --- | --- | --- | --- | --- |";
        for (const QJsonValue& scorecardValue : summary.candidate.validationScorecards) {
            const QJsonObject scorecard = scorecardValue.toObject();
            const QString datasetLabel = QString("%1 (%2)")
                .arg(scorecard.value("dataset_display_name").toString())
                .arg(scorecard.value("dataset_status").toString());
            for (const QJsonValue& metricValue : scorecard.value("metric_results").toArray()) {
                const QJsonObject metric = metricValue.toObject();
                const QString unit = metric.value("unit").toString();
                const double rawValue = metric.value("value").toDouble();
                const QString valueText = unit.isEmpty()
                    ? QString::number(rawValue, 'f', 4)
                    : QString("%1 %2").arg(QString::number(rawValue, 'f', unit == "fraction" ? 4 : 3), unit);
                const QString thresholdText = metric.value("threshold_value").isNull()
                    ? "n/a"
                    : (unit.isEmpty()
                          ? QString::number(metric.value("threshold_value").toDouble(), 'f', 4)
                          : QString("%1 %2").arg(QString::number(metric.value("threshold_value").toDouble(), 'f', unit == "fraction" ? 4 : 3), unit));
                const QString status = metric.value("passed").isBool()
                    ? (metric.value("passed").toBool() ? "PASS" : "FLAG")
                    : "INFO";
                lines << formatValidationMetricRow(
                    datasetLabel,
                    metric.value("display_name").toString(),
                    valueText,
                    thresholdText,
                    status);
            }
        }

        const QJsonArray validationWarnings = summary.candidate.validationSummary.value("key_warnings").toArray();
        if (!validationWarnings.isEmpty()) {
            lines << "";
            lines << "### Validation Notes";
            lines << "";
            for (const QJsonValue& warning : validationWarnings) {
                lines << QString("- %1").arg(warning.toString());
            }
        }
    }

    if (!summary.candidate.warnings.isEmpty()) {
        lines << "";
        lines << "## Candidate Warnings";
        lines << "";
        for (const QString& warning : summary.candidate.warnings) {
            lines << QString("- %1").arg(warning);
        }
    }

    if (summary.baseline.has_value() && !summary.baseline->warnings.isEmpty()) {
        lines << "";
        lines << "## Baseline Warnings";
        lines << "";
        for (const QString& warning : summary.baseline->warnings) {
            lines << QString("- %1").arg(warning);
        }
    }

    return lines.join('\n');
}

bool writeMarkdownReport(const QString& path, const ComparisonSummary& summary, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error != nullptr) {
            *error = QString("Could not write report file: %1").arg(path);
        }
        return false;
    }

    QTextStream stream(&file);
    stream << buildMarkdownReport(summary);
    return true;
}

} // namespace trade_study
