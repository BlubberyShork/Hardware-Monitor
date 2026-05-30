#pragma once
#include <vector>

// TODO - Redesign this to be more scalabale and flexible, similar to Sensor in Libre

enum class GPUClockDomain {
    Graphics,       // Core / Graphics / GPU clock
    Memory,         // Memory / VRAM clock
    Video,          // Video / Media engine clock  
    Unknown
};

struct ClockEntry {
    GPUClockDomain  clk_type = GPUClockDomain::Unknown;
    double          clk_spd  = 0.0;
};

struct GPULiveData {
    // Temperatures
    int curr_avg_temp     = 0;
    int curr_hotspot_temp = 0;

    // Clock speeds
    std::vector<ClockEntry> clks;

    // Utilization
    unsigned int curr_graphics_utilization      = 0;
    unsigned int curr_frame_buffer_utilization  = 0;
    unsigned int curr_video_engine_utilization  = 0;

    // Fan speed
    unsigned int fan_speed = 0;
};