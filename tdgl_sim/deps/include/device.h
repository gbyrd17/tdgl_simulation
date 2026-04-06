#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct polygon {
  std::string name;
  std::vector<glm::vec2> vertices;

  bool isMesh; 
  float meshDensity;

  bool contains(glm::vec2 p);
};

struct layer {
  std::string name;

  double lambda;          // penetration depth
  double xi;              // coherence length
  double thickness;       // thickness of layer
  double sigma_n;         // normal state conductivity
  double gamma;           // tdgl relaxation rate
  float  z;               // vertical position in stack

  std::vector<polygon> polygons;

  // gpu texture resources
  unsigned int phiTex   = 0;    // order param field
  unsigned int jTex     = 0;    // supercurrent field
                                //
  
};

struct device {
  std::vector<layer> layers;
  glm::vec3 externalB = glm::vec3(0,0,0);
  float temp          = 0.1f;

  glm::vec2 worldSize;
};

