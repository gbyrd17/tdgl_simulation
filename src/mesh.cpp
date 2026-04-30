#pragma once
#include "mesh.h"

namespace GeometryUtils {
/**
 * @brief Robust Winding Number implementation for dependable point-in-polygon
 * tests. Encapsulated here to prevent global namespace pollution.
 */
static int calculate_winding_number(glm::vec2 p,
                                    const std::vector<glm::vec2> &poly) {
  int wn = 0;
  const size_t n = poly.size();

  for (size_t i = 0; i < n; ++i) {
    size_t next = (i + 1) % n;

    if (poly[i].y <= p.y) {
      if (poly[next].y > p.y) {
        float isect = (poly[next].x - poly[i].x) * (p.y - poly[i].y) -
                      (p.x - poly[i].x) * (poly[next].y - poly[i].y);
        if (isect > 0)
          wn++;
      }
    } else {
      if (poly[next].y <= p.y) {
        float isect = (poly[next].x - poly[i].x) * (p.y - poly[i].y) -
                      (p.x - poly[i].x) * (poly[next].y - poly[i].y);
        if (isect < 0)
          wn--;
      }
    }
  }
  return wn;
}

/**
 * @brief Computes Axis-Aligned Bounding Box (AABB) for a polygon.
 * Optimization: Prevents O(N*M) complexity on every shape application.
 */
static void get_poly_bounds(const std::vector<glm::vec2> &poly, glm::vec2 &min,
                            glm::vec2 &max) {
  min = glm::vec2(std::numeric_limits<float>::max());
  max = glm::vec2(std::numeric_limits<float>::lowest());
  for (const auto &v : poly) {
    min = glm::min(min, v);
    max = glm::max(max, v);
  }
}
} // namespace GeometryUtils

void Mesh::apply_shape(const std::vector<glm::vec2> &polygon,
                       MeshTraits::Trait trait) {
  if (polygon.size() < 3)
    return; // Dependable check: Polygons need at least 3 vertices

  glm::vec2 pMin, pMax;
  GeometryUtils::get_poly_bounds(polygon, pMin, pMax);

  for (size_t i = 0; i < cells.size(); ++i) {
    const glm::vec2 &pos = cells[i].center;

    // Optimization: Fast AABB rejection
    if (pos.x < pMin.x || pos.x > pMax.x || pos.y < pMin.y || pos.y > pMax.y) {
      continue;
    }

    if (GeometryUtils::calculate_winding_number(pos, polygon) != 0) {
      traits[i] = trait;

      // sync mask with the physical trait
      // VOID is only truly inactive meshtype
      mask[i] = (trait.type == MeshTraits::Type::VOID) ? 0.0f : 1.0f;
    }
  }
}

void StructuredMesh::build() {
  const uint32_t total_cells = resX * resY;

  cells.assign(total_cells, MeshCell{});
  traits.assign(total_cells, MeshTraits::Trait{});
  mask.assign(total_cells, 1.0f);

  const float dx = world_size.x / static_cast<float>(resX);
  const float dy = world_size.y / static_cast<float>(resY);
  const float cell_area = dx * dy;

  for (uint32_t y = 0; y < resY; ++y) {
    for (uint32_t x = 0; x < resX; ++x) {
      const uint32_t idx = y * resX + x;
      MeshCell &cell = cells[idx];

      // centers are offset to pixel-centers
      cell.center = glm::vec2((static_cast<float>(x) + 0.5f) * dx,
                              (static_cast<float>(y) + 0.5f) * dy);
      cell.area = cell_area;

      // finite difference stencil
      int32_t count = 0;

      if (x > 0) // left
        cell.neighbors[count++] = static_cast<int32_t>(idx - 1);
      if (x < resX - 1) // right
        cell.neighbors[count++] = static_cast<int32_t>(idx + 1);
      if (y > 0) // up
        cell.neighbors[count++] = static_cast<int32_t>(idx - resX);
      if (y < resY - 1) // down
        cell.neighbors[count++] = static_cast<int32_t>(idx + resX);

      cell.neighbor_count = count;

      // initialize remaining slots to empty (-1)
      for (int32_t i = count; i < Constants::MAX_NEIGHBORS; ++i) {
        cell.neighbors[i] = -1;
      }
    }
  }
}

void VoronoiMesh::build() {
  // Note for Research Cases:
  // A production Voronoi builder usually requires a robust Delaunay
  // Triangulation library (like Triangle or Boost.Polygon). Manual
  // implementation here would risk 'floating point slivers' that crash the
  // solver.

  // For now, we stub this out to ensure the factory functions.
  cells.resize(resX * resY); // ResX*ResY as a target count
  traits.resize(cells.size());
  mask.resize(cells.size(), 1.0f);

  // TODO: Integrate a robust Fortune's algorithm or Lloyd's relaxation here.
}
