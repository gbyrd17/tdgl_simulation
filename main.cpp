#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <device.h>
#include <geometry.h>
#include <simulator.h>
#include <infoPane.h>
#include <vector>
#include <iostream>

int main()  {
  device dev;     // init device 
  dev.worldSize = glm::vec2(96.0f, 96.0f);  // multiple of Abrikosov spacing for clean lattice
  dev.externalB = glm::vec3(0, 0, 0.15f);  // moderate field for visible vortex nucleation in a 100ξ domain
  dev.surfaceVoltage = 0.5f;  // electric potential to drive lattice formation via condensate dynamics
  dev.sheetCurrentDensity = 0.05f;  // injected surface current density to nucleate vortex motion

  layer nbLayer;  // init niobium layer
  nbLayer.xi      = 1.0f;
  nbLayer.lambda  = 2.0f;
  nbLayer.gamma   = 0.1f;
  nbLayer.epsilon = 1.0f; // stronger superconducting condensate

  polygon rect = geometry::genRectangle({48, 48}, {80, 80});
  rect.isMesh = true;
  nbLayer.polygons.push_back(rect);

  // polygon bodyNb; // define geometry of the niobium layer
  // bodyNb.isMesh = true;

  // polygon hole;   // define a hole to put in the niobium layer
  // hole.isMesh = false;

  // nbLayer.polygons.push_back(bodyNb);
  // nbLayer.polygons.push_back(hole);

  dev.layers.push_back(nbLayer); // write layer and its geometry into device

  // init window
  std::cout << "starting glfw ~" << std::endl;
  if (!glfwInit()) {
    std::cerr << "ERROR::GLFW_INITIALIZATION" << std::endl;
    return -1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // compute shaders need 4.3+
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(1536, 768, "GPU TDGL Solver", NULL, NULL);
  if (!window) {
    std::cerr << "ERROR::GLFW_WINDOW_INITIALIZATION" << std::endl;
    return -1;
  }

  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "ERROR::GLAD_PROCESS_INITIALIZATION" << std::endl;
    return -1;
  }
  std::cout << "OpenGL Context: " << glGetString(GL_VERSION) << std::endl;

  // glfw window must be instantiated before any shader math is performed
  simulator sim(dev, nbLayer, 1024, 1024, 300);

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

    // make sure we havent hijacked our own texture pointer

    for (int i = 0; i < sim.stepsPerFrame; i++) {sim.step();} 

    // render step
    int w, h_win; glfwGetFramebufferSize(window, &w, &h_win);
    // sidebar occupies right 1/6; two sim panels share the left 5/6
    int simW  = w * 5 / 6;
    int sideW = w - simW;

    // Render first viewport always
    glViewport(0, 0, simW / 2, h_win);
    sim.render(0);

    // Optionally render second viewport (can be disabled for speed)
    if (sim.m_renderBothViewports) {
      glViewport(simW / 2, 0, simW / 2, h_win);
      sim.render(1);
    }

    // ── SIDEBAR ──────────────────────────────────────────────────────────
    infoPane sidebar(dev, sim, nbLayer);
    sidebar.drawSidebar(simW, sideW, h_win);
    sidebar.drawLegend();
    sidebar.drawSimVars();
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

