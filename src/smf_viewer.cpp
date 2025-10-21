// src/smf_viewer.cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
};

static std::vector<Vertex> vertices;
static std::vector<unsigned int> indices;
static GLuint VAO = 0, VBO = 0, EBO = 0, program = 0;

static float cameraAngle = 0.0f;
static float cameraRadius = 3.0f;
static float cameraHeight = 0.0f;
static bool perspectiveProj = true;

static glm::vec3 modelCentroid(0.0f);
static float modelScale = 1.0f;

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

std::string slurp(const char* path) {
    std::ifstream in(path);
    if(!in) return std::string();
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

GLuint compileGLSL(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        char buf[1024]; glGetShaderInfoLog(s, 1024, NULL, buf);
        std::cerr << "Shader compile error:\n" << buf << std::endl;
    }
    return s;
}

GLuint makeProgramFromFiles(const char* vpath, const char* fpath) {
    std::string vsrc = slurp(vpath);
    std::string fsrc = slurp(fpath);
    if(vsrc.empty() || fsrc.empty()) {
        std::cerr << "Cannot read shader files\n";
        return 0;
    }
    GLuint vs = compileGLSL(GL_VERTEX_SHADER, vsrc.c_str());
    GLuint fs = compileGLSL(GL_FRAGMENT_SHADER, fsrc.c_str());
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok) {
        char buf[1024]; glGetProgramInfoLog(prog, 1024, NULL, buf);
        std::cerr << "Program link error:\n" << buf << std::endl;
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

bool load_smf(const std::string& path, std::vector<glm::vec3>& positions, std::vector<glm::ivec3>& faces) {
    std::ifstream in(path);
    if(!in.is_open()) { std::cerr << "Cannot open SMF file: " << path << std::endl; return false; }
    std::string line; int lineno=0;
    while(std::getline(in, line)) {
        lineno++;
        // trim
        auto first = line.find_first_not_of(" \t\r\n");
        if(first==std::string::npos) continue;
        if(line[first]=='#' || line[first]=='$') continue;
        std::stringstream ss(line);
        std::string tag; ss >> tag;
        if(tag=="v") {
            glm::vec3 p; if(!(ss >> p.x >> p.y >> p.z)) { continue; }
            positions.push_back(p);
        } else if(tag=="f") {
            std::vector<int> idxs;
            std::string token;
            while(ss >> token) {
                // take substring before '/' if present
                size_t slash = token.find('/');
                std::string pri = (slash==std::string::npos) ? token : token.substr(0, slash);
                int id = -1;
                try { id = std::stoi(pri) - 1; } catch(...) { id = -1; }
                if(id < 0) { idxs.clear(); break; }
                idxs.push_back(id);
            }
            if(idxs.size() < 3) continue;
            // triangulate fan
            for(size_t k=1;k+1<idxs.size();++k) faces.push_back(glm::ivec3(idxs[0], idxs[k], idxs[k+1]));
        }
    }
    return !positions.empty() && !faces.empty();
}

bool build_mesh_from_smf(const std::string& filename) {
    std::vector<glm::vec3> positions;
    std::vector<glm::ivec3> faces;
    if(!load_smf(filename, positions, faces)) return false;

    // compute centroid and bounding radius
    glm::vec3 c(0.0f);
    for(auto &p: positions) c += p;
    c /= (float)positions.size();
    float maxd = 0.0f;
    for(auto &p: positions) maxd = std::max(maxd, glm::length(p - c));
    if(maxd <= 0.00001f) maxd = 1.0f;
    modelCentroid = c;
    modelScale = 1.0f / maxd; // scale to roughly unit radius

    // per-vertex normal accumulation
    std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));
    for(auto &f: faces) {
        if((size_t)f.x >= positions.size() || (size_t)f.y >= positions.size() || (size_t)f.z >= positions.size()) continue;
        glm::vec3 v0 = positions[f.y] - positions[f.x];
        glm::vec3 v1 = positions[f.z] - positions[f.x];
        glm::vec3 fn = glm::cross(v0, v1);
        if(glm::length(fn) > 1e-8f) fn = glm::normalize(fn);
        normals[f.x] += fn; normals[f.y] += fn; normals[f.z] += fn;
    }
    // build vertex list and index list (we will reuse indices as loaded)
    vertices.clear();
    indices.clear();
    vertices.resize(positions.size());
    for(size_t i=0;i<positions.size();++i) {
        Vertex v;
        v.Position = positions[i];
        glm::vec3 n = normals[i];
        if(glm::length(n) > 1e-8f) v.Normal = glm::normalize(n);
        else v.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices[i] = v;
    }
    for(auto &f: faces) {
        if((size_t)f.x >= vertices.size() || (size_t)f.y >= vertices.size() || (size_t)f.z >= vertices.size()) continue;
        indices.push_back((unsigned int)f.x);
        indices.push_back((unsigned int)f.y);
        indices.push_back((unsigned int)f.z);
    }

    std::cout << "✅ Loaded " << positions.size() << " vertices and " << (indices.size()/3) << " faces.\n";
    return !vertices.empty() && !indices.empty();
}

