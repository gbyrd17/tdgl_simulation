#include "device.h"
#include <simulator.h>
#include <geometry.h>
#include <iostream>

simulator::simulator(device& device, int resolution): m_device(device), m_res(resolution) {
  initTextures();
  initShaders();
  uploadMask();
  quench();
}

void simulator::initTextures() {
  glGenTextures(2, m_phiTextures);
  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, m_phiTextures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_res, m_res, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  glGenTextures(1, &m_maskTexture);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_res, m_res, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void simulator::uploadMask() {
  if (m_device.layers.empty()) return;

  std::vector<float> data = geometry::genMask(m_device.layers[0], m_res, m_device.worldSize);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_res, m_res, GL_RED, GL_FLOAT, data.data());
}

void simulator::step(float dt) {
  if (m_device.layers.empty()) return;
  layer& layer = m_device.layers[0];

  glUseProgram(m_compPID);

  // Bind textures to image units
  glBindImageTexture(0, m_phiTextures[m_readIdx], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
  glBindImageTexture(1, m_phiTextures[m_writeIdx], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
  glBindImageTexture(2, m_maskTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

  float h = m_device.worldSize.x / (float)m_res;
  // Set Uniforms (Ported from your main.cpp)
  // Note: Use m_device.layers[0].xi, etc.
  glUniform1f(glGetUniformLocation(m_compPID, "uXi"), layer.xi);
  glUniform1f(glGetUniformLocation(m_compPID, "uLambda"), layer.lambda);
  glUniform1f(glGetUniformLocation(m_compPID, "uGamma"), layer.gamma);
  glUniform1f(glGetUniformLocation(m_compPID, "uQ"), 1.0f);
  glUniform1f(glGetUniformLocation(m_compPID, "uH"), h);
  glUniform1f(glGetUniformLocation(m_compPID, "uL"), m_device.worldSize.x);
  glUniform1i(glGetUniformLocation(m_compPID, "uUseNoise"), simulator::useNoise);
  glUniform1f(glGetUniformLocation(m_compPID, "uDt"), dt);
  glUniform1f(glGetUniformLocation(m_compPID, "uBField"), m_device.externalB.z);

  glDispatchCompute(m_res / 16, m_res / 16, 1);

  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  // Swap buffers
  m_readIdx = 1 - m_readIdx;
  m_writeIdx = 1 - m_writeIdx;
}
