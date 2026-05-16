#pragma once

#include <string>
#include <vector>

namespace charts {

struct Point
{
    double x = 0.0;
    double y = 0.0;
};

struct Series
{
    std::string name;
    std::vector<Point> points;
    int red = 14;
    int green = 165;
    int blue = 233;
    bool dashed = false;
    bool visible = true;
};

struct Theme
{
    int surface_r = 193;
    int surface_g = 202;
    int surface_b = 212;
    int border_r = 127;
    int border_g = 143;
    int border_b = 159;
    int grid_r = 134;
    int grid_g = 150;
    int grid_b = 168;
    int text_r = 17;
    int text_g = 35;
    int text_b = 58;
};

struct ViewModel
{
    std::string title = "Graph";
    std::string subtitle = "Interactive simulation trace";
    std::string y_axis_title = "Value";
    std::string x_axis_title = "Time (s)";
    Series primary;
    Series secondary;
    bool comparison_mode = false;
    bool marker_enabled = false;
    double marker_x = 0.0;
    Theme theme;
};

} // namespace charts
