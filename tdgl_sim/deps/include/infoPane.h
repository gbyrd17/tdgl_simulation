#pragma once
#include <device.h>
#include <simulator.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

class infoPane {
  public:
    infoPane(device& device, simulator& simulator, layer& layer);
    virtual ~infoPane();

    void drawSidebar(int simW, int sideW, int h_win);
    void drawLegend();
    void drawSimVars();

  private:
  
  device&     m_device;
  simulator&  m_simulator;
  layer&      m_layer;
  float gridUnit;
  float gridUnitX;
  float gridUnitY;

};
