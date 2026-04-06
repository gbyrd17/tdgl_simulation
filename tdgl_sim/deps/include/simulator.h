#pragma once
#include <device.h>
#include <glad/glad.h>
#include <memory>

class simulator {
  public:
    simulator(device& device, layer& layer, int resolution, int stepsPerFrame = 30);
    virtual ~simulator();

    // fundamental parameters
    const float mc = 2.0f;   // Cooper pair mass
    const float q  = 2.0f;   // Cooper pair charge

    double h;
    double dt;              // timestep determined as 0.05 * cfl criterio

    bool useNoise = false;
    int m_res, stepsPerFrame;
    float simTime;

    void step();
    void render();
    void quench();

    GLuint getDisplayTexture() const { return m_phiTextures[m_readIdx]; }
    void updateParams();

    void render(int renderMode);

  private:


    device& m_device;
    layer&  m_layer;


    GLuint m_compPID;
    GLuint m_rendPID;
    GLuint m_quadVAO;
    GLuint m_quadVBO;

    GLuint m_phiTextures[2];
    GLuint m_maskTexture;
    int m_readIdx   = 0;
    int m_writeIdx  = 0;

    void swapBuffers(layer& layer);
    void initRenderProg();
    // vbo handler potentially for later versions
    void initRenderQuad(); 

    void initTextures();
    void initShaders();
    void uploadMask();
};
