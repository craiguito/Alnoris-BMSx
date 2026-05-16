#pragma once

#include "ChartTypes.h"

#include <optional>

namespace charts {

class Module
{
public:
    Module();

    void set_theme(const Theme& theme);
    void show_single_series(const std::string& title, const std::string& y_axis_title, const Series& series);
    void show_comparison(
        const std::string& title,
        const std::string& y_axis_title,
        const Series& baseline,
        const Series& candidate
    );
    void set_marker(std::optional<double> marker_x);

    const ViewModel& view_model() const;
    int nearest_primary_point(double x, double y, double x_scale, double y_scale, double plot_left, double plot_top, double plot_width, double plot_height) const;

private:
    ViewModel m_view;
};

} // namespace charts
