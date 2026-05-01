#pragma once 
#include "types.h"
#include <vector>
#include <stdexcept>

class FieldProfile {
  private:
    std::vector<FieldRamp> ramps;

    mutable float cached_time = 1.0f;
    mutable glm::vec3 cached_value;

    glm::vec3 interpolate(const FieldRamp &r, float time) const;
  public:
    FieldProfile();

    void addRamp(const FieldRamp& ramp);

    glm::vec3 evaluate(float time);

    const std::vector<FieldRamp> &getRamps() const { return ramps; }
    float getDuration() const;

    bool isValid() const;
};
