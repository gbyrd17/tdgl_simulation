#include <simulator.h>
#include <device.h>
#include <infoPane.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// constructor 
infoPane::infoPane(device& device, simulator& simulator, layer& layer ): 
  m_device(device), m_simulator(simulator), m_layer(layer) {
    gridUnitX = (m_device.worldSize.x / m_simulator.m_mesh->resX) / m_layer.xi;
    gridUnitY = (m_device.worldSize.y / m_simulator.m_mesh->resY) / m_layer.xi;
  }

// destructor
infoPane::~infoPane() {
  
}

void infoPane::drawSidebar(int simW, int sideW, int h_win) {
  ImGui::SetNextWindowPos(ImVec2((float)simW, 0));
  ImGui::SetNextWindowSize(ImVec2((float)sideW, (float)h_win));
  ImGui::Begin("##sidebar", nullptr,
    ImGuiWindowFlags_NoDecoration        |
    ImGuiWindowFlags_NoMove              |
    ImGuiWindowFlags_NoResize            |
    ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGui::TextColored(ImVec4(0.6f,0.8f,1.f,1.f), "LEFT  : Phase map");
  ImGui::TextColored(ImVec4(0.6f,0.8f,1.f,1.f), "RIGHT : Density + Supercurrent");
  ImGui::Separator();
}

void infoPane::drawLegend() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wPos = ImGui::GetCursorScreenPos();
    float cx = wPos.x + 42.f;
    float cy = wPos.y + 42.f;
    float rad = 34.f;

    for (int i = 0; i < 36; i++) {
      float a1 = (i       / 36.f) * 2.f * 3.14159f;
      float a2 = ((i + 1) / 36.f) * 2.f * 3.14159f;
      float r, g, b;
      ImGui::ColorConvertHSVtoRGB(i / 36.f, 0.8f, 0.8f, r, g, b);
      dl->PathLineTo(ImVec2(cx, cy));
      dl->PathArcTo(ImVec2(cx, cy), rad, a1, a2);
      dl->PathFillConvex(ImGui::GetColorU32(ImVec4(r, g, b, 1.f)));
    }

    ImU32 white = ImGui::GetColorU32(ImVec4(1,1,1,0.9f));
    dl->AddText(ImVec2(cx - 12.f, cy - rad - 14.f), white, "Phase");
    dl->AddText(ImVec2(cx + rad + 4.f, cy - 7.f),   white, "0");
    dl->AddText(ImVec2(cx - 5.f, cy + rad + 4.f),   white, "pi/2");

    ImGui::Dummy(ImVec2(0.f, rad * 2.f + 20.f));
    ImGui::Separator();
}

