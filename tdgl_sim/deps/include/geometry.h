#pragma once
#include <device.h>
#include <glm/glm.hpp>
#include <vector>

class geometry {
  public:
    // shape gen
    static polygon genRectangle(glm::vec2 center, glm::vec2 size);
    static polygon genRing(glm::vec2 center, float innerRad, float outerRad);
    static polygon genPolyVec(std::vector<glm::vec2> points);

    static std::vector<float> genMask(const layer& layer, int res, glm::vec2 worldSize);
    static void maskToLayer(layer& layer, const polygon& polygon);

  private:
    static bool isInside(glm::vec2 p, const polygon& poly);
};
