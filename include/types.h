#pragma once 
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct Material { // material parameters
  std::string name;
  float xi      = 1.0f;     // coherence length
  float lambda  = 2.0f;     // penetration depth
  float gamma   = 0.1f;     // relaxation rate
  float epsilon = 1.0f;     // ginzburg-landau parameter
  float u       = 5.79f;    // 

  bool isValid() const {
    return (xi > 0 && lambda > 0 && gamma > 0 && epsilon > 0);
  }
};

struct CellEdge {
  int   neighborIds;  // index of a neighbor cell
  float length;       // length of edge 
  float distance;     // distance of cell centers
  float weight;       // length / distance.  precomputed then sent to gpu in laplacian
};

struct Cell { // carries geometry of each cell in voronoi mesh
  int id = -1;      // a cell w/ id = -1 is unassigned

  // cell parameters
  glm::vec2 center;                 // center of cell
  float area = 0.0f;                // area of cell 
  std::vector<CellEdge> edges;      // set of all edges of cell
  std::vector<glm::vec2> vertices;  // points representing vertices of cell

  bool isValid() const {
    return (id >= 0 && area > 0 && !edges.empty());
  }
};


struct MeshRegion {
  enum class Type {
    BULK,       // normal superconductor
    VOID,       // non-superconductor
    ELECTRODE,  // electrode region (+) => cathode, (-) => anode
  };
  Type type = Type::BULK;
  int regionId = -1;
  
  bool isValid() const { return regionId >= 0; }
};

struct Polygon {
  std::string name;
  std::vector<glm::vec2> vertices;

  bool contains(glm::vec2 p) const; // overlap check logic
  
  bool isValid() const { return vertices.size() >= 3; }
};

struct FieldRamp{ // time dependent applied fields
  enum class Type { LINEAR, EXPONENTIAL, SIGMOID } type;

  float start_time  = 0.0f;
  float end_time    = 0.0f;
  glm::vec3 start_value;
  glm::vec3 end_value;

  bool isValid() {
    return start_time < end_time;
  }
};

struct Electrode : MeshRegion {
  float voltage         = 0.0f;
  float volume_current  = 0.0f;

  bool isValid() const {
    return type == Type::ELECTRODE;
  }
};
