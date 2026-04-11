#include "device.h"
#include <iterator>
#include <simulator.h>
#include <geometry.h>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <algorithm>
#include <memory>
#include "cmath"

#define PI 3.14159265359f

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
simulator::simulator(device& device, layer& layer, int resX, int resY, int stepsPerFrame) : 
                     m_device(device), m_layer(layer), stepsPerFrame(stepsPerFrame) 
{
  simTime = 0;
  m_mesh = std::make_unique<structuredMesh>(m_device.worldSize, resX, resY);
  h = m_mesh->spacing.x;
  dt = (0.05f * h * h);  // Use a physically reasonable TDGL timestep for observable amplitude growth
  initDt = dt;
  dt_max = 5 * dt;         // needs to be set by cfl criterion
  std::cout << "dt=" << dt << " h=" << h << " L=" << m_mesh->size.x << " res=" << m_mesh->resX << "x" << m_mesh->resY << std::endl;

  prevPhi2.resize(m_mesh->resX * m_mesh->resY, 0.0f);
  m_phiBuffer.resize(m_mesh->resX * m_mesh->resY * 4);

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
  glDeleteTextures(1, &m_neighborTexture);
  glDeleteTextures(1, &m_edgeWeightTexture);

  glDeleteBuffers(1, &m_quadVBO);
  glDeleteVertexArrays(1, &m_quadVAO);

  glDeleteProgram(m_compPID);
  glDeleteProgram(m_rendPID);
}

void simulator::initTextures() {
  glGenTextures(2, m_phiTextures);
  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, m_phiTextures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_mesh->resX, m_mesh->resY, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  glGenTextures(1, &m_maskTexture);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_mesh->resX, m_mesh->resY, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  std::cout << "generated textures successfully" << std::endl;

  // Build neighbor connectivity texture
  buildNeighborTexture();
  
  // Build edge weight texture
  buildEdgeWeightTexture();
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

  // cache compute uniform locations
  m_compImgSamplerLoc = glGetUniformLocation(m_compPID, "img_sampler");
  m_compXiLoc        = glGetUniformLocation(m_compPID, "uXi");
  m_compLambdaLoc    = glGetUniformLocation(m_compPID, "uLambda");
  m_compGammaLoc     = glGetUniformLocation(m_compPID, "uGamma");
  m_compQLoc         = glGetUniformLocation(m_compPID, "uQ");
  m_compHLoc         = glGetUniformLocation(m_compPID, "uH");
  m_compLLoc         = glGetUniformLocation(m_compPID, "uL");
  m_compRelaxLoc     = glGetUniformLocation(m_compPID, "uRelax");
  m_compUseNoiseLoc  = glGetUniformLocation(m_compPID, "uUseNoise");
  m_compDtLoc        = glGetUniformLocation(m_compPID, "uDt");
  m_compAlphaLoc     = glGetUniformLocation(m_compPID, "uAlpha");
  m_compBFieldLoc    = glGetUniformLocation(m_compPID, "uBField");
  m_compEpsilonLoc   = glGetUniformLocation(m_compPID, "uEpsilon");
  m_compTimeLoc      = glGetUniformLocation(m_compPID, "uTime");

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

  // cache render uniform locations
  m_rendFieldLoc       = glGetUniformLocation(m_rendPID, "uField");
  m_rendMaskLoc        = glGetUniformLocation(m_rendPID, "uMask");
  m_rendRenderModeLoc  = glGetUniformLocation(m_rendPID, "uRenderMode");
  m_rendHLoc           = glGetUniformLocation(m_rendPID, "uH");
  m_rendAlphaLoc       = glGetUniformLocation(m_rendPID, "uAlpha");
  m_rendBetaLoc        = glGetUniformLocation(m_rendPID, "uBeta");
  m_rendLLoc           = glGetUniformLocation(m_rendPID, "uL");
  m_rendBFieldLoc      = glGetUniformLocation(m_rendPID, "uBField");
  m_rendQLoc           = glGetUniformLocation(m_rendPID, "uQ");
  m_rendEpsilonLoc     = glGetUniformLocation(m_rendPID, "uEpsilon");

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

  m_mesh->mask = geometry::genMask(m_device.layers[0], m_mesh->resX, m_device.worldSize);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mesh->resX, m_mesh->resY, GL_RED, GL_FLOAT, m_mesh->mask.data());
}

