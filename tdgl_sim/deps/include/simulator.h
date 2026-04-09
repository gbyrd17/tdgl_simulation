#pragma once
#include <device.h>
#include <glad/glad.h>

class simulator {
  public:
    simulator(device& device, layer& layer, int resolution, int stepsPerFrame = 30);
    virtual ~simulator();

    // fundamental parameters
    const float mc = 2.0f;   // Cooper pair mass
    const float q  = 2.0f;   // Cooper pair charge

    float h;
    float dt;              // timestep determined as 0.05 * cfl criterio

    bool useNoise = true;
    int m_res, stepsPerFrame;
    float simTime;

    float phiAvg = 0.0f;
    float phiMin = 0.0f;
    float phiMax = 0.0f;
    int phiStatCounter = 0;

    void step();
    void render();
    void quench();

    GLuint getDisplayTexture() const { return m_phiTextures[m_readIdx]; }
    void updateParams();

    void render(int renderMode);

  private:
    device& m_device;
    void updatePhiStats();
    layer&  m_layer;

    GLuint m_compPID;
    GLuint m_rendPID;
    GLuint m_quadVAO;
    GLuint m_quadVBO;

    GLuint m_phiTextures[2];
    GLuint m_maskTexture;
    int m_readIdx   = 0;
    int m_writeIdx  = 1;

    void swapBuffers(layer& layer);
    void initRenderProg();
    // vbo handler potentially for later versions
    void initRenderQuad(); 

    void initTextures();
    void initShaders();
    void uploadMask();
};
