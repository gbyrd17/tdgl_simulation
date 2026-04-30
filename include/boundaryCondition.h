#include "electrode.h"
#include "mesh.h"
class BoundaryConditionSolver {
  const ElectrodeRegistry &electrodes;

  void solveVoltageField(Mesh &m);
  void solveCurrent(Mesh &m);
  void uploadElectrodeParams(const Mesh &m);
};
