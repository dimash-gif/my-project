// shading_demo_full.cpp
// Self-contained SMF viewer with Gouraud & Phong shading, two lights, materials,
// robust keyboard input handling for UTM (GL 3.0 compatibility / GLSL 130).

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

struct Vertex { glm::vec3 pos; glm::vec3 normal; };
struct Material { glm::vec3 ambient, diffuse, specular; float shininess; };

static std::vector<Vertex> g_vertices;
static std::vector<unsigned int> g_indices;
static GLuint g_VAO = 0, g_VBO = 0, g_EBO = 0;

static bool g_usePhong = true;
static int g_materialIndex = 0;
static Material g_materials[3];

static float camAngle = 0.0f, camRadius = 3.5f, camHeight = 0.0f;
static float lightAngle = 0.0f, lightRadius = 2.0f, lightHeight = 0.5f;

static bool g_perspective = true;

static double lastFrameTime = 0.0;

// input edge detection
static bool prevKeys[1024];

static void setDefaultMaterials() {
    g_materials[0] = { {0.6f,0.2f,0.2f}, {0.9f,0.1f,0.1f}, {0.8f,0.8f,0.8f}, 80.0f }; // bright specular
    g_materials[1] = { {0.0215f,0.1745f,0.0215f}, {0.07568f,0.61424f,0.07568f}, {0.633f,0.727811f,0.633f}, 76.8f }; // emerald
    g_materials[2] = { {0.0f,0.05f,0.05f}, {0.4f,0.5f,0.5f}, {0.04f,0.7f,0.7f}, 10.0f }; // cyan rubber
}

// small helper to read file to string (SMF loader uses this not for shaders)
static std::string readFileToString(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";
    std::stringstream ss; ss << ifs.rdbuf(); return ss.str();
}

// SMF loader: reads 'v x y z' and 'f i j k' lines (1-based indices)
static bool loadSMF(const std::string &filename, std::vector<glm::vec3> &positions, std::vector<glm::ivec3> &faces) {
    std::ifstream in(filename);
    if (!in.is_open()) { std::cerr << "Cannot open SMF: " << filename << '\n'; return false; }
    std::string line; int ln = 0;
    while (std::getline(in, line)) {
        ++ln;
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        char type; ss >> type;
        if (type == 'v') {
            glm::vec3 p; if (!(ss >> p.x >> p.y >> p.z)) { std::cerr<<"Bad v at "<<ln<<"\n"; continue; }
            positions.push_back(p);
        } else if (type == 'f') {
            glm::ivec3 f; if (!(ss >> f.x >> f.y >> f.z)) { std::cerr<<"Bad f at "<<ln<<"\n"; continue; }
            // convert to 0-based index
            f -= glm::ivec3(1);
            // sanity check
            if (f.x < 0 || f.y < 0 || f.z < 0) { std::cerr<<"Invalid face idx at "<<ln<<"\n"; continue; }
            faces.push_back(f);
        }
    }
    return !positions.empty() && !faces.empty();
}

// embed vertex/fragment shader sources (GLSL 130)
static const char* gouraud_vs = R"(
#version 130
in vec3 aPos;
in vec3 aNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;
uniform vec3 worldLightPos;
uniform vec3 worldLightAmbient;
uniform vec3 worldLightDiffuse;
uniform vec3 worldLightSpec;
uniform vec3 cameraLightPos;
uniform vec3 cameraLightAmbient;
uniform vec3 cameraLightDiffuse;
uniform vec3 cameraLightSpec;
uniform vec3 materialAmbient;
uniform vec3 materialDiffuse;
uniform vec3 materialSpec;
uniform float materialShininess;
out vec3 outColor;
void main() {
    vec3 FragPos = vec3(model * vec4(aPos,1.0));
    vec3 N = normalize(mat3(model) * aNormal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 result = vec3(0.0);

    // world light
    vec3 L1 = normalize(worldLightPos - FragPos);
    float diff1 = max(dot(N, L1), 0.0);
    vec3 R1 = reflect(-L1, N);
    float spec1 = pow(max(dot(viewDir, R1), 0.0), materialShininess);
    result += worldLightAmbient * materialAmbient;
    result += worldLightDiffuse * diff1 * materialDiffuse;
    result += worldLightSpec * spec1 * materialSpec;

    // camera light
    vec3 L2 = normalize(cameraLightPos - FragPos);
    float diff2 = max(dot(N, L2), 0.0);
    vec3 R2 = reflect(-L2, N);
    float spec2 = pow(max(dot(viewDir, R2), 0.0), materialShininess);
    result += cameraLightAmbient * materialAmbient;
    result += cameraLightDiffuse * diff2 * materialDiffuse;
    result += cameraLightSpec * spec2 * materialSpec;

    outColor = result;
    gl_Position = projection * view * model * vec4(aPos,1.0);
}
)";

