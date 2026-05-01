#pragma once
#include "types.h"
#include <memory>
#include <vector>

class Mesh {
public:
    static std::unique_ptr<Mesh> create(glm::vec2 size, int num_sites, int seed = 42);

    glm::vec2 getSize() const { return size; }
    int getCellCount() const { return static_cast<int>(cells.size()); }
    const Cell& getCell(int id) const;
    const MeshRegion& getRegion(int id) const;

    void setTraitRegion(const Polygon& shape, const MeshRegion& trait);

private:
    glm::vec2 size;
    std::vector<Cell> cells;
    std::vector<MeshRegion::Type> traits;
    std::vector<float> mask;

    Mesh(glm::vec2 size, int num_sites);
    
};
