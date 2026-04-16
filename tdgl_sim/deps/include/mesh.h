#pragma once
#include <glm/glm.hpp>
#include <vector>

/*
 * 'meshCell' struct:
 * :----------------:
 * Struct allowing for id referencing of cells.  Used in mesh.build() methods and computation shader.
 */
struct meshCell {
  int id;
  glm::vec2 center;
  std::vector<int> vertexIndices;
  std::vector<int> neighborIds;  // indices of adjacent cells
  float area;
};

/*
 * 'meshPartition' struct:
 * :---------------------:
 * Defines a rectangular region of the mesh for parallel GPU computation
 * Interior cells are independent, boundary cells depend on adjacent partitions
 */
struct meshPartition {
  // Full partition bounds
  int startX, startY;    // top-left corner in grid coords
  int endX, endY;        // bottom-right corner (exclusive)
  int resX, resY;        // resolution of this partition
  int cellCount;         // total cells = resX * resY
  
  // Interior region (cells not on partition boundary, can be computed in parallel)
  int interiorStartX, interiorStartY;
  int interiorEndX, interiorEndY;
  int interiorResX, interiorResY;
  int interiorCellCount;
  
  // Boundary region (cells on edge, depend on adjacent partitions)
  int boundaryWidth;  // thickness of boundary layer (typically 1 for nearest neighbors)
};

/*
 * 'mesh' class:
 * :-----------:
 * Provides basic methods common to all mesh types.  Used as base class for our two mesh types.
 */
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
    std::vector<meshPartition> partitions;  // partitions for parallel GPU computation

    virtual void build() = 0;
    void partition(int partitionSize = 256);  // Create mesh partitions (default 256x256 tiles)
    int getCellCount() const { return (int)cells.size(); }
    const meshCell& getCell(int id) const { return cells[id]; }
};


/*
 * 'structuredMesh' class <== base: 'mesh' class:
 * :-------------------------------------------:
 * Structured mesh with regular finite difference.  Trivial edge structure.
 */
class structuredMesh : public mesh {
  public:
    structuredMesh(glm::vec2 worldSize, int resX, int resY);
    void build() override;
};

/* 
 * 'voronoiMesh' class <== base: 'mesh' class:
 * :-------------------------------------------:
 * This mesh type generates a Voronoi tessellation based on jittered sites placed on a regular grid.
 * Each cell's neighbors are determined by proximity, and edge weights are computed based on actual distances between cell centers.
 * This allows for more accurate representation of irregular geometries compared to a structured grid.
*/
class voronoiMesh : public mesh {
  public:
    voronoiMesh(glm::vec2 worldSize, int numSites);
    void build() override;
};

