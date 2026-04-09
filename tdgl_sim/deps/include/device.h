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
  float  u = 5.79f;       // ratio of relaxation times for the amplitude and phase of phi
  float  z;               // vertical position in stack
  float  epsilon = 0.5f;  // GL free energy parameter: (Tc - T) / Tc; controls equilibrium |psi| magnitude

  std::vector<polygon> polygons;
};

struct device {
  std::vector<layer> layers;
  glm::vec3 externalB = glm::vec3(0,0,0);
  float temp          = 0.1f;

  glm::vec2 worldSize;
};

