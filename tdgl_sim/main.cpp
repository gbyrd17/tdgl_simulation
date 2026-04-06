#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <device.h>
#include <geometry.h>
#include <simulator.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>

bool useNoise = false;

// ----------------------- Shader helpers -----------------------
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    if(!file) { std::cerr << "Failed to open " << path << "\n"; return ""; }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

GLuint compileShader(GLenum type, const std::string &src){
    GLuint s = glCreateShader(type);
    const char* c_src = src.c_str();
    glShaderSource(s,1,&c_src,nullptr);
    glCompileShader(s);
    int success; char info[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if(!success){ glGetShaderInfoLog(s,512,nullptr,info); std::cerr << "Error: "<< info << "\n"; }
    return s;
}

int main()  {
  device dev;     // init device 
  dev.worldSize = glm::vec2(80.0, 80.0);

  layer nbLayer;  // init niobium layer
  nbLayer.xi = 1.0f;
  nbLayer.gamma = 1.0f;

  nbLayer.polygons.push_back(geometry::genRectangle({40, 40}, {60, 10}));

  polygon bodyNb; // define geometry of the niobium layer
  bodyNb.isMesh = true;

  polygon hole;   // define a hole to put in the niobium layer
  hole.isMesh = false;

  nbLayer.polygons.push_back(bodyNb);
  nbLayer.polygons.push_back(hole);

  dev.layers.push_back(nbLayer); // write layer and its geometry into device

  simulator sim(dev, nbLayer, 1024);

  // init window
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // compute shaders need 4.3+
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(1536, 768, "GPU TDGL Solver", NULL, NULL);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  // init imgui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
  ImGui::StyleColorsDark();

  int readTex = 0;
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);
    glfwPollEvents();

    // handle input
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    dev.externalB.z = 0.05f;

    for (int i = 0; i < sim.stepsPerFrame; i++) {sim.step();} 

    // render step
    int w, h_win; glfwGetFramebufferSize(window, &w, &h_win);
    // sidebar occupies right 1/3; two sim panels share the left 2/3
    int simW  = w * 5 / 6;
    int sideW = w - simW;

    glViewport(0, 0, simW / 2, h_win);
    sim.render(0);

    glViewport(simW / 2, 0, simW / 2, h_win);
    sim.render(1);

    // ── SIDEBAR ──────────────────────────────────────────────────────────
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

    // phase colour wheel
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wpos    = ImGui::GetCursorScreenPos();
    float  cx      = wpos.x + 42.f;
    float  cy      = wpos.y + 42.f;
    float  rad     = 34.f;
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

    // system scale
    float gridUnit = (dev.worldSize.x / sim.m_res)/nbLayer.xi;
    ImGui::TextColored(ImVec4(0.f,1.f,1.f,1.f), "SYSTEM SCALE");
    ImGui::Text("Box size    %.2f x %.2f m",  dev.worldSize.x/nbLayer.xi, dev.worldSize.x/nbLayer.xi);
    ImGui::Text("Grid step   %.3e m",         gridUnit);
    ImGui::Text("xi          %.4e m",         nbLayer.xi);
    ImGui::Separator();

    // magnetics
    ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "MAGNETICS");
    ImGui::Text("B field     %.3e", dev.externalB.z);
    ImGui::Text("N expected  %.1e", (sim.mc * dev.externalB.z * dev.worldSize.x * dev.worldSize.x) / (2.f * 3.14159f));
    ImGui::Separator();

    // material
    float normKappa = nbLayer.lambda / (sqrt(2) * nbLayer.xi);
    ImGui::TextColored(ImVec4(1.f,0.5f,0.f,1.f), "MATERIAL");
    ImGui::Text("kappa       %.4e", normKappa);
    ImGui::Text("xi^2        %.4e", nbLayer.xi * nbLayer.xi);
    ImGui::Text("lambda^2    %.4e", nbLayer.lambda * nbLayer.lambda);
    if      (normKappa < 1.f) ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f), "Type-I");
    else if (normKappa > 1.f) ImGui::TextColored(ImVec4(1.f,0.6f,0.2f,1.f), "Type-II");
    else                      ImGui::TextColored(ImVec4(1.f,1.f,1.f,1.f),   "Bogomolny point");
    ImGui::Separator();

    // controls
    ImGui::TextColored(ImVec4(0.8f,1.f,0.8f,1.f), "CONTROLS");
    ImGui::SliderInt("Steps/Frame", &sim.stepsPerFrame, 10, 500);

    bool noiseCheck = useNoise;
    if (ImGui::Checkbox("Thermal noise", &noiseCheck)) useNoise = noiseCheck;

    if (ImGui::Button("Random Quench", ImVec2(-1, 0))) sim.quench();
    ImGui::Separator();

    ImGui::Text("%.1f FPS  (%.2f ms)", ImGui::GetIO().Framerate,
                1000.f / ImGui::GetIO().Framerate);
    ImGui::Text("Sim time  %.4e", sim.simTime);

    ImGui::End();
    // ── END SIDEBAR ───────────────────────────────────────────────────────

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

