#include "PackLayoutGenerator.h"

#include <algorithm>

namespace cad::battery {

void PackLayoutGenerator::rebuildDocument(core::CadDocument& document, const PackLayoutConfig& config)
{
    document.clear();
    document.metadata().layout_config = config;

    const int series_count = std::max(1, config.cells_in_series);
    const int parallel_count = std::max(1, config.cells_in_parallel);
    const float pack_width = std::max(240.0f, (series_count - 1) * config.x_spacing + config.cell_radius * 2.8f);
    const float pack_depth = std::max(180.0f, (parallel_count - 1) * config.z_spacing + config.cell_radius * 2.8f);

    for (int row = 0; row < parallel_count; ++row) {
        for (int col = 0; col < series_count; ++col) {
            CellEntity cell;
            cell.label = "Cell";
            cell.position = {
                (col - (series_count - 1) / 2.0f) * config.x_spacing,
                0.0f,
                (row - (parallel_count - 1) / 2.0f) * config.z_spacing
            };
            cell.radius = config.cell_radius;
            cell.height = config.cell_height;
            cell.series_index = col;
            cell.parallel_index = row;
            document.addCell(cell);
        }
    }

    CoolingPlateEntity coolingPlate;
    coolingPlate.label = "Cooling plate";
    coolingPlate.center = {0.0f, -128.0f, 0.0f};
    coolingPlate.size = {pack_width + 72.0f, 24.0f, pack_depth + 92.0f};
    document.addCoolingPlate(coolingPlate);

    BusbarEntity negativeBusbar;
    negativeBusbar.label = "Negative busbar";
    negativeBusbar.center = {0.0f, 112.0f, -pack_depth * 0.5f};
    negativeBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    document.addBusbar(negativeBusbar);

    BusbarEntity positiveBusbar;
    positiveBusbar.label = "Positive busbar";
    positiveBusbar.center = {0.0f, 112.0f, pack_depth * 0.5f};
    positiveBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    document.addBusbar(positiveBusbar);

    ModuleBoundaryEntity moduleBoundary;
    moduleBoundary.label = "Module boundary";
    moduleBoundary.center = {0.0f, 0.0f, 0.0f};
    moduleBoundary.size = {pack_width + 96.0f, config.cell_height + 56.0f, pack_depth + 92.0f};
    document.addModuleBoundary(moduleBoundary);

    PackEnclosureEntity enclosure;
    enclosure.label = "Pack enclosure";
    enclosure.center = {0.0f, 0.0f, 0.0f};
    enclosure.size = {pack_width + 140.0f, config.cell_height + 92.0f, pack_depth + 136.0f};
    enclosure.wall_thickness = 8.0f;
    document.addPackEnclosure(enclosure);
}

} // namespace cad::battery
