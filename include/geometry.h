#pragma once
#include "types.h"

namespace geometry {
  Polygon rectangle(glm::vec2 center, glm::vec2 size);
  Polygon circle(glm::vec2 center, float radius, int num_segments = 64);
  Polygon triangle(glm::vec2 center, glm::vec2 size);
  Polygon ring(glm::vec2 center, float inner_radius, float outer_radius, int num_segments = 64);

  bool contains(const Polygon &poly, glm::vec2 p); // is p in poly
  glm::vec2 minBounds(const Polygon &poly);
  glm::vec2 maxBounds(const Polygon &poly);
  float area(const Polygon &poly);
};
