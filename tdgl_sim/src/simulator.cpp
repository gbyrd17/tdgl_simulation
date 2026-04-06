#include "device.h"
#include <simulator.h>
#include <geometry.h>
#include <iostream>
#include <random>

// constructor
simulator::simulator(device& device, layer& layer, int resolution, int stepsPerFrame) : 
                     m_device(device), m_layer(layer), m_res(resolution), stepsPerFrame(stepsPerFrame) 
{
  m_res = resolution;
  simTime = 0;

  h  = device.worldSize.x * m_res; 
  dt = 0.05f * ((h * h) / (4 * layer.xi * layer.xi * layer.gamma));

  initTextures();
  initShaders();
  initRenderQuad();
  uploadMask();
  quench();
}

// destructor
simulator::~simulator() {
  glDeleteTextures(2, m_phiTextures);
  glDeleteTextures(1, &m_maskTexture);

  glDeleteBuffers(1, &m_quadVBO);
  glDeleteVertexArrays(1, &m_quadVAO);

  glDeleteProgram(m_compPID);
  glDeleteProgram(m_rendPID);
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

void simulator::step() {
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

  // increment simTime
  simTime += dt;
}

void simulator::quench() {
  useNoise = true;

  std::vector<float> initData(m_res * m_res * 4, 0.0f);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(0,2.0f*3.14159265f);

  for(int i = 0; i< (m_res * m_res); i++){
      float theta = dist(gen);
      initData[i * 4 + 0] = cos(theta);
      initData[i * 4 + 1] = sin(theta);
      initData[i * 4 + 2] = 0.0f;
      initData[i * 4 + 3] = 0.0f;
  }

  for(int i = 0; i < 2;i++){
      glBindTexture(GL_TEXTURE_2D, m_phiTextures[i]);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_res, m_res, GL_RGBA, GL_FLOAT, initData.data());
  }
  useNoise = false; // only in initial quench
};

void simulator::initRenderQuad() {
  float quadVertices[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
  };

  glGenVertexArrays(1, &m_quadVAO);
  glGenBuffers(1, &m_quadVBO);

  glBindVertexArray(m_quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

  // position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

  // texture coord attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4* sizeof(float), (void*)(2*sizeof(float)));
}


void simulator::render(int renderMode) {
  glUseProgram(m_rendPID);

  // bind current read texture and mask
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glUniform1i(glGetUniformLocation(m_rendPID, "uMask"), 1);

  // pass current calculation
  float h       = m_device.worldSize.x / (float)m_res;
  float alpha   = 1.0f / (4.0f * mc * m_layer.xi * m_layer.xi);
  float beta    = (q * q * m_layer.lambda * m_layer.lambda * alpha) / (mc);

  glUniform1i(glGetUniformLocation(m_rendPID, "uRenderMode"), renderMode);
  glUniform1i(glGetUniformLocation(m_rendPID,          "uH"),          h);
  glUniform1i(glGetUniformLocation(m_rendPID,      "uAlpha"),      alpha);
  glUniform1i(glGetUniformLocation(m_rendPID,       "uBeta"),       beta);
  glUniform1i(glGetUniformLocation(m_rendPID,          "uL"), m_device.worldSize.x);
  glUniform1i(glGetUniformLocation(m_rendPID,       "uExtB"), m_device.externalB.z);
  glUniform1i(glGetUniformLocation(m_rendPID,          "uQ"),       1.0f);

  glBindVertexArray(m_quadVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
