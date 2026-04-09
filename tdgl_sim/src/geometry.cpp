#include <geometry.h>
#include <vector>

// ray casting 
bool geometry::isInside(glm::vec2 p, const polygon& poly) {
  bool inside = false;
  size_t n = poly.vertices.size();
  if (n < 3) return false;

  for (size_t i = 0, j = n-1; i < n; j = i++) {
    const auto& vi = poly.vertices[i];
    const auto& vj = poly.vertices[j];

    bool intersect = ((vi.y > p.y) != (vj.y > p.y)) &&
        (p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x);

    if (intersect) inside = !inside;
  };
  return inside;
}

// rasterization
std::vector<float> geometry::genMask(const layer& layer, int res, glm::vec2 worldSize) {
  std::vector<float> mask(res * res, 0.0f);
  float dx = worldSize.x / res;
  float dy = worldSize.y / res;

  for (int y = 0; y < res; y++) {
    for (int x = 0; x < res; x++) {
      glm::vec2 p(x * dx, y * dy);

      for (const auto& poly : layer.polygons) {
        if (isInside(p, poly)) {
          mask[y * res + x] = poly.isMesh ? 1.0f : 0.0f;
        }
      }
    }
  }
  return mask;
}

polygon geometry::genRectangle(glm::vec2 center, glm::vec2 size) {
  polygon p;
  p.name      = "rectangle";
  p.isMesh    = true;
  p.vertices  = {
    {center.x - size.x/2, center.y - size.y/2},
    {center.x + size.x/2, center.y - size.y/2},
    {center.x + size.x/2, center.y + size.y/2},
    {center.x - size.x/2, center.y + size.y/2}
  };
  return p;
}
