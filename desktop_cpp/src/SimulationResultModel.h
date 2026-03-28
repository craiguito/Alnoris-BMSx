#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace desktop {

struct SimulationTracePoint
{
    double time_s = 0.0;
    double current_a = 0.0;
    double pack_voltage_v = 0.0;
    double pack_power_w = 0.0;
    double pack_heat_w = 0.0;
    double soc_avg = 0.0;
    double soc_min = 0.0;
    double soc_max = 0.0;
    double temp_avg_c = 0.0;
    double temp_max_c = 0.0;
    std::vector<double> group_soc;
    std::vector<double> group_temp_c;
    std::vector<double> group_voltage_v;
    std::vector<int> group_zone_ids;
};

struct SimulationSummaryModel
{
    double delivered_energy_wh = 0.0;
    double delivered_capacity_ah = 0.0;
    double peak_temp_c = 0.0;
    double min_terminal_voltage_v = 0.0;
    double runtime_s = 0.0;
    double final_soc_avg = 0.0;
    double soc_spread = 0.0;
    double max_group_temp_c = 0.0;
    double min_group_voltage_v = 0.0;
    double capacity_retention = 1.0;
    double resistance_growth = 0.0;
    int weakest_group_index = -1;
    int hottest_group_index = -1;
    int hottest_zone_id = -1;
    double max_zone_temp_c = 0.0;
    QString termination_reason;
};

struct SimulationResultModel
{
    bool valid = false;
    QString error;
    double pack_nominal_voltage_v = 0.0;
    double pack_capacity_ah = 0.0;
    double theoretical_energy_wh = 0.0;
    QStringList group_labels;
    QStringList group_entity_ids;
    std::vector<SimulationTracePoint> points;
    SimulationSummaryModel summary;

    [[nodiscard]] bool hasPoints() const { return !points.empty(); }
    [[nodiscard]] int pointCount() const { return static_cast<int>(points.size()); }
    [[nodiscard]] int groupCount() const;
    [[nodiscard]] int clampedPointIndex(int index) const;
    [[nodiscard]] const SimulationTracePoint* pointAt(int index) const;
    [[nodiscard]] QString groupLabel(int index) const;
};

SimulationResultModel parseSimulationResultPayload(const QJsonObject& payload);

} // namespace desktop
