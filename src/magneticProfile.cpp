#include "magneticProfile.h"
#include <cmath>
#include <glm/ext/vector_float3.hpp>
#include <stdexcept>

MagneticProfile::MagneticProfile() : b_cached(0.0f), last_eval_time(-1.0f) {}

void MagneticProfile::addRamp(float t_start, float t_end, glm::vec3 b_initial,
                              glm::vec3 b_final, RampType type) {
  // validate ramp ------------------
  if (t_start >= t_end) {
    throw std::invalid_argument("MagneticProfile::addRamp(): t_start >= t_end");
  } // check if period is bad
  if (!ramps.empty()) {
    if (t_start < ramps.back().t_end) {
      throw std::invalid_argument(
          "MagneticProfile::addRamp(): overlapping ramps");
    }
  } // check if period overlaps with other ramps
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(b_initial[i]) || !std::isfinite(b_final[i])) {
      throw std::invalid_argument(
          "MagneticProfile::addRamp(): non-finite magnetic field value");
    }
  } // check for divergence
  // --------------------------------

  Ramp r{t_start, t_end, b_initial, b_final, type};
  ramps.push_back(r);
  last_eval_time = -1.0f;
}

float MagneticProfile::interpolate(float t, float t_start, float t_end,
                                   float val_start, float val_end,
                                   RampType type) const {
  if (t <= t_start) {
    return val_start;
  }
  if (t >= t_end) {
    return val_end;
  } // drop out if at start or end of ramp period

  switch (type) {
  case RampType::LINEAR: {
    float alpha = (t - t_start) / (t_end - t_start);
    return val_start + alpha * (val_end - val_start);
  }
  case RampType::EXPONENTIAL: {
    float alpha = (t - t_start) / (t_end - t_start);
    float decay = std::exp(-3.0 * alpha);
    return val_start + (1.0f - decay) * (val_end - val_start);
  }
  case RampType::SIGMOID: {
    float alpha = (t - t_start) / (t_end - t_start);
    float x = 10.0f * alpha - 5.0f; // range [-5, 5]
    float sigmoid = 1.0f / (1.0f + std::exp(-x));
    return val_start + sigmoid * (val_end - val_start);
  }
  }
}

const MagneticProfile::Ramp *MagneticProfile::whatRamp(float sim_time) const {
  for (const auto &r : ramps) {
    if (sim_time >= r.t_start && sim_time <= r.t_end) {
      return &r;
    } // figure out what ramp is happening at sim_time
  }
  return nullptr;
}

glm::vec3 MagneticProfile::evaluate(float sim_time) {
  // validation -----------------------------------
  if (sim_time < 0.0f) {
    throw std::invalid_argument("MagneticProfile::evaluate(): sim_time < 0");
  } // catch catastrophic error where sim_time < 0.

  if (sim_time == last_eval_time) {
    return b_cached;
  } // make sure we dont recalculate points we've already done

  if (ramps.empty()) {
    b_cached = glm::vec3(0.0f);
    last_eval_time = sim_time;
    return b_cached;
  } // make sure ramps aren't empty
  // ---------------------------------------------

  const Ramp *current_ramp = whatRamp(sim_time);

  if (current_ramp == nullptr) {
    b_cached = ramps.back().b_final;
    last_eval_time = sim_time;
    return b_cached;
  }

  glm::vec3 result;
  for (int i = 0; i < 3; ++i) {
    result[i] = interpolate(sim_time, current_ramp->t_start,
                            current_ramp->t_end, current_ramp->b_initial[i],
                            current_ramp->b_final[i], current_ramp->type);
  }
  b_cached = result;
  last_eval_time = sim_time;
  return b_cached;
}

float MagneticProfile::getDuration() const {
  if (ramps.empty())
    return 0.0f;
  return ramps.back().t_end;
}

bool MagneticProfile::validate() const {
  if (ramps.empty()) {
    return true;
  } // empty, fine.

  for (size_t i = 1; i < ramps.size(); ++i) {
    if (ramps[i].t_start < ramps[i - 1].t_end) {
      return false;
    } // overlapping ramp phases, error out.
  }

  for (const auto &r : ramps) {
    if (!r.isValid()) {
      return false;
    } // if ramps arent themselves valid, error out.
  }
  return true;
}
