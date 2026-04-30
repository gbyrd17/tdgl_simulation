#pragma once
#include "constants.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace MeshTraits {
enum class Type : uint8_t { BULK, VOID, SOURCE, SINK, BOUNDARY, DEFECT };

struct Trait {
  Type type = Type::BULK;
  int32_t electrode_id = -1;
  float parameter = 0.0f;
};
} // namespace MeshTraits

struct MeshCell {
  glm::vec2 center;
  float area;
  int32_t neighbor_count;
  int32_t neighbors[Constants::MAX_NEIGHBORS];
};

class Mesh {
public:
  virtual ~Mesh() = default;

  glm::vec2 world_size;
  uint32_t resX, resY;

  std::vector<MeshCell> cells;
  std::vector<MeshTraits::Trait> traits;
  std::vector<float>
      mask; // should be defined in traits as part of the meshtrait type

  virtual void build() = 0;

  void apply_shape(const std::vector<glm::vec2> &polygon,
                   MeshTraits::Trait trait);
};

class StructuredMesh : public Mesh {
public:
  StructuredMesh(glm::vec2 size, uint32_t rx, uint32_t ry) {
    world_size = size;
    resX = rx;
    resY = ry;
  }
  void build() override;
};

class VoronoiMesh : public Mesh {
public:
  VoronoiMesh(glm::vec2 size, uint32_t count) {
    world_size = size; // count determines resolution
  }
  void build() override;
};

class MeshFactory {
public:
  enum class MeshType { RECTANGULAR, VORONOI };

  static std::unique_ptr<Mesh> create(MeshType mt, glm::vec2 size,
                                      uint32_t res) {
    std::unique_ptr<Mesh> m;
    if (mt == MeshType::RECTANGULAR) {
      m = std::make_unique<StructuredMesh>(size, res, res);
    } else {
      m = std::make_unique<VoronoiMesh>(size, res * res);
    }
    m->build();
    return m;
  }
};
