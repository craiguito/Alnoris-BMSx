#pragma once

#include "CadTypes.h"

namespace cad {

class Module
{
public:
    Module();

    bool set_cell_mesh_path(const std::string& path);
    void set_viewport_size(int width, int height);
    void set_pack_config(const PackConfig& config);
    void orbit(float delta_yaw_deg, float delta_pitch_deg);
    void zoom(float delta);
    void select_cell(int index);
    int hit_test_cell(float x, float y) const;

    const FrameData& frame_data() const;
    int selected_cell() const;

private:
    void rebuild_frame();
    double estimated_cell_temp(int index) const;
    Vec3 temperature_color(double temp_c) const;

    std::vector<Vec3> m_cell_mesh_vertices;
    PackConfig m_config;
    int m_viewport_width = 1;
    int m_viewport_height = 1;
    float m_yaw_deg = -32.0f;
    float m_pitch_deg = -18.0f;
    float m_zoom = 1.0f;
    int m_selected_cell = 0;
    FrameData m_frame;
};

} // namespace cad
