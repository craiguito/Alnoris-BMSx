#include "ChartModule.h"

#include <cmath>
#include <limits>

namespace charts {

Module::Module()
{
    m_view.primary.name = "Current Scenario";
    m_view.secondary.name = "Baseline";
    m_view.secondary.red = 123;
    m_view.secondary.green = 135;
    m_view.secondary.blue = 148;
    m_view.secondary.dashed = true;
}

void Module::set_theme(const Theme& theme)
{
    m_view.theme = theme;
}

void Module::show_single_series(const std::string& title, const std::string& y_axis_title, const Series& series)
{
    m_view.title = title;
    m_view.y_axis_title = y_axis_title;
    m_view.primary = series;
    m_view.secondary.points.clear();
    m_view.comparison_mode = false;
}

void Module::show_comparison(
    const std::string& title,
    const std::string& y_axis_title,
    const Series& baseline,
    const Series& candidate
)
{
    m_view.title = title;
    m_view.y_axis_title = y_axis_title;
    m_view.secondary = baseline;
    m_view.primary = candidate;
    m_view.comparison_mode = true;
}

void Module::set_marker(std::optional<double> marker_x)
{
    m_view.marker_enabled = marker_x.has_value();
    m_view.marker_x = marker_x.value_or(0.0);
}

const ViewModel& Module::view_model() const
{
    return m_view;
}

int Module::nearest_primary_point(double x, double y, double x_scale, double y_scale, double plot_left, double plot_top, double plot_width, double plot_height) const
{
    if (m_view.primary.points.empty()) {
        return -1;
    }

    double min_dist = std::numeric_limits<double>::max();
    int best_index = -1;
    for (int i = 0; i < static_cast<int>(m_view.primary.points.size()); ++i) {
        const Point& point = m_view.primary.points[i];
        const double px = plot_left + (point.x * x_scale * plot_width);
        const double py = plot_top + plot_height - (point.y * y_scale * plot_height);
        const double dx = px - x;
        const double dy = py - y;
        const double dist = std::sqrt((dx * dx) + (dy * dy));
        if (dist < min_dist) {
            min_dist = dist;
            best_index = i;
        }
    }

    return min_dist <= 28.0 ? best_index : -1;
}

} // namespace charts
