#include "device.h"
#include <iterator>
#include <simulator.h>
#include <geometry.h>
#include <iostream>
#include <fstream>
#include <random>

std::string readFile(const char* path) {
  std::ifstream file(path);
  if(!file) { std::cerr << "Failed to open " << path << "\n"; return ""; }
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void checkShader(GLuint shader, std::string type) {
  GLint success;
  char infoLog[1024];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 1024, NULL, infoLog);
    std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
  } else { std::cout << "SUCCESS::COMPILED shader of type: " << type << std::endl; }
}

void checkProgram(GLuint pid, std::string type) {
  GLint success;
  char log[1024];
  glGetProgramiv(pid, GL_LINK_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(pid, 1024, NULL, log);
    std::cerr << "ERROR::PROGRAM_LINKTIME_ERROR of type: " << type << "\n" << log << "\n -- --------------------------------------------------- -- " << std::endl;
  } else { std::cout << "SUCCESS::INITIALIZED program of type: " << type << std::endl; }
}

void checkFile(std::string file, std::string type) {
  if (file.empty()) {
    std::cout << "ERROR::DIRECTORY_NOT_FOUND of file: " << file << "of type: " << type << std::endl;
  } else { std::cout << "SUCCESS::FOUND file of type: " << type << std::endl; }
}

// constructor
simulator::simulator(device& device, layer& layer, int resolution, int stepsPerFrame) : 
                     m_device(device), m_layer(layer), m_res(resolution), stepsPerFrame(stepsPerFrame) 
{
  m_res = resolution;
  simTime = 0;

  h  = device.worldSize.x / (float)m_res; 
  dt = 0.05f * h * h;  // Use a physically reasonable TDGL timestep for observable amplitude growth
  std::cout << "dt=" << dt << " h=" << h << std::endl;
  initTextures();
  initShaders();
  initRenderQuad();
  uploadMask();
  quench();
  std::cout << "SUCCESS::INITIALIZED simulator" << std::endl;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  glGenTextures(1, &m_maskTexture);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_res, m_res, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  std::cout << "generated textures successfully" << std::endl;
}

void simulator::initShaders() {
  // load and compile compute shader
  std::string compSrc = readFile("../shaders/comp.glsl");
  checkFile(compSrc, "COMPILE SHADER");
  const char* compPtr = compSrc.c_str();
  GLuint compShader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(compShader, 1, &compPtr, NULL);
  glCompileShader(compShader);
  checkShader(compShader, "COMPUTE");

  m_compPID = glCreateProgram();
  glAttachShader(m_compPID, compShader);
  glLinkProgram(m_compPID);
  checkProgram(m_compPID, "COMPUTE");
  glDeleteShader(compShader);

  // load and compile render shaders
  std::string vertSrc = readFile("../shaders/vert.glsl");
  checkFile(vertSrc, "VERTEX SHADER");
  std::string fragSrc = readFile("../shaders/frag.glsl");
  checkFile(fragSrc, "FRAGMENT SHADER");

  const char* vertPtr = vertSrc.c_str();
  const char* fragPtr = fragSrc.c_str();

  GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertShader, 1, &vertPtr, NULL);
  glCompileShader(vertShader);

  GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragShader, 1, &fragPtr, NULL);
  glCompileShader(fragShader);

  m_rendPID = glCreateProgram();
  glAttachShader(m_rendPID, vertShader);
  glAttachShader(m_rendPID, fragShader);
  glLinkProgram(m_rendPID);
  checkProgram(m_rendPID, "RENDER");

  glDeleteShader(vertShader);
  glDeleteShader(fragShader);
}

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
  std::cout << "SUCCESS::RENDER of type: QUAD" << std::endl;
}

void simulator::uploadMask() {
  if (m_device.layers.empty()) return;

  std::vector<float> data = geometry::genMask(m_device.layers[0], m_res, m_device.worldSize);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_res, m_res, GL_RED, GL_FLOAT, data.data());
}

