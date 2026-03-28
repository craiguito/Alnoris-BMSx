#include "SimulationResultModel.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace desktop {
namespace {

std::vector<double> toDoubleVector(const QJsonValue& value)
{
    std::vector<double> output;
    const QJsonArray array = value.toArray();
    output.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue& item : array) {
        output.push_back(item.toDouble());
    }
    return output;
}

std::vector<int> toIntVector(const QJsonValue& value)
{
    std::vector<int> output;
    const QJsonArray array = value.toArray();
    output.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue& item : array) {
        output.push_back(item.toInt());
    }
    return output;
}

QStringList toStringList(const QJsonValue& value)
{
    QStringList output;
    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        output.push_back(item.toString());
    }
    return output;
}

double valueWithFallback(const QJsonObject& object, const char* primary_key, const char* fallback_key, double fallback_value = 0.0)
{
    if (object.contains(primary_key)) {
        return object.value(primary_key).toDouble(fallback_value);
    }
    if (fallback_key != nullptr && object.contains(fallback_key)) {
        return object.value(fallback_key).toDouble(fallback_value);
    }
    return fallback_value;
}

QString stringWithFallback(const QJsonObject& object, const char* primary_key, const char* fallback_key)
{
    if (object.contains(primary_key)) {
        return object.value(primary_key).toString();
    }
    if (fallback_key != nullptr && object.contains(fallback_key)) {
        return object.value(fallback_key).toString();
    }
    return {};
}

} // namespace

int SimulationResultModel::groupCount() const
{
    if (!group_labels.isEmpty()) {
        return group_labels.size();
    }
    if (!group_entity_ids.isEmpty()) {
        return group_entity_ids.size();
    }
    for (const SimulationTracePoint& point : points) {
        if (!point.group_soc.empty()) {
            return static_cast<int>(point.group_soc.size());
        }
        if (!point.group_temp_c.empty()) {
            return static_cast<int>(point.group_temp_c.size());
        }
        if (!point.group_voltage_v.empty()) {
            return static_cast<int>(point.group_voltage_v.size());
        }
    }
    return 0;
}

int SimulationResultModel::clampedPointIndex(int index) const
{
    if (points.empty()) {
        return -1;
    }
    return std::clamp(index, 0, static_cast<int>(points.size()) - 1);
}

const SimulationTracePoint* SimulationResultModel::pointAt(int index) const
{
    const int clamped = clampedPointIndex(index);
    if (clamped < 0) {
        return nullptr;
    }
    return &points[static_cast<std::size_t>(clamped)];
}

QString SimulationResultModel::groupLabel(int index) const
{
    if (index >= 0 && index < group_labels.size()) {
        return group_labels.at(index);
    }
    return QString("Group %1").arg(index + 1);
}

SimulationResultModel parseSimulationResultPayload(const QJsonObject& payload)
{
    SimulationResultModel model;
    model.pack_nominal_voltage_v = payload.value("pack_nominal_voltage_v").toDouble();
    model.pack_capacity_ah = payload.value("pack_capacity_ah").toDouble();
    model.theoretical_energy_wh = payload.value("theoretical_energy_wh").toDouble();
    model.group_labels = toStringList(payload.value("group_labels"));
    model.group_entity_ids = toStringList(payload.value("group_entity_ids"));

    const QJsonObject summary = payload.value("summary").toObject();
    model.summary.delivered_energy_wh = summary.value("delivered_energy_wh").toDouble();
    model.summary.delivered_capacity_ah = summary.value("delivered_capacity_ah").toDouble();
    model.summary.peak_temp_c = valueWithFallback(summary, "peak_temp_c", "max_group_temp_c");
    model.summary.min_terminal_voltage_v = valueWithFallback(summary, "min_terminal_voltage_v", "min_group_voltage_v");
    model.summary.runtime_s = summary.value("runtime_s").toDouble();
    model.summary.final_soc_avg = valueWithFallback(summary, "final_soc_avg", "final_soc");
    model.summary.soc_spread = summary.value("soc_spread").toDouble();
    model.summary.max_group_temp_c = valueWithFallback(summary, "max_group_temp_c", "peak_temp_c");
    model.summary.min_group_voltage_v = valueWithFallback(summary, "min_group_voltage_v", "min_terminal_voltage_v");
    model.summary.capacity_retention = valueWithFallback(summary, "capacity_retention", "estimated_capacity_retention", 1.0);
    model.summary.resistance_growth = valueWithFallback(summary, "resistance_growth", "estimated_resistance_growth", 0.0);
    model.summary.weakest_group_index = summary.value("weakest_group_index").toInt(-1);
    model.summary.hottest_group_index = summary.value("hottest_group_index").toInt(-1);
    model.summary.hottest_zone_id = summary.value("hottest_zone_id").toInt(-1);
    model.summary.max_zone_temp_c = summary.value("max_zone_temp_c").toDouble();
    model.summary.termination_reason = stringWithFallback(summary, "termination_reason", nullptr);

    const QJsonArray time_series = payload.value("time_series").toArray();
    model.points.reserve(static_cast<std::size_t>(time_series.size()));
    for (const QJsonValue& value : time_series) {
        const QJsonObject point = value.toObject();
        SimulationTracePoint trace;
        trace.time_s = point.value("time_s").toDouble();
        trace.current_a = point.value("current_a").toDouble(payload.value("discharge_current_a").toDouble());
        trace.pack_voltage_v = valueWithFallback(point, "pack_voltage_v", "terminal_voltage_v");
        trace.pack_power_w = valueWithFallback(point, "pack_power_w", "power_w");
        trace.pack_heat_w = valueWithFallback(point, "pack_heat_w", "heat_w");
        trace.soc_avg = valueWithFallback(point, "soc_avg", "soc");
        trace.soc_min = valueWithFallback(point, "soc_min", "soc", trace.soc_avg);
        trace.soc_max = valueWithFallback(point, "soc_max", "soc", trace.soc_avg);
        trace.temp_avg_c = valueWithFallback(point, "temp_avg", "temp_c");
        trace.temp_max_c = valueWithFallback(point, "temp_max", "temp_c", trace.temp_avg_c);
        trace.group_soc = toDoubleVector(point.value("group_soc"));
        trace.group_temp_c = toDoubleVector(point.value("group_temp"));
        trace.group_voltage_v = toDoubleVector(point.value("group_voltage"));
        trace.group_zone_ids = toIntVector(point.value("group_zone_ids"));
        model.points.push_back(std::move(trace));
    }

    model.valid = !model.points.empty();
    if (!model.valid) {
        model.error = "Simulation payload did not contain a usable time_series.";
    }
    return model;
}

} // namespace desktop
