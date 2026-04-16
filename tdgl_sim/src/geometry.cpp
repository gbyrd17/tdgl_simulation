#include <geometry.h>
#include <vector>
#include <cmath>

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

// rasterization onto mesh cells
std::vector<float> geometry::genMask(const layer& layer, int res, glm::vec2 worldSize) {
  std::vector<float> mask(res * res, 0.0f);
  float dx = worldSize.x / res;
  float dy = worldSize.y / res;

  for (int y = 0; y < res; y++) {
    for (int x = 0; x < res; x++) {
      glm::vec2 p((x + 0.5f) * dx, (y + 0.5f) * dy);
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

polygon geometry::genCircle(glm::vec2 center, float radius, int numSegments) {
  polygon p;
  p.name      = "circle";
  p.isMesh    = true;
  p.vertices.clear();
  
  const float TAU = 6.28318530718f;  // 2π
  for (int i = 0; i < numSegments; ++i) {
    float angle = TAU * i / numSegments;
    float x = center.x + radius * cos(angle);
    float y = center.y + radius * sin(angle);
    p.vertices.push_back({x, y});
  }
  
  return p;
}

polygon geometry::genTriangle(glm::vec2 center, float size) {
  polygon p;
  p.name      = "triangle";
  p.isMesh    = true;
  
  // Equilateral triangle with apex at top
  const float PI = 3.14159265359f;
  const float TAU = 2.0f * PI;
  
  float h = size / (2.0f * sqrt(3.0f) / 2.0f);  // height
  float w = size;  // width (base)
  
  // Three vertices of equilateral triangle
  p.vertices = {
    {center.x,               center.y + h/3.0f},          // apex (top)
    {center.x - w/2.0f,      center.y - h/3.0f},          // bottom left
    {center.x + w/2.0f,      center.y - h/3.0f}           // bottom right
  };
  
  return p;
}

void geometry::applyPolygonToMesh(mesh& targetMesh, const polygon& shape) {
  // Rasterize polygon onto mesh cells
  for (int i = 0; i < (int)targetMesh.cells.size(); ++i) {
    const meshCell& cell = targetMesh.cells[i];
    if (isInside(cell.center, shape)) {
      targetMesh.mask[i] = shape.isMesh ? 1.0f : 0.0f;
    }
  }
}

void geometry::applyPolygonsToMesh(mesh& targetMesh, const std::vector<polygon>& shapes) {
  // Apply all polygons to mesh in order
  // Later polygons overwrite earlier ones
  for (const auto& shape : shapes) {
    applyPolygonToMesh(targetMesh, shape);
  }
}

polygon geometry::genRing(glm::vec2 center, float innerRad, float outerRad) {
  // Approximate annulus as polygon with outer circle vertices
  polygon p;
  p.name   = "ring";
  p.isMesh = true;
  
  const float TAU = 6.28318530718f;
  int numSegments = 32;
  
  for (int i = 0; i < numSegments; ++i) {
    float angle = TAU * i / numSegments;
    float x = center.x + outerRad * cos(angle);
    float y = center.y + outerRad * sin(angle);
    p.vertices.push_back({x, y});
  }
  
  return p;
}

polygon geometry::genPolyVec(std::vector<glm::vec2> points) {
  polygon p;
  p.name     = "custom_polygon";
  p.isMesh   = true;
  p.vertices = points;
  return p;
}

void geometry::maskToLayer(layer& layer, const polygon& polygon) {
  // Add polygon to layer's polygon list
  layer.polygons.push_back(polygon);
}
