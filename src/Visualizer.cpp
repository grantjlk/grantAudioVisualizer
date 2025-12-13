#include "Visualizer.h"
// Vertex shader source - renders bars as instanced quads
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
// IMPORTANT: change barheights if i make it variable
uniform float barHeights[32];
uniform int barCount;

void main() {
    int barIndex = gl_InstanceID;
    float barWidth = 2.0 / float(barCount);
    float spacing = barWidth * 0.1;
    float actualWidth = barWidth - spacing;
    
    // Calculate bar position
    float xOffset = -1.0 + barWidth * float(barIndex) + spacing * 0.5;
    
    // Scale height (clamp to prevent overflow)
    float height = min(barHeights[barIndex] * 10.0, 2.0);
    
    // Position vertex
    vec2 pos = aPos;
    pos.x = pos.x * actualWidth + xOffset + actualWidth * 0.5;
    pos.y = pos.y * height - 1.0; //+ height if we want it to float
    
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

// Fragment shader source - simple solid color
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 barColor;

void main() {
    FragColor = vec4(barColor, 1.0);
}
)";

Visualizer::Visualizer(int width, int height, int numBars)
	: window(nullptr), windowWidth(width), windowHeight(height),
    shaderProgram(0), VAO(0), VBO(0),
    numBars(numBars), smoothingFactor(0.5f) {
    
	barHeights.resize(numBars, 0.0f);
    smoothedHeights.resize(numBars, 0.0f);
}

Visualizer::~Visualizer(){
	// clean up opengl resources
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
	// terminate glfw
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

// rteurns true if successfully setup window, false if not.
bool Visualizer::setupWindow(){
	// initialize GLFW
    if (!glfwInit()) {
		std::cerr << "GLFW failed to initialize\n";
		return false;
	}
	// set OpenGL version to 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	window = glfwCreateWindow(windowWidth, windowHeight, "Audio Visualizer", nullptr, nullptr);

	if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

	
    glfwMakeContextCurrent(window);

	// set resize callback (needed because of c style function)
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

	glViewport(0, 0, windowWidth, windowHeight);
    
    return true;
}
// typical shader compilation code
GLuint Visualizer::_compileShader(const char* source, GLenum type){
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    // check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed:\n" << infoLog << "\n";
        return 0;
    }
    
    return shader;
}

// typical shader program creation code
GLuint Visualizer::_createShaderProgram(GLuint vertexShader, GLuint fragmentShader){
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << "\n";
        return 0;
    }
    
    return program;
}
bool Visualizer::setupShaders(){
	GLuint vertexShader = _compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = _compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);
    
    if (!vertexShader || !fragmentShader) {
        return false;
    }
    
    shaderProgram = _createShaderProgram(vertexShader, fragmentShader);
    // clean up shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!shaderProgram) {
        return false;
    }
    // get uniform locations
    uniformBarHeights = glGetUniformLocation(shaderProgram, "barHeights");
    uniformBarCount = glGetUniformLocation(shaderProgram, "barCount");
    uniformBarColor = glGetUniformLocation(shaderProgram, "barColor");
    
    return true;
}

void Visualizer::setupGeometry(){
	// Define a simple quad (two triangles)
    float vertices[] = {
        // x, y
        0.0f, 0.0f,  // bottom-left
        1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f,  // top-right
        
        1.0f, 1.0f,  // top-right
        0.0f, 1.0f,  // top-left
        0.0f, 0.0f   // bottom-left
    };
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void Visualizer::framebufferSizeCallback(GLFWwindow* window, int width, int height){
	glViewport(0, 0, width, height);
	//update stored dimensions(using static cast because this func cannot access our members)
    Visualizer* vis = static_cast<Visualizer*>(glfwGetWindowUserPointer(window));
    if (vis) {
        vis->windowWidth = width;
        vis->windowHeight = height;
    }
}

bool Visualizer::initialize(){
	if(!setupWindow()){
        return false;
    }
    if(!setupShaders()){
        return false;
    }
    setupGeometry();

    return true;
}
void Visualizer::updateData(const std::vector<float>& buckets){
    if (buckets.size() != barHeights.size()) {
        std::cerr << "Warning: Bucket count mismatch\n";
        return;
    }
    
    // apply smoothing by setting new height to the last height * smoothing factor + new bucket * 1 - smoothing factor
    for(size_t i = 0; i < buckets.size(); i++){
        smoothedHeights[i] = smoothedHeights[i] * smoothingFactor + buckets[i] * (1.0f - smoothingFactor);
        barHeights[i] = smoothedHeights[i];
    }
}

void Visualizer::render(){
    // Clear screen
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    
    glUseProgram(shaderProgram);
    // set uniforms
    glUniform1fv(uniformBarHeights, numBars, barHeights.data());
    glUniform1i(uniformBarCount, numBars);
    glUniform3f(uniformBarColor, 0.2f, 0.8f, 0.9f);
    // Draw instanced bars
    glBindVertexArray(VAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, numBars);
    glBindVertexArray(0);

    glfwSwapBuffers(window);
}

bool Visualizer::shouldClose() const{
    if (!window) return true;
    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        return true;
    }
    
    return glfwWindowShouldClose(window);
}

void Visualizer::pollEvents(){
    glfwPollEvents();
}

void Visualizer::setSmoothingFactor(float factor){
    smoothingFactor = factor;
}