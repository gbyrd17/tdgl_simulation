#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <complex>
#include <cmath>
#include <random>

float alpha = -1.f, beta = 1.0f, gamma_ = 1.0f, mc = 1.0f, q = 1.0f;
float xi_2()      { return 1.0f / (4.0f * mc * std::abs(alpha)); }
float lam_2()     { return (mc * beta) / (q * q * std::abs(alpha)); }
float normKappa() { return sqrt(lam_2() / (2 * xi_2())); }
bool useNoise = false; 

// config
const int dim     = 1024; // 256
float h           = xi_2() / 5;
float L           = dim * h;
int stepsPerFrame = 100;


// parameters
int N_vortices = 4;
float bField = (2.0f * 3.14159265f * N_vortices) / (q * L * L * dim * dim);

// sim params
float dt_cfl() { return (0.0002f * h * h) / (4.0f * xi_2() * gamma_); }

//  load shaders
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    if (!file) { std::cerr << "Failed to open " << path << "\n"; return ""; }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

GLuint compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c_src = src.c_str();
    glShaderSource(s, 1, &c_src, nullptr);
    glCompileShader(s);
    int success; char info[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(s, 512, nullptr, info); std::cerr << "Error: " << info << "\n"; }
    return s;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // compute shaders need 4.3+
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1024, 512, "GPU TDGL Solver", NULL, NULL);
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
      std::uniform_real_distribution<float> dist(0, 2.0f * 3.14159f);
      float eq = std::sqrt(std::abs(alpha) / (2.0f * beta));
      for(int i=0; i<dim*dim; ++i) {
          float theta = dist(rng);
          initData[i*4 + 0] = eq * cos(theta); // re
          initData[i*4 + 1] = eq * sin(theta); // im
      }
      for (int i =0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, dim, dim, GL_RGBA, GL_FLOAT, initData.data());
      }
      useNoise = false;
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
        glUniform1f(glGetUniformLocation(compProg, "uUseNoise"), useNoise);
        glUniform1f(glGetUniformLocation(compProg, "uDt"), dt_cfl());
        glUniform1f(glGetUniformLocation(compProg, "uBField"), bField);

        // run multiple steps per frame
        for (int i = 0; i < stepsPerFrame; ++i) {
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
        glBindVertexArray(VAO);

        glViewport(0, 0, w/2, h_win);
        glUniform1i(glGetUniformLocation(renderProg, "uRenderMode"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glViewport(w/2, 0, w/2, h_win);
        glUniform1i(glGetUniformLocation(renderProg, "uRenderMode"), 1);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // build out ui
        ImGui::Begin("Simulation Controls");
        ImGui::Text("Resolution: %d x %d", dim, dim);
        ImGui::SliderInt("Steps/Frame", &stepsPerFrame, 100, 500);
        
        if (ImGui::SliderFloat("Alpha (Temp)", &alpha, -5.0f, 0.0f)) {
            // bField needs to be stable relative to quantization
            h = xi_2() / 2.0f; // Update resolution
            L = dim * h;       // Update physical size
            // RECALCULATE B to keep flux quantized
            bField = (2.0f * 3.14159265f * N_vortices) / (q *L * L);
        }
        
        if (ImGui::SliderInt("Vortex Count", &N_vortices, 0, 512)) {
            bField = (2.0f * 3.14159265f * N_vortices) / (q *L * L);
        }
        
        ImGui::Separator();
        ImGui::Text("B Field: %.3e", bField);
        ImGui::Text("Expected Vortex Count: %.1f", (q * bField * L * L) / (2.0f * M_PI));
        ImGui::Value("Xi^2", xi_2());
        ImGui::Value("normKappa", normKappa());

        if (ImGui::Button("Random Quench", ImVec2(-1, 0))) {
            quench();
        }
        
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        if (normKappa() < 1) {
          ImGui::Text("Superconductor is of Type-I");
        } else if (normKappa() > 1) {
          ImGui::Text("Superconductor is of Type-II");
        } else {
          ImGui::Text("Superconductor type cannot be determined");
        }

        ImGui::End();

        // final render pass for ui
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