void simulator::buildNeighborTexture() {
  // Encode neighbor indices into RGBA texture (up, down, left, right)
  std::vector<glm::vec4> neighborData(m_mesh->resX * m_mesh->resY);
  
  for (int y = 0; y < m_mesh->resY; y++) {
    for (int x = 0; x < m_mesh->resX; x++) {
      int idx = y * m_mesh->resX + x;
      const meshCell& cell = m_mesh->cells[idx];
      
      // Store neighbor IDs normalized to [0, 1] for texture encoding
      float scale = 1.0f / (float)(m_mesh->resX * m_mesh->resY);
      
      glm::vec4 neighbors(0.0f);
      if (cell.neighborIds.size() >= 4) {
        neighbors.r = (float)cell.neighborIds[0] * scale;  // up
        neighbors.g = (float)cell.neighborIds[1] * scale;  // down
        neighbors.b = (float)cell.neighborIds[2] * scale;  // left
        neighbors.a = (float)cell.neighborIds[3] * scale;  // right
      }
      
      neighborData[idx] = neighbors;
    }
  }
  
  // Create and upload neighbor texture
  glGenTextures(1, &m_neighborTexture);
  glBindTexture(GL_TEXTURE_2D, m_neighborTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_mesh->resX, m_mesh->resY, 0, GL_RGBA, GL_FLOAT, neighborData.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void simulator::buildEdgeWeightTexture() {
  // Upload pre-computed edge weights from mesh to GPU texture
  // Weights are normalized by reference distance for discretization consistency
  
  glGenTextures(1, &m_edgeWeightTexture);
  glBindTexture(GL_TEXTURE_2D, m_edgeWeightTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_mesh->resX, m_mesh->resY, 0, GL_RGBA, GL_FLOAT, m_mesh->edgeWeights.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void simulator::step() {
  if (m_device.layers.empty()) return;

  glUseProgram(m_compPID);

  // Bind textures to image units
  glBindImageTexture(0, m_phiTextures[m_readIdx],   0, GL_FALSE, 0, GL_READ_ONLY,   GL_RGBA32F);
  glBindImageTexture(1, m_phiTextures[m_writeIdx],  0, GL_FALSE, 0, GL_WRITE_ONLY,  GL_RGBA32F);
  glBindImageTexture(2, m_maskTexture,              0, GL_FALSE, 0, GL_READ_ONLY,   GL_R32F);
  
  // Bind texture sampler and neighbor connectivity
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glUniform1i(m_compImgSamplerLoc, 0);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, m_neighborTexture);
  glUniform1i(glGetUniformLocation(m_compPID, "img_neighbors"), 3);

  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, m_edgeWeightTexture);
  glUniform1i(glGetUniformLocation(m_compPID, "img_edgeWeights"), 4);

  float alpha = 1.0f / (4.0f * mc * m_layer.xi * m_layer.xi);
  glUniform1f(m_compXiLoc,       m_layer.xi);
  glUniform1f(m_compLambdaLoc,   m_layer.lambda);
  glUniform1f(m_compGammaLoc,    m_layer.gamma);
  glUniform1f(m_compQLoc,        q);
  glUniform1f(m_compHLoc,        h);
  glUniform1f(m_compLLoc,        m_device.worldSize.x);
  glUniform1f(m_compRelaxLoc,    m_layer.u);
  glUniform1i(m_compUseNoiseLoc, useNoise);
  glUniform1f(m_compDtLoc,       dt);
  glUniform1f(m_compAlphaLoc,    alpha);
  glUniform1f(m_compBFieldLoc,   m_device.externalB.z);
  glUniform1f(m_compEpsilonLoc,  m_layer.epsilon);
  glUniform1f(m_compTimeLoc,     simTime);

  int dispatchJobs = (m_mesh->resX + m_computeGroupSize - 1) / m_computeGroupSize;
  glDispatchCompute(dispatchJobs, dispatchJobs, 1);

  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
  glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
  glBindImageTexture(2, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

  // Swap buffers and advance time
  std::swap(m_readIdx, m_writeIdx);
  simTime += dt;

  // Update adaptive time step only every few solver iterations to avoid CPU/GPU stalls
  m_dtUpdateCounter += 1;
  if (m_dtUpdateCounter >= m_dtUpdateInterval) {
    m_dtUpdateCounter = 0;

    glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, m_phiBuffer.data());

    int count = m_mesh->resX * m_mesh->resY;
    float maxDelta = 0.0f;
    for (int i = 0; i < count; ++i) {
      float x = m_phiBuffer[i * 4 + 0];
      float y = m_phiBuffer[i * 4 + 1];
      float phi2 = x * x + y * y;
      float dphi = std::abs(phi2 - prevPhi2[i]);
      maxDelta = std::max(maxDelta, dphi);
      prevPhi2[i] = phi2;
    }

    if (maxChange.size() >= static_cast<size_t>(stepsPerFrame)) {
      maxChange.erase(maxChange.begin());
    }
    maxChange.push_back(maxDelta);
    findTimeStep();
  }

  // Update vortex count periodically, but avoid doing this every step.
  vortexCountCounter += 1;
  if (vortexCountCounter >= 100) {
    vortexCount = countVortices();
    vortexCountCounter = 0;
  }
}


void simulator::findTimeStep() {
  if (maxChange.empty()) {
    return;
  }

  float deltaN = 0.0f;
  for (float value : maxChange) {
    deltaN += value;
  }

  if (deltaN <= 0.0f) {
    return;
  }

  float candidate = 0.5f * (dt + ((initDt * maxChange.size()) / deltaN));
  dt = std::max(dt_min, std::min(candidate, dt_max));
}

void simulator::quench() {
  // Use a small-amplitude random seed, not sustained thermal noise, so the system
  // can transition to its natural vortex state under the applied field.
  useNoise = false;
  std::vector<float> initData(m_mesh->resX * m_mesh->resY * 4, 0.0f);

  float phiEq   = sqrt(m_layer.epsilon);
  float phiAmp  = 0.1f * phiEq; // start near zero with small random perturbations
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

  for(int i = 0; i < (m_mesh->resX * m_mesh->resY); i++) {
      initData[i * 4 + 0] = phiAmp * dist(gen);
      initData[i * 4 + 1] = phiAmp * dist(gen);
      initData[i * 4 + 2] = 0.0f;
      initData[i * 4 + 3] = 0.5f;
  }

  for(int i = 0; i < 2;i++){
      glBindTexture(GL_TEXTURE_2D, m_phiTextures[i]);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mesh->resX, m_mesh->resY, GL_RGBA, GL_FLOAT, initData.data());
  }
  std::cout << "!! SYSTEM QUENCHED !!" << std::endl;
  simTime = 0;
}

void simulator::quenchSeededLattice() {
  // Seed a triangular Abrikosov lattice with proper vortex cores
  // Each vortex core has 2π phase winding and amplitude healing profile
  useNoise = false;
  std::vector<float> initData(m_mesh->resX * m_mesh->resY * 4, 0.0f);

  float phiEq = sqrt(m_layer.epsilon);
  float xi = m_layer.xi;  // coherence length

  // Calculate Abrikosov lattice spacing: a ≈ sqrt(2*Phi_0 / (sqrt(3)*B))
  float B = m_device.externalB.z;
  float latticeDist = (B > 1e-6f) ? sqrt(4.0f * 3.14159f / (1.732f * B)) : 12.0f;

  // Convert to grid spacing units for indexing
  float latticeSpacing = latticeDist / h;
  float coreHealing = xi / h;  // core healing length in grid points

  std::cout << "Seeding TRUE Abrikosov lattice with spacing " << latticeDist << " ξ ("
            << latticeSpacing << " grid pts), core healing " << coreHealing << " pts" << std::endl;

  // Triangular lattice basis vectors
  float a1x = latticeSpacing;
  float a1y = 0.0f;
  float a2x = 0.5f * latticeSpacing;
  float a2y = 0.866f * latticeSpacing;

  // Build list of lattice site positions (vortex core centers)
  std::vector<glm::vec2> latticeSites;
  for (int n1 = -4; n1 <= 4; n1++) {
    for (int n2 = -4; n2 <= 4; n2++) {
      float lx = n1 * a1x + n2 * a2x;
      float ly = n1 * a1y + n2 * a2y;
      // Keep sites within or near domain (with periodic wrapping)
      latticeSites.push_back(glm::vec2(lx, ly));
    }
  }

  // For each grid cell, find nearest vortex cores and compute order parameter
  for (int yi = 0; yi < m_mesh->resY; yi++) {
    for (int xi = 0; xi < m_mesh->resX; xi++) {
      float x = float(xi);
      float y = float(yi);

      // Find the closest vortex core (use periodic boundaries)
      float minDist = 1e9f;
      float vortexX = 0.0f, vortexY = 0.0f;

      for (const auto& site : latticeSites) {
        float dx = x - site.x;
        float dy = y - site.y;
        // Handle periodic boundaries
        while (dx > m_mesh->resX * 0.5f) dx -= m_mesh->resX;
        while (dx < -m_mesh->resX * 0.5f) dx += m_mesh->resX;
        while (dy > m_mesh->resY * 0.5f) dy -= m_mesh->resY;
        while (dy < -m_mesh->resY * 0.5f) dy += m_mesh->resY;

        float dist = sqrt(dx*dx + dy*dy);
        if (dist < minDist) {
          minDist = dist;
          vortexX = site.x;
          vortexY = site.y;
        }
      }

      // Vortex core phase: 2π winding around nearest core
      float dx = x - vortexX;
      float dy = y - vortexY;
      // Handle periodic boundaries
      while (dx > m_mesh->resX * 0.5f) dx -= m_mesh->resX;
      while (dx < -m_mesh->resX * 0.5f) dx += m_mesh->resX;
      while (dy > m_mesh->resY * 0.5f) dy -= m_mesh->resY;
      while (dy < -m_mesh->resY * 0.5f) dy += m_mesh->resY;

      float vortexPhase = atan2(dy, dx);

      // Healing profile: amplitude grows from 0 at core to phiEq at far field
      // Use form: |φ| = φ_eq * sqrt(1 - exp(-r²/ξ²))
      // This gives correct r → 0 scaling (linear) and r → ∞ saturation
      float rNorm = minDist / coreHealing;
      float healingFactor = sqrt(1.0f - exp(-rNorm * rNorm));
      float mag = phiEq * healingFactor;

      // Store complex order parameter as (Re, Im) with proper vortex core structure
      initData[(yi * m_mesh->resX + xi) * 4 + 0] = mag * cos(vortexPhase);
      initData[(yi * m_mesh->resX + xi) * 4 + 1] = mag * sin(vortexPhase);
      initData[(yi * m_mesh->resX + xi) * 4 + 2] = 0.0f;
      initData[(yi * m_mesh->resX + xi) * 4 + 3] = 0.5f;
    }
  }

  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, m_phiTextures[i]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mesh->resX, m_mesh->resY, GL_RGBA, GL_FLOAT, initData.data());
  }

  std::cout << "!! TRUE ABRIKOSOV LATTICE SEEDED !!" << std::endl;
  simTime = 0;
}

