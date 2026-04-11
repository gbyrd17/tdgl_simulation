#pragma once
#include <glm/glm.hpp>
#include <vector>

// Simple mesh element: cell with vertices and neighbors
struct meshCell {
  int id;
  glm::vec2 center;
  std::vector<int> vertexIndices;
  std::vector<int> neighborIds;  // indices of adjacent cells
  float area;
};

// Base mesh class for general unstructured grids
class mesh {
  public:
    virtual ~mesh() = default;

    int resX = 0, resY = 0;
    glm::vec2 size;
    glm::vec2 spacing;
    std::vector<glm::vec2> vertices;
    std::vector<meshCell> cells;
    std::vector<float> mask;
    std::vector<glm::vec4> edgeWeights;  // normalized edge distances: (up, down, left, right)

    virtual void build() = 0;
    int getCellCount() const { return (int)cells.size(); }
    const meshCell& getCell(int id) const { return cells[id]; }
};

// Structured rectangular grid (regular finite difference mesh)
class structuredMesh : public mesh {
  public:
    structuredMesh(glm::vec2 worldSize, int resX, int resY);
    void build() override;
};

// Voronoi mesh (placeholder for future generalization)
class voronoiMesh : public mesh {
  public:
    voronoiMesh(glm::vec2 worldSize, int numSites);
    void build() override;
};
