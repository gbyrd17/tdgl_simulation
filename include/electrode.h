#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

#include "constants.h"
#include "mesh.h"

class Electrode {
  int id;
  MeshTraits::Type role;
  glm::vec2 position;
  float radius;
  float potential;
  bool isVoltageControlled;
};

class ElectrodeRegistry {
public:
  std::vector<Electrode> electrodes;
  std::unordered_map<int, int> id_to_idx;

  int add_electrode(const Electrode &e);
  Electrode *get_by_id(int id);
  std::vector<Electrode *> getNear(glm::vec2 pos, float search_radius);
  void apply_to_mesh(Mesh &target_mesh);

private:
};
