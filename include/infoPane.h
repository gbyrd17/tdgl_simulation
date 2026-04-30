#pragma once
#include <../imgui/imgui.h>
#include <device.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <simulator.h>

class infoPane {
public:
  infoPane(device &device, Simulator &simulator, layer &layer);
  virtual ~infoPane();

  void drawSidebar(int simW, int sideW, int h_win);
  void drawLegend();
  void drawSimVars();

private:
  device &m_device;
  Simulator &m_simulator;
  layer &m_layer;
  float gridUnit;
  float gridUnitX;
  float gridUnitY;
};