static const char* gouraud_fs = R"(
#version 130
in vec3 outColor;
out vec4 FragColor;
void main() { FragColor = vec4(outColor, 1.0); }
)";

static const char* phong_vs = R"(
#version 130
in vec3 aPos;
in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    FragPos = vec3(model * vec4(aPos,1.0));
    Normal = normalize(mat3(model) * aNormal);
    gl_Position = projection * view * model * vec4(aPos,1.0);
}
)";

static const char* phong_fs = R"(
#version 130
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;
uniform vec3 viewPos;
uniform vec3 worldLightPos;
uniform vec3 worldLightAmbient;
uniform vec3 worldLightDiffuse;
uniform vec3 worldLightSpec;
uniform vec3 cameraLightPos;
uniform vec3 cameraLightAmbient;
uniform vec3 cameraLightDiffuse;
uniform vec3 cameraLightSpec;
uniform vec3 materialAmbient;
uniform vec3 materialDiffuse;
uniform vec3 materialSpec;
uniform float materialShininess;
void main() {
    vec3 N = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 result = vec3(0.0);

    vec3 L1 = normalize(worldLightPos - FragPos);
    float diff1 = max(dot(N, L1), 0.0);
    vec3 R1 = reflect(-L1, N);
    float spec1 = pow(max(dot(viewDir, R1), 0.0), materialShininess);
    result += worldLightAmbient * materialAmbient;
    result += worldLightDiffuse * diff1 * materialDiffuse;
    result += worldLightSpec * spec1 * materialSpec;

    vec3 L2 = normalize(cameraLightPos - FragPos);
    float diff2 = max(dot(N, L2), 0.0);
    vec3 R2 = reflect(-L2, N);
    float spec2 = pow(max(dot(viewDir, R2), 0.0), materialShininess);
    result += cameraLightAmbient * materialAmbient;
    result += cameraLightDiffuse * diff2 * materialDiffuse;
    result += cameraLightSpec * spec2 * materialSpec;

    FragColor = vec4(result, 1.0);
}
)";