void setup_gl_buffers() {
    if(VAO) { glDeleteVertexArrays(1, &VAO); VAO=0; }
    if(VBO) { glDeleteBuffers(1, &VBO); VBO=0; }
    if(EBO) { glDeleteBuffers(1, &EBO); EBO=0; }
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void processInput(GLFWwindow* window) {
    // continuous input
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraAngle -= 0.02f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraAngle += 0.02f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraRadius = std::max(0.05f, cameraRadius - 0.05f);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraRadius += 0.05f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cameraHeight += 0.03f;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cameraHeight -= 0.03f;

    static bool pWasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pWasPressed) { perspectiveProj = !perspectiveProj; std::cout << "Projection: " << (perspectiveProj ? "Perspective" : "Orthographic") << std::endl; }
        pWasPressed = true;
    } else pWasPressed = false;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
}

int main(int argc, char** argv) {
    if(argc < 2) { std::cerr << "Usage: ./smf_viewer <models/your.smf>\n"; return 1; }

    glfwSetErrorCallback([](int e, const char* desc){ std::cerr << "GLFW err " << e << ": " << desc << std::endl; });
    if(!glfwInit()) { std::cerr << "glfwInit failed\n"; return 1; }

    // request context compatible with Mesa in UTM: try core 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(900, 700, "SMF Viewer - normals->color", NULL, NULL);
    if(!window) {
        // fallback: no profile request
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        window = glfwCreateWindow(900, 700, "SMF Viewer - fallback", NULL, NULL);
        if(!window) { std::cerr << "Failed to create GLFW window\n"; glfwTerminate(); return 1; }
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "gladLoadGLLoader failed\n"; glfwTerminate(); return 1; }

    // load model and build mesh
    if(!build_mesh_from_smf(argv[1])) { std::cerr << "Failed to build mesh\n"; glfwTerminate(); return 1; }

    // compile program (we expect shaders in shaders/basic.vert and shaders/basic.frag)
    program = makeProgramFromFiles("shaders/basic.vert", "shaders/basic.frag");
    if(!program) { std::cerr << "Failed to create program\n"; glfwTerminate(); return 1; }

    setup_gl_buffers();
    glEnable(GL_DEPTH_TEST);

    // precompute model transform to center/scale model into view (translate by -centroid then scale)
    glm::mat4 modelTranslate = glm::translate(glm::mat4(1.0f), -modelCentroid);
    glm::mat4 modelScaleM = glm::scale(glm::mat4(1.0f), glm::vec3(modelScale));
    glm::mat4 modelBase = modelScaleM * modelTranslate;

    std::cout << "Controls: A/D rotate, W/S zoom, Q/E height, P toggle projection, ESC exit\n";

    // main loop
    while(!glfwWindowShouldClose(window)) {
        processInput(window);

        int w,h; glfwGetFramebufferSize(window, &w, &h);
        float aspect = (h==0)?1.0f:(float)w/(float)h;
        glViewport(0,0,w,h);
        glClearColor(0.08f,0.08f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // camera on cylinder
        glm::vec3 camPos;
        camPos.x = modelCentroid.x + cameraRadius * cos(cameraAngle);
        camPos.y = modelCentroid.y + cameraHeight;
        camPos.z = modelCentroid.z + cameraRadius * sin(cameraAngle);

        glm::mat4 view = glm::lookAt(camPos, modelCentroid, glm::vec3(0.0f,1.0f,0.0f));
        glm::mat4 proj = perspectiveProj ? glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f)
                                         : glm::ortho(-1.5f*aspect, 1.5f*aspect, -1.5f, 1.5f, -10.0f, 10.0f);

        glUseProgram(program);
        // model = base (center+scale); you can apply extra rotation if desired
        glm::mat4 model = modelBase;
        glUniformMatrix4fv(glGetUniformLocation(program,"model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program,"view"),  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program,"projection"), 1, GL_FALSE, glm::value_ptr(proj));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(program);
    if(VAO) glDeleteVertexArrays(1, &VAO);
    if(VBO) glDeleteBuffers(1, &VBO);
    if(EBO) glDeleteBuffers(1, &EBO);
    glfwTerminate();
    return 0;
}

