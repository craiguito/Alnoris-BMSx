#include "BatteryVisualizationBuilder.h"

#include <algorithm>

namespace cad::battery {

BatteryVisualizationOverlay BatteryVisualizationBuilder::build(
    const core::CadDocument& document,
    const ElectricalConfig& electrical,
    const ThermalConfig& thermal
)
{
    BatteryVisualizationOverlay overlay;
    overlay.active_metric = BatteryVisualizationOverlay::Metric::Temperature;
    const int parallel_count = std::max(1, document.metadata().layout_config.cells_in_parallel);
    const double branch_current = electrical.discharge_current_a / static_cast<double>(parallel_count);
    const double ohmic_heat = branch_current * branch_current * electrical.internal_resistance_ohm;
    const double cooling_effect = std::max(0.15, thermal.cooling_coeff_w_per_k / 6.0);

    for (const CellEntity& cell : document.cells()) {
        const double gradient = cell.series_index * 0.9 + cell.parallel_index * 1.3;
        overlay.cell_temperature_c[cell.id] =
            thermal.ambient_temp_c + (ohmic_heat * 13.0 / cooling_effect) + gradient;
    }

    return overlay;
}

} // namespace cad::battery