// utility to compile program from embedded source
static GLuint compileProgramFromSources(const char* vsSrc, const char* fsSrc) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, NULL);
    glCompileShader(vs);
    GLint ok; glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetShaderInfoLog(vs, 1024, NULL, buf); std::cerr<<"VS compile error: "<<buf<<"\n"; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetShaderInfoLog(fs, 1024, NULL, buf); std::cerr<<"FS compile error: "<<buf<<"\n"; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aNormal");
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { char buf[1024]; glGetProgramInfoLog(prog, 1024, NULL, buf); std::cerr<<"Link error: "<<buf<<"\n"; }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// build mesh buffers from SMF positions/faces
static bool buildMeshFromSMF(const std::string &path) {
    std::vector<glm::vec3> pos;
    std::vector<glm::ivec3> faces;
    if (!loadSMF(path, pos, faces)) { std::cerr<<"SMF load failed\n"; return false; }
    if (faces.size() < 1) { std::cerr<<"No faces\n"; return false; }

    // compute averaged normals
    std::vector<glm::vec3> normals(pos.size(), glm::vec3(0.0f));
    for (auto &f : faces) {
        if ((size_t)f.x >= pos.size() || (size_t)f.y >= pos.size() || (size_t)f.z >= pos.size()) continue;
        glm::vec3 v1 = pos[f.y] - pos[f.x];
        glm::vec3 v2 = pos[f.z] - pos[f.x];
        glm::vec3 fn = glm::normalize(glm::cross(v1, v2));
        normals[f.x] += fn; normals[f.y] += fn; normals[f.z] += fn;
    }

    g_vertices.clear(); g_indices.clear();
    for (size_t i=0;i<pos.size();++i) {
        Vertex v; v.pos = pos[i]; v.normal = glm::normalize(normals[i]); g_vertices.push_back(v);
    }
    for (auto &f : faces) { g_indices.push_back(f.x); g_indices.push_back(f.y); g_indices.push_back(f.z); }

    // create GL buffers
    glGenVertexArrays(1, &g_VAO);
    glGenBuffers(1, &g_VBO);
    glGenBuffers(1, &g_EBO);
    glBindVertexArray(g_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, g_vertices.size()*sizeof(Vertex), g_vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_indices.size()*sizeof(unsigned int), g_indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,normal));
    glBindVertexArray(0);
    std::cout << "✅ Loaded " << pos.size() << " vertices and " << faces.size() << " faces.\n";
    return true;
}

// input processing with edge detection
static bool keyPressedOnce(GLFWwindow* w, int key) {
    int state = glfwGetKey(w, key);
    bool pressed = (state == GLFW_PRESS || state == GLFW_REPEAT);
    if (pressed && !prevKeys[key]) { prevKeys[key] = true; return true; }
    if (!pressed) prevKeys[key] = false;
    return false;
}

// process continuous keys each frame
static void processContinuousInput(GLFWwindow* win) {
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) camAngle -= 0.02f;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) camAngle += 0.02f;
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) { camRadius -= 0.04f; if (camRadius<0.2f) camRadius=0.2f; }
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) camRadius += 0.04f;
    // light adjustments via arrow keys / I K U O
    if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) lightAngle -= 0.02f;
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) lightAngle += 0.02f;
    if (glfwGetKey(win, GLFW_KEY_I) == GLFW_PRESS) lightRadius -= 0.04f;
    if (glfwGetKey(win, GLFW_KEY_K) == GLFW_PRESS) lightRadius += 0.04f;
    if (glfwGetKey(win, GLFW_KEY_U) == GLFW_PRESS) lightHeight += 0.04f;
    if (glfwGetKey(win, GLFW_KEY_O) == GLFW_PRESS) lightHeight -= 0.04f;
}

