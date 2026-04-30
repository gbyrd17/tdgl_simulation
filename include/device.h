#pragma once
#include "electrode.h"
#include "magneticProfile.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

/*'polygon' struct:
 *:---------------:
 *
 */
struct Polygon {
  std::string name;
  std::vector<glm::vec2> vertices;

  bool isMesh;
  float meshDensity;

  bool contains(glm::vec2 p);
};

struct Layer {
  std::string name;

  double lambda;    // penetration depth
  double xi;        // coherence length
  double thickness; // thickness of layer
  double sigma_n;   // normal state conductivity
  double gamma;     // tdgl relaxation rate
  float u =
      5.79f; // ratio of relaxation times for the amplitude and phase of phi
  float z;   // vertical position in stack
  float epsilon = 0.5f; // GL free energy parameter: (Tc - T) / Tc; controls
                        // equilibrium |psi| magnitude

  std::vector<Polygon> polygons;
};

struct device {
  std::unique_ptr<MagneticProfile> fieldProfile; // new mag field control system
  std::unique_ptr<ElectrodeRegistry> electrodes;

  std::vector<Layer> layers;
  float temp = 0.1f;

  glm::vec3 externalB =
      glm::vec3(0, 0, 0);      // DEPRECATED: legacy support- will be phased out
  float surfaceVoltage = 0.0f; // electric potential to drive lattice formation
  float sheetCurrentDensity = 0.0f; // supercurrent density injected at surface

  device() : fieldProfile(std::make_unique<MagneticProfile>()) {}

  glm::vec2 worldSize;
};