void simulator::step() {
  if (m_device.layers.empty()) return;

  glUseProgram(m_compPID);

  // Bind textures to image units
  glBindImageTexture(0, m_phiTextures[m_readIdx],   0, GL_FALSE, 0, GL_READ_ONLY,   GL_RGBA32F);
  glBindImageTexture(1, m_phiTextures[m_writeIdx],  0, GL_FALSE, 0, GL_WRITE_ONLY,  GL_RGBA32F);
  glBindImageTexture(2, m_maskTexture,              0, GL_FALSE, 0, GL_READ_ONLY,   GL_R32F);
  
  // Bind texture sampler
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glUniform1i(glGetUniformLocation(m_compPID, "img_sampler"), 0);

  // Set Uniforms (Ported from your main.cpp)
  // Note: Use m_device.layers[0].xi, etc.
  float alpha   = 1.0f / (4.0f * mc * m_layer.xi * m_layer.xi);
  glUniform1f(glGetUniformLocation(m_compPID, "uXi"),       m_layer.xi);
  glUniform1f(glGetUniformLocation(m_compPID, "uLambda"),   m_layer.lambda);
  glUniform1f(glGetUniformLocation(m_compPID, "uGamma"),    m_layer.gamma);
  glUniform1f(glGetUniformLocation(m_compPID, "uQ"),        q);
  glUniform1f(glGetUniformLocation(m_compPID, "uH"),        h);
  glUniform1f(glGetUniformLocation(m_compPID, "uL"),        m_device.worldSize.x);
  glUniform1f(glGetUniformLocation(m_compPID, "uRelax"),    m_layer.u);
  glUniform1i(glGetUniformLocation(m_compPID, "uUseNoise"), useNoise);
  glUniform1f(glGetUniformLocation(m_compPID, "uDt"),       dt);
  glUniform1f(glGetUniformLocation(m_compPID, "uAlpha"),    alpha);
  glUniform1f(glGetUniformLocation(m_compPID, "uBField"),   m_device.externalB.z);
  glUniform1f(glGetUniformLocation(m_compPID, "uEpsilon"),  m_layer.epsilon);
  glUniform1f(glGetUniformLocation(m_compPID, "uTime"),      simTime);

  int computeJobs = m_res/16;
  glDispatchCompute(computeJobs, computeJobs, 1);

  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
  glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
  glBindImageTexture(2, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

  // Swap buffers
  std::swap(m_readIdx, m_writeIdx); 

  // increment simTime
  simTime += dt;

  // Update phi magnitude statistics periodically for diagnostics
  phiStatCounter += 1;
  if (phiStatCounter >= 30) {
    updatePhiStats();
    phiStatCounter = 0;
  }
}

void simulator::updatePhiStats() {
  std::vector<float> buffer(m_res * m_res * 4);
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, buffer.data());

  float minVal = 1e9f;
  float maxVal = 0.0f;
  double sumVal = 0.0;
  int count = m_res * m_res;

  for (int i = 0; i < count; ++i) {
    float x = buffer[i * 4 + 0];
    float y = buffer[i * 4 + 1];
    float mag = sqrt(x * x + y * y);
    sumVal += mag;
    minVal = std::min(minVal, mag);
    maxVal = std::max(maxVal, mag);
  }

  phiAvg = float(sumVal / double(count));
  phiMin = minVal;
  phiMax = maxVal;
}

void simulator::quench() {
  // Use a small-amplitude random seed, not sustained thermal noise, so the system
  // can transition to its natural vortex state under the applied field.
  useNoise = false;
  std::vector<float> initData(m_res * m_res * 4, 0.0f);

  float phiEq   = sqrt(m_layer.epsilon);
  float phiAmp  = 0.1f * phiEq; // start near zero with small random perturbations
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

  for(int i = 0; i < (m_res * m_res); i++) {
      initData[i * 4 + 0] = phiAmp * dist(gen);
      initData[i * 4 + 1] = phiAmp * dist(gen);
      initData[i * 4 + 2] = 0.0f;
      initData[i * 4 + 3] = 0.5f;
  }

  for(int i = 0; i < 2;i++){
      glBindTexture(GL_TEXTURE_2D, m_phiTextures[i]);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_res, m_res, GL_RGBA, GL_FLOAT, initData.data());
  }
  std::cout << "!! SYSTEM QUENCHED !!" << std::endl;
  simTime = 0;
};


void simulator::render(int renderMode) {
  glUseProgram(m_rendPID);

  // bind current read texture and mask
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glUniform1i(glGetUniformLocation(m_rendPID, "uField"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glUniform1i(glGetUniformLocation(m_rendPID, "uMask"), 1);

  // pass current calculation
  float h       = m_device.worldSize.x / (float)m_res;
  float alpha   = 1.0f / (4.0f * mc * m_layer.xi * m_layer.xi);
  float beta    = (q * q * m_layer.lambda * m_layer.lambda * alpha) / (mc);

  glUniform1i(glGetUniformLocation(m_rendPID, "uRenderMode"), renderMode);
  glUniform1f(glGetUniformLocation(m_rendPID,          "uH"),          h);
  glUniform1f(glGetUniformLocation(m_rendPID,      "uAlpha"),      alpha);
  glUniform1f(glGetUniformLocation(m_rendPID,       "uBeta"),       beta);
  glUniform1f(glGetUniformLocation(m_rendPID,          "uL"), m_device.worldSize.x);
  glUniform1f(glGetUniformLocation(m_rendPID,     "uBField"), m_device.externalB.z);
  glUniform1f(glGetUniformLocation(m_rendPID,          "uQ"),          q);
  glUniform1f(glGetUniformLocation(m_rendPID,    "uEpsilon"), m_layer.epsilon);

  glBindVertexArray(m_quadVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
