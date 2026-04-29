#pragma once
#include "device.h"
#include "mesh.h"
#include <glad/glad.h>
#include <memory>
#include <vector>

class simulator {
  public:
    simulator(device& device, layer& layer, int resX, int resY, int stepsPerFrame = 30);
    virtual ~simulator();

    // fundamental parameters
    const float mc = 2.0f;   // Cooper pair mass
    const float q  = 2.0f;   // Cooper pair charge

    float h;
    float dt;              // timestep determined as 0.05 * cfl criterio

    bool useNoise = true;
    int m_resX, m_resY, stepsPerFrame;
    float simTime;

    std::unique_ptr<mesh> m_mesh;

    float phiAvg = 0.0f;
    float phiMin = 0.0f;
    float phiMax = 0.0f;
    int phiStatCounter = 0;
    int vortexCount = 0;

    // Adaptive time stepping parameters
    const float dt_min = 1e-8f;
    float dt_max = 0.0f;

    int m_computeGroupSize = 32;
    int m_dtUpdateInterval = 100;  // Reduced readback frequency: was 10, now 100 (10x fewer stalls)
    int m_dtUpdateCounter = 0;
    int m_partitionSize = 512;  // Increased default: larger tiles = better GPU cache utilization
    int m_vortexCountInterval = 1000;  // Reduced vortex count frequency (was 100)
    int m_vortexCountCounter = 0;
    bool m_enableVortexCounting = true;  // User can disable for speed
    bool m_renderBothViewports = true;   // User can disable to render only one
    std::vector<float> m_phiBuffer;

    void step();
    void render();
    void quench();
    void quenchSeededLattice();
    
    // Mesh partitioning for parallel GPU computation
    void setPartitionSize(int size) { 
      m_partitionSize = std::max(32, size);  // minimum 32x32 tiles
      m_mesh->partition(m_partitionSize);
    }
    
    // Geometry application: apply shapes to mesh
    void applyGeometryRectangle(glm::vec2 center, glm::vec2 size);
    void applyGeometryCircle(glm::vec2 center, float radius);
    void applyGeometryTriangle(glm::vec2 center, float size);
    void applyGeometryPolygon(const polygon& shape);
    void clearGeometry();  // reset mesh mask to all 1s

    GLuint getDisplayTexture() const { return m_phiTextures[m_readIdx]; }
    void updateParams();

    void render(int renderMode);

  private:
    device& m_device;
    void updatePhiStats();
    int countVortices();    layer&  m_layer;

    float initDt;
    std::vector<float> prevPhi2; // for adaptive time stepping
    std::vector<float> maxChange; 
    void findTimeStep();

    GLuint m_compPID;
    GLuint m_rendPID;
    GLuint m_quadVAO;
    GLuint m_quadVBO;

    GLint m_compImgSamplerLoc;
    GLint m_compXiLoc;
    GLint m_compLambdaLoc;
    GLint m_compGammaLoc;
    GLint m_compQLoc;
    GLint m_compHLoc;
    GLint m_compLLoc;
    GLint m_compRelaxLoc;
    GLint m_compUseNoiseLoc;
    GLint m_compDtLoc;
    GLint m_compAlphaLoc;
    GLint m_compBFieldLoc;
    GLint m_compEpsilonLoc;
    GLint m_compTimeLoc;
    GLint m_compSurfaceVoltageLoc;
    GLint m_compSheetCurrentLoc;

    GLint m_rendFieldLoc;
    GLint m_rendMaskLoc;
    GLint m_rendRenderModeLoc;
    GLint m_rendHLoc;
    GLint m_rendAlphaLoc;
    GLint m_rendBetaLoc;
    GLint m_rendLLoc;
    GLint m_rendBFieldLoc;
    GLint m_rendQLoc;
    GLint m_rendEpsilonLoc;

    GLuint m_phiTextures[2];
    GLuint m_maskTexture;
    GLuint m_neighborTexture;  // stores neighbor connectivity: (up, down, left, right)
    GLuint m_edgeWeightTexture;  // stores normalized edge distances: (up, down, left, right)
    int m_readIdx   = 0;
    int m_writeIdx  = 1;

    void swapBuffers(layer& layer);
    void initRenderProg();
    // vbo handler potentially for later versions
    void initRenderQuad(); 

    void initTextures();
    void initShaders();
    void uploadMask();
    void buildNeighborTexture();
    void buildEdgeWeightTexture();
};
