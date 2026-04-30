#pragma once
#include <cmath>

namespace Constants {
// Core mathematical constant
constexpr float PI = 3.14159265359f;

// TDGL physics parameters
constexpr float COOPER_PAIR_MASS = 2.0f;   // mc = 2.0 (dimensionless)
constexpr float COOPER_PAIR_CHARGE = 2.0f; // q = 2.0 (dimensionless)

// Time stepping constraints
constexpr float CFL_FACTOR = 0.05f;      // Courant-Friedrichs-Lewy factor
constexpr float DT_GROWTH_FACTOR = 5.0f; // dt_max = 5 * dt_init
constexpr float DT_MIN = 1e-8f;          // minimum adaptive timestep

// Mesh generation
constexpr float VORONOI_JITTER_FACTOR = 0.3f; // ±30% of spacing
constexpr int VORONOI_CIRCLE_SEGMENTS = 32;   // polygon approximation
constexpr int MAX_NEIGHBORS = 8;              //

// Electrode generation
constexpr int MAX_ELECTRODES = 16;

// Ramp generation
constexpr size_t MAX_RAMPS = 32;

// GPU/Rendering
constexpr int COMPUTE_GROUP_SIZE = 32;         // work group size (32x32)
constexpr int SHADER_LOG_SIZE = 1024;          // error message buffer
constexpr float TEXTURE_FILTER_DAMPING = 0.7f; // Jacobi relaxation factor

// Validation bounds
constexpr float INVALID_FIELD_THRESHOLD = 1e6f; // detect NaN/Inf
constexpr int MAX_TEXTURE_DIMENSION = 4096;     // GPU limit
} // namespace Constants