void infoPane::drawSimVars() {
    ImGui::TextColored(ImVec4(0.f,1.f,1.f,1.f), "SYSTEM SCALE");
    ImGui::Text("Box size    %.2f x %.2f m",  m_device.worldSize.x/m_layer.xi, m_device.worldSize.y/m_layer.xi);
    ImGui::Text("Grid step   %.3e m",         m_simulator.h);
    ImGui::Text("xi          %.4e m",         m_layer.xi);
    ImGui::Separator();

    // magnetics
    ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "MAGNETICS");
    ImGui::Text("B field     %.3e", m_device.externalB.z);
    ImGui::Text("N expected  %.1e", (m_simulator.mc * m_device.externalB.z * m_device.worldSize.x * m_device.worldSize.x) / (2.f * 3.14159f));
    ImGui::Separator();

    // material
    float normKappa = m_layer.lambda / (sqrt(2) * m_layer.xi);
    ImGui::TextColored(ImVec4(1.f,0.5f,0.f,1.f), "MATERIAL");
    ImGui::Text("kappa       %.4e", normKappa);
    ImGui::Text("xi^2        %.4e", m_layer.xi * m_layer.xi);
    ImGui::Text("lambda^2    %.4e", m_layer.lambda * m_layer.lambda);
    if      (normKappa < 1.f) ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f), "Type-I");
    else if (normKappa > 1.f) ImGui::TextColored(ImVec4(1.f,0.6f,0.2f,1.f), "Type-II");
    else                      ImGui::TextColored(ImVec4(1.f,1.f,1.f,1.f),   "Bogomolny point");
    ImGui::Separator();

    // controls
    ImGui::TextColored(ImVec4(0.8f,1.f,0.8f,1.f), "CONTROLS");
    ImGui::SliderInt("Steps/Frame", &m_simulator.stepsPerFrame, 10, 500);
    
    if (ImGui::SliderInt("Partition Size##gpu", &m_simulator.m_partitionSize, 32, 1024)) {
      m_simulator.setPartitionSize(m_simulator.m_partitionSize);
    }

    bool noiseCheck = m_simulator.useNoise;
    if (ImGui::Checkbox("Thermal noise", &noiseCheck)) m_simulator.useNoise = noiseCheck;
    
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.f,0.8f,0.2f,1.f), "PERFORMANCE");
    ImGui::Checkbox("Render Both Viewports", &m_simulator.m_renderBothViewports);
    ImGui::Checkbox("Enable Vortex Count", &m_simulator.m_enableVortexCounting);
    ImGui::SliderInt("Vortex Count Freq", &m_simulator.m_vortexCountInterval, 100, 5000);
    ImGui::SliderInt("Timestep Update Freq", &m_simulator.m_dtUpdateInterval, 10, 500);
    ImGui::Separator();

    if (ImGui::Button("Random Quench", ImVec2(-1, 0))) m_simulator.quench();
    if (ImGui::Button("Seed Abrikosov Lattice", ImVec2(-1, 0))) m_simulator.quenchSeededLattice();
    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.8f,1.f,0.8f,1.f), "GEOMETRY");
    static glm::vec2 geomCenter(48.0f, 48.0f);
    static float geomSize = 30.0f;
    ImGui::InputFloat2("Center##geom", &geomCenter.x);
    ImGui::SliderFloat("Size", &geomSize, 5.0f, 80.0f);
    
    if (ImGui::Button("Apply Rectangle", ImVec2(-1, 0))) {
      m_simulator.applyGeometryRectangle(geomCenter, glm::vec2(geomSize, geomSize));
    }
    if (ImGui::Button("Apply Circle", ImVec2(-1, 0))) {
      m_simulator.applyGeometryCircle(geomCenter, geomSize);
    }
    if (ImGui::Button("Apply Triangle", ImVec2(-1, 0))) {
      m_simulator.applyGeometryTriangle(geomCenter, geomSize);
    }
    if (ImGui::Button("Clear Geometry", ImVec2(-1, 0))) {
      m_simulator.clearGeometry();
    }
    ImGui::Separator();

    ImGui::Text("%.1f FPS  (%.2f ms)", ImGui::GetIO().Framerate,
                1000.f / ImGui::GetIO().Framerate);
    ImGui::Text("m_simulator time  %.4e", m_simulator.simTime);
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f,0.8f,1.f,1.f), "PHI MAGNITUDE");
    ImGui::Text("avg |psi|   %.4e", m_simulator.phiAvg);
    ImGui::Text("min |psi|   %.4e", m_simulator.phiMin);
    ImGui::Text("max |psi|   %.4e", m_simulator.phiMax);
    ImGui::Separator();
    
    ImGui::TextColored(ImVec4(1.f,0.8f,0.2f,1.f), "VORTEX STATISTICS");
    ImGui::Text("Vortex count   %d", m_simulator.vortexCount);
    float expectedVortices = (m_simulator.mc * m_device.externalB.z * m_device.worldSize.x * m_device.worldSize.x) / (2.f * 3.14159f);
    ImGui::Text("Expected       %.0f", expectedVortices);
    ImGui::Text("Occupancy      %.1f%%", 100.f * m_simulator.vortexCount / std::max(1.f, expectedVortices));
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.f,0.8f,0.8f,1.f), "ADAPTIVE TIMESTEP");
    ImGui::Text("dt          %.4e", m_simulator.dt);

    ImGui::End(); 
}
