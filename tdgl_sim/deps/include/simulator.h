#pragma once
#include <device.h>
#include <glad/glad.h>
#include <memory>

class simulator {
  public:
    simulator(device& device, int resolution);
    ~simulator();

    bool useNoise = false;
    void step(float dt);
    void render();
    void quench();

    GLuint getDisplayTexture() const { return m_phiTextures[m_readIdx]; }
    void updateParams();

  private:
    device& m_device;
    int m_res;

    unsigned int m_compPID;
    unsigned int m_rendPID;

    GLuint m_phiTextures[2];
    GLuint m_maskTexture;
    int m_readIdx   = 0;
    int m_writeIdx  = 0;

    void swapBuffers(layer& layer);
    void initTextures();
    void initShaders();
    void uploadMask();
};