// main
int main(int argc, char** argv) {
    if (argc < 2) { std::cerr<<"Usage: "<<argv[0]<<" <model.smf>\n"; return -1; }
    std::string modelPath = argv[1];

    if (!glfwInit()) { std::cerr<<"GLFW init fail\n"; return -1; }

    // request compatibility-friendly context (UTM safe)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_ANY_PROFILE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

    GLFWwindow* window = glfwCreateWindow(900, 700, "SMF Shading (Gouraud/Phong)", NULL, NULL);
    if (!window) { std::cerr << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr<<"GLAD init failed\n"; return -1; }

    // build mesh
    if (!buildMeshFromSMF(modelPath)) { std::cerr << "Failed to build mesh\n"; return -1; }

    // compile shaders
    GLuint gouraudProg = compileProgramFromSources(gouraud_vs, gouraud_fs);
    GLuint phongProg = compileProgramFromSources(phong_vs, phong_fs);

    setDefaultMaterials();

    glEnable(GL_DEPTH_TEST);

    // init prev keys
    for (int i=0;i<1024;++i) prevKeys[i]=false;

    // main loop
    lastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        double dt = now - lastFrameTime;
        lastFrameTime = now;

        // input: continuous
        processContinuousInput(window);

        // input: edge toggles
        if (keyPressedOnce(window, GLFW_KEY_G)) { g_usePhong = !g_usePhong; std::cout << "Shading: " << (g_usePhong ? "Phong\n" : "Gouraud\n"); }
        if (keyPressedOnce(window, GLFW_KEY_P)) { g_perspective = !g_perspective; std::cout << "Projection: " << (g_perspective ? "Perspective\n" : "Orthographic\n"); }
        if (keyPressedOnce(window, GLFW_KEY_1)) { g_materialIndex = 0; std::cout<<"Material 1\n"; }
        if (keyPressedOnce(window, GLFW_KEY_2)) { g_materialIndex = 1; std::cout<<"Material 2\n"; }
        if (keyPressedOnce(window, GLFW_KEY_3)) { g_materialIndex = 2; std::cout<<"Material 3\n"; }
        if (keyPressedOnce(window, GLFW_KEY_ESCAPE)) { glfwSetWindowShouldClose(window, true); }

        // animate camera slowly? (optional)
        // camAngle += 0.0f; // keep controlled by user

        // compute transforms & light positions
        glm::vec3 camPos( camRadius * cos(camAngle), camHeight, camRadius * sin(camAngle) );
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f,1.0f,0.0f));
        int w,h; glfwGetFramebufferSize(window, &w, &h);
        float aspect = (w>0 && h>0) ? (float)w/(float)h : 1.0f;
        glm::mat4 proj = g_perspective ? glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f)
                                       : glm::ortho(-camRadius*aspect, camRadius*aspect, -camRadius, camRadius, -100.0f, 100.0f);
        glm::mat4 model = glm::mat4(1.0f);

        // lights
        glm::vec3 worldLightPos( lightRadius * cos(lightAngle), lightHeight, lightRadius * sin(lightAngle) );
        glm::vec3 cameraLightPos = camPos; // camera-space light attached to eye
        glm::vec3 worldLightAmbient(0.2f), worldLightDiffuse(0.6f), worldLightSpec(1.0f);
        glm::vec3 cameraLightAmbient(0.1f), cameraLightDiffuse(0.4f), cameraLightSpec(0.5f);

        glClearColor(0.07f,0.08f,0.12f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLuint prog = g_usePhong ? phongProg : gouraudProg;
        glUseProgram(prog);

        // upload common uniforms
        GLint loc_model = glGetUniformLocation(prog, "model");
        GLint loc_view  = glGetUniformLocation(prog, "view");
        GLint loc_proj  = glGetUniformLocation(prog, "projection");
        GLint loc_viewPos = glGetUniformLocation(prog, "viewPos"); // phong expects viewPos; gouraud uses it too

        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(loc_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(loc_proj, 1, GL_FALSE, glm::value_ptr(proj));
        if (loc_viewPos != -1) glUniform3fv(loc_viewPos, 1, glm::value_ptr(camPos));

        // lights
        auto setVec3 = [&](const char* name, const glm::vec3 &v){
            GLint L = glGetUniformLocation(prog, name);
            if (L != -1) glUniform3fv(L, 1, glm::value_ptr(v));
        };
        auto setFloat = [&](const char* name, float val){ GLint L = glGetUniformLocation(prog, name); if (L != -1) glUniform1f(L, val); };
        setVec3("worldLightPos", worldLightPos);
        setVec3("worldLightAmbient", worldLightAmbient);
        setVec3("worldLightDiffuse", worldLightDiffuse);
        setVec3("worldLightSpec", worldLightSpec);
        setVec3("cameraLightPos", cameraLightPos);
        setVec3("cameraLightAmbient", cameraLightAmbient);
        setVec3("cameraLightDiffuse", cameraLightDiffuse);
        setVec3("cameraLightSpec", cameraLightSpec);

        // material
        setVec3("materialAmbient", g_materials[g_materialIndex].ambient);
        setVec3("materialDiffuse", g_materials[g_materialIndex].diffuse);
        setVec3("materialSpec", g_materials[g_materialIndex].specular);
        setFloat("materialShininess", g_materials[g_materialIndex].shininess);

        // draw
        glBindVertexArray(g_VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)g_indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &g_VAO);
    glDeleteBuffers(1, &g_VBO);
    glDeleteBuffers(1, &g_EBO);
    glfwTerminate();
    return 0;
}