void simulator::updatePhiStats() {
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, m_phiBuffer.data());

  float minVal = 1e9f;
  float maxVal = 0.0f;
  double sumVal = 0.0;
  int count = m_mesh->resX * m_mesh->resY;

  for (int i = 0; i < count; ++i) {
    float x = m_phiBuffer[i * 4 + 0];
    float y = m_phiBuffer[i * 4 + 1];
    float mag = sqrt(x * x + y * y);
    sumVal += mag;
    minVal = std::min(minVal, mag);
    maxVal = std::max(maxVal, mag);
  }

  phiAvg = float(sumVal / double(count));
  phiMin = minVal;
  phiMax = maxVal;
}

int simulator::countVortices() {
  // Count vortex singularities by detecting 2π phase windings around plaquettes
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, m_phiBuffer.data());

  int vortexCount = 0;
  
  // Check each plaquette for phase circulation
  for (int y = 0; y < m_mesh->resY - 1; y++) {
    for (int x = 0; x < m_mesh->resX - 1; x++) {
      // Get phases at four corners (with periodic wrap)
      auto getPhase = [&](int px, int py) {
        px = (px + m_mesh->resX) % m_mesh->resX;
        py = (py + m_mesh->resY) % m_mesh->resY;
        float psi_x = m_phiBuffer[(py * m_mesh->resX + px) * 4 + 0];
        float psi_y = m_phiBuffer[(py * m_mesh->resX + px) * 4 + 1];
        return atan2(psi_y, psi_x);
      };
      
      float p0 = getPhase(x, y);
      float p1 = getPhase(x + 1, y);
      float p2 = getPhase(x + 1, y + 1);
      float p3 = getPhase(x, y + 1);
      
      // Compute circulation around plaquette
      // Account for 2π wraps
      auto angleDiff = [](float a, float b) {
        float diff = a - b;
        while (diff > PI) diff -= 2.0f * PI;
        while (diff < -PI) diff += 2.0f * PI;
        return diff;
      };
      
      float circulation = angleDiff(p0, p1) + angleDiff(p1, p2) + 
                        angleDiff(p2, p3) + angleDiff(p3, p0);
      
      // Normalize to vortex count (circulation/2π)
      if (abs(circulation) > PI) {
        vortexCount++;
      }
    }
  }
  
  return vortexCount;
}

void simulator::render(int renderMode) {
  glUseProgram(m_rendPID);

  // bind current read texture and mask
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_phiTextures[m_readIdx]);
  glUniform1i(m_rendFieldLoc, 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_maskTexture);
  glUniform1i(m_rendMaskLoc, 1);

  float h    = m_device.worldSize.x / (float)m_mesh->resX;
  float alpha = 1.0f / (4.0f * mc * m_layer.xi * m_layer.xi);
  float beta  = (q * q * m_layer.lambda * m_layer.lambda * alpha) / mc;

  glUniform1i(m_rendRenderModeLoc, renderMode);
  glUniform1f(m_rendHLoc,          h);
  glUniform1f(m_rendAlphaLoc,      alpha);
  glUniform1f(m_rendBetaLoc,       beta);
  glUniform1f(m_rendLLoc,          m_device.worldSize.x);
  glUniform1f(m_rendBFieldLoc,     m_device.externalB.z);
  glUniform1f(m_rendQLoc,          q);
  glUniform1f(m_rendEpsilonLoc,    m_layer.epsilon);

  glBindVertexArray(m_quadVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
