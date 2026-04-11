#include <mesh.h>
#include <cmath>
#include <random>

// Structured mesh: regular rectangular grid
structuredMesh::structuredMesh(glm::vec2 worldSize, int resX_, int resY_) {
  resX = resX_;
  resY = resY_;
  size = worldSize;
  spacing = glm::vec2(worldSize.x / resX, worldSize.y / resY);
  build();
}

void structuredMesh::build() {
  cells.clear();
  vertices.clear();

  // Create cell centers at grid points
  int cellId = 0;
  for (int y = 0; y < resY; y++) {
    for (int x = 0; x < resX; x++) {
      meshCell cell;
      cell.id = cellId++;
      cell.center = glm::vec2(
        (x + 0.5f) * spacing.x,
        (y + 0.5f) * spacing.y
      );
      cell.area = spacing.x * spacing.y;

      // 4-neighbor connectivity (up, down, left, right)
      int left = (x - 1 + resX) % resX;
      int right = (x + 1) % resX;
      int down = (y - 1 + resY) % resY;
      int up = (y + 1) % resY;

      cell.neighborIds = {
        up * resX + x,        // up
        down * resX + x,      // down
        y * resX + left,      // left
        y * resX + right      // right
      };

      cells.push_back(cell);
    }
  }

  mask.assign(resX * resY, 1.0f);

  // Compute edge weights (all uniform for structured grid)
  float cellSpacing = glm::length(spacing);  // Effective grid spacing
  edgeWeights.clear();
  for (int i = 0; i < resX * resY; i++) {
    // For structured mesh, all neighbors are at distance = spacing
    float w = 1.0f;  // normalized weight (distance / spacing = 1)
    edgeWeights.push_back(glm::vec4(w, w, w, w));  // (up, down, left, right)
  }
}

// Voronoi mesh: jittered grid with computed neighbors
voronoiMesh::voronoiMesh(glm::vec2 worldSize, int numSites) {
  size = worldSize;
  
  // Simple grid layout of Voronoi sites
  int gridSide = (int)sqrt((float)numSites);
  resX = gridSide;
  resY = gridSide;
  spacing = glm::vec2(worldSize.x / gridSide, worldSize.y / gridSide);
  
  build();
}

void voronoiMesh::build() {
  cells.clear();
  vertices.clear();

  // Generate jittered Voronoi sites on a grid
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> jitter(-spacing.x * 0.3f, spacing.x * 0.3f);

  int cellId = 0;
  for (int y = 0; y < resY; y++) {
    for (int x = 0; x < resX; x++) {
      meshCell cell;
      cell.id = cellId++;
      
      // Grid position with random jitter
      float baseX = (x + 0.5f) * spacing.x;
      float baseY = (y + 0.5f) * spacing.y;
      cell.center = glm::vec2(baseX + jitter(gen), baseY + jitter(gen));
      
      // Clamp to domain
      cell.center.x = glm::clamp(cell.center.x, 0.0f, size.x);
      cell.center.y = glm::clamp(cell.center.y, 0.0f, size.y);
      
      cell.area = spacing.x * spacing.y;

      // Simple 4-neighbor connectivity (approximation for Voronoi)
      int left = (x - 1 + resX) % resX;
      int right = (x + 1) % resX;
      int down = (y - 1 + resY) % resY;
      int up = (y + 1) % resY;

      cell.neighborIds = {
        up * resX + x,        // up
        down * resX + x,      // down
        y * resX + left,      // left
        y * resX + right      // right
      };

      cells.push_back(cell);
    }
  }

  mask.assign(resX * resY, 1.0f);

  // Compute edge weights based on actual distances in Voronoi mesh
  float refDistance = glm::length(spacing);  // Reference distance for normalization
  edgeWeights.clear();
  
  for (int idx = 0; idx < (int)cells.size(); idx++) {
    const meshCell& cell = cells[idx];
    glm::vec4 weights(1.0f);  // default
    
    if (cell.neighborIds.size() >= 4) {
      for (int i = 0; i < 4; i++) {
        int neighborId = cell.neighborIds[i];
        if (neighborId >= 0 && neighborId < (int)cells.size()) {
          const meshCell& neighbor = cells[neighborId];
          glm::vec2 delta = neighbor.center - cell.center;
          float distance = glm::length(delta);
          weights[i] = distance / refDistance;  // normalize by reference spacing
        }
      }
    }
    
    edgeWeights.push_back(weights);
  }
}
