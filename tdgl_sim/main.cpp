#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>

bool useNoise = false;

// Physical parameters (use these to scale everything)
float mc     = 1.0f;   // Cooper pair mass
float q      = 1.0f;   // Cooper pair charge
float alpha  = -1.0f;  // GL coefficient
float beta   = 1.0f;   // GL coefficient
float gamma_ = 1.0f;   // damping

float alpha_min = -0.1f, alpha_max = 2.0f; 
float beta_min  =  0.1f, beta_max  =  2.0f;

float xi_2()  { return 1.0f / (4.0f * mc * std::abs(alpha)); }
float lam_2() { return (mc * beta) / (q*q*std::abs(alpha)); }
float normKappa() { return sqrt(lam_2() / (2 * xi_2())); }

// --- Simulation grid ---
const int dim = 512;
float h = xi_2()/5;
float L = dim*h;
int stepsPerFrame = 100;




// --- Vortices ---
int N_vortices = 16;
float bField = (2.0f*3.14159265f*N_vortices)/(q*L*L);

// --- Time step ---
float dt_cfl() { return (0.01f*h*h)/(4.0f*xi_2()*gamma_); }
float simTime = 0;

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

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // compute shaders need 4.3+
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(1536, 768, "GPU TDGL Solver", NULL, NULL);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  // create ping-pong textures (A and B)
  GLuint tex[2];
  glGenTextures(2, tex);
  std::vector<float> initData(dim * dim * 4, 0.0f);

  // initial quench => random phase
  auto quench = [&]() {
    useNoise = true;
    std::mt19937 rng(47);
    std::uniform_real_distribution<float> dist(0,2.0f*3.14159265f);
    float eq = sqrt(std::abs(alpha)/(2.0f*beta));

    for(int i=0;i<dim*dim;i++){
        float theta = dist(rng);
        float r     = eq * (0.8f + 0.2f*(dist(rng)/(2.0f*3.14159265f)));
        initData[i*4 + 0] = r * cos(theta);
        initData[i*4 + 1] = r * sin(theta);
        initData[i*4 + 2] = 0.0f;
        initData[i*4 + 3] = 0.0f;
    }

    for(int i=0;i<2;i++){
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,dim,dim,GL_RGBA,GL_FLOAT,initData.data());
    }
    useNoise = false; // only in initial quench
  };

  for (int i = 0; i < 2; ++i) {
    glBindTexture(GL_TEXTURE_2D, tex[i]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, dim, dim);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  // initial quench to start simulation
  quench();

  // compute shader (TDGL calc)
  std::string compSrc = loadShaderSource("../shaders/comp.glsl");
  GLuint compShader = compileShader(GL_COMPUTE_SHADER, compSrc);
  GLuint compProg = glCreateProgram();
  glAttachShader(compProg, compShader);
  glLinkProgram(compProg);

  // render shader (viz)
  std::string vertSrc = loadShaderSource("../shaders/vert.glsl");
  std::string fragSrc = loadShaderSource("../shaders/frag.glsl");
  GLuint renderProg = glCreateProgram();
  glAttachShader(renderProg, compileShader(GL_VERTEX_SHADER, vertSrc));
  glAttachShader(renderProg, compileShader(GL_FRAGMENT_SHADER, fragSrc));
  glLinkProgram(renderProg);

  float verts[] = { -1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1 };
  GLuint VAO, VBO;
  glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
  glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);
  glEnableVertexAttribArray(0);

  // init imgui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
  ImGui::StyleColorsDark();

  int readTex = 0;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // handle input
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // GPU compute
    glUseProgram(compProg);
    glUniform1f(glGetUniformLocation(compProg, "uAlpha"), alpha);
    glUniform1f(glGetUniformLocation(compProg, "uBeta"), beta);
    glUniform1f(glGetUniformLocation(compProg, "uGamma"), gamma_);
    glUniform1f(glGetUniformLocation(compProg, "uMC"), mc);
    glUniform1f(glGetUniformLocation(compProg, "uQ"), q);
    glUniform1f(glGetUniformLocation(compProg, "uH"), h);
    glUniform1f(glGetUniformLocation(compProg, "uL"), L);
    glUniform1i(glGetUniformLocation(compProg, "uUseNoise"), useNoise ? 1 : 0);
    glUniform1f(glGetUniformLocation(compProg, "uDt"), dt_cfl());
    glUniform1f(glGetUniformLocation(compProg, "uBField"), bField);

    // run multiple steps per frame
    for (int i = 0; i < stepsPerFrame; ++i) {
      simTime += dt_cfl();
      glUniform1f(glGetUniformLocation(compProg, "uTime"), simTime);
      glBindImageTexture(0, tex[readTex], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
      glBindImageTexture(1, tex[1 - readTex], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
      glDispatchCompute(dim / 16, dim / 16, 1);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      readTex = 1 - readTex;
    }


    // render step
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[readTex]);
    glUniform1f(glGetUniformLocation(renderProg, "uH"), h);
    glUniform1f(glGetUniformLocation(renderProg, "uQ"), q);
    glUniform1f(glGetUniformLocation(renderProg, "uL"), L);
    glUniform1f(glGetUniformLocation(renderProg, "uBField"), bField);
    glUniform1i(glGetUniformLocation(renderProg, "uField"), 0);
    glUniform1f(glGetUniformLocation(renderProg, "uPhiEq2"), (alpha < 0.f) ? -alpha / (2.f * beta) : 1.f);

    int w, h_win; glfwGetFramebufferSize(window, &w, &h_win);
    // sidebar occupies right 1/3; two sim panels share the left 2/3
    int simW  = w * 5 / 6;
    int sideW = w - simW;
    glBindVertexArray(VAO);

    glViewport(0, 0, simW / 2, h_win);
    glUniform1i(glGetUniformLocation(renderProg, "uRenderMode"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glViewport(simW / 2, 0, simW / 2, h_win);
    glUniform1i(glGetUniformLocation(renderProg, "uRenderMode"), 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    float xi = sqrt(xi_2());

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
    ImGui::TextColored(ImVec4(0.f,1.f,1.f,1.f), "SYSTEM SCALE");
    ImGui::Text("Box size    %.2f x %.2f m",  L/xi, L/xi);
    ImGui::Text("Grid step   %.3e m",         h/xi);
    ImGui::Text("xi          %.4e m",         xi);
    ImGui::Separator();

    // magnetics
    ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "MAGNETICS");
    ImGui::Text("B field     %.3e", bField);
    ImGui::Text("N target    %d",   N_vortices);
    ImGui::Text("N expected  %.1e", (q * bField * L * L) / (2.f * 3.14159f));
    ImGui::Separator();

    // material
    ImGui::TextColored(ImVec4(1.f,0.5f,0.f,1.f), "MATERIAL");
    ImGui::Text("kappa       %.4e", normKappa());
    ImGui::Text("xi^2        %.4e", xi_2());
    ImGui::Text("lambda^2    %.4e", lam_2());
    if      (normKappa() < 1.f) ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f), "Type-I");
    else if (normKappa() > 1.f) ImGui::TextColored(ImVec4(1.f,0.6f,0.2f,1.f), "Type-II");
    else                        ImGui::TextColored(ImVec4(1.f,1.f,1.f,1.f),   "Bogomolny point");
    ImGui::Separator();

    // controls
    ImGui::TextColored(ImVec4(0.8f,1.f,0.8f,1.f), "CONTROLS");
    ImGui::SliderInt("Steps/Frame", &stepsPerFrame, 10, 500);

    if (ImGui::SliderScalar("Alpha", ImGuiDataType_Double, &alpha, &alpha_min, &alpha_max, "%.3e")) {
      h      = xi_2() / 5.0f;
      L      = dim * h;
      bField = (2.0f*3.14159265f*N_vortices)/(q*L*L);
    }
    ImGui::SliderScalar("Beta", ImGuiDataType_Double, &beta, &beta_min, &beta_max, "%.3e");
    if (ImGui::SliderInt("N vortex", &N_vortices, 0, 128)) {
      bField = (2.0f*3.14159265f*N_vortices)/(q*L*L);
    }

    bool noiseCheck = useNoise;
    if (ImGui::Checkbox("Thermal noise", &noiseCheck)) useNoise = noiseCheck;

    if (ImGui::Button("Random Quench", ImVec2(-1, 0))) quench();
    ImGui::Separator();

    ImGui::Text("%.1f FPS  (%.2f ms)", ImGui::GetIO().Framerate,
                1000.f / ImGui::GetIO().Framerate);
    ImGui::Text("Sim time  %.4e", simTime);

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
