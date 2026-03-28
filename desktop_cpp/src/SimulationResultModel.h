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
    double pack_temp_avg_c = 0.0;
    double pack_temp_max_c = 0.0;
    std::vector<double> group_soc;
    std::vector<double> group_voltage_v;
    std::vector<double> group_core_temp_c;
    std::vector<double> group_surface_temp_c;
    std::vector<double> group_heat_w;
    std::vector<double> group_hysteresis_v;
    std::vector<double> group_diffusion_stress;
    std::vector<double> group_effective_resistance_ohm;
    std::vector<int> group_zone_ids;
    QStringList group_labels;
    QStringList group_entity_ids;
    int weakest_group_index = -1;
    int hottest_group_index = -1;
};

struct SimulationWarningModel
{
    QString code;
    QString message;
    QString severity;
};

struct SimulationSummaryModel
{
    double delivered_energy_wh = 0.0;
    double delivered_capacity_ah = 0.0;
    double runtime_s = 0.0;
    double final_soc_avg = 0.0;
    double soc_spread = 0.0;
    double max_group_temp_c = 0.0;
    double min_group_voltage_v = 0.0;
    double capacity_retention = 1.0;
    double resistance_growth = 0.0;
    double max_core_temp_c = 0.0;
    double max_surface_temp_c = 0.0;
    double temp_gradient_max_c = 0.0;
    double max_diffusion_stress = 0.0;
    double total_balance_ah = 0.0;
    double max_zone_temp_c = 0.0;
    int weakest_group_index = -1;
    int hottest_group_index = -1;
    int hottest_zone_id = -1;
    int first_faulted_group_index = -1;
    int fault_count = 0;
    bool balancing_used = false;
    bool profile_used = false;
    QString termination_reason;
    QString chemistry_name;
    QStringList enabled_nonlinear_features;
    std::vector<SimulationWarningModel> warnings;
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
    [[nodiscard]] QString groupEntityId(int index) const;
};

SimulationResultModel parseSimulationResultPayload(const QJsonObject& payload);

} // namespace desktop
