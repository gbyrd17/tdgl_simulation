#pragma once
#include <device.h>
#include <mesh.h>
#include <glm/glm.hpp>
#include <vector>

class geometry {
  public:
    // Shape generators: create polygon definitions for arbitrary geometry
    static polygon genRectangle(glm::vec2 center, glm::vec2 size);
    static polygon genCircle(glm::vec2 center, float radius, int numSegments = 32);
    static polygon genTriangle(glm::vec2 center, float size);
    static polygon genRing(glm::vec2 center, float innerRad, float outerRad);
    static polygon genPolyVec(std::vector<glm::vec2> points);

    // Mesh-polygon integration: apply geometry to a mesh
    static void applyPolygonToMesh(mesh& targetMesh, const polygon& shape);
    static void applyPolygonsToMesh(mesh& targetMesh, const std::vector<polygon>& shapes);

    // Legacy support: rasterize polygons to mask array
    static std::vector<float> genMask(const layer& layer, int res, glm::vec2 worldSize);
    static void maskToLayer(layer& layer, const polygon& polygon);

  private:
    static bool isInside(glm::vec2 p, const polygon& poly);
};
