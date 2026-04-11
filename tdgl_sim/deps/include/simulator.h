#pragma once
#include <device.h>
#include <mesh.h>
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
    int vortexCountCounter = 0;

    // Adaptive time stepping parameters
    const float dt_min = 1e-8f;
    float dt_max = 0.0f;

    int m_computeGroupSize = 32;
    int m_dtUpdateInterval = 10;
    int m_dtUpdateCounter = 0;
    std::vector<float> m_phiBuffer;

    void step();
    void render();
    void quench();
    void quenchSeededLattice();

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
