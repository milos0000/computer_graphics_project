#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/camera.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/model.h>
#include <learnopengl/shader.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action,
		  int mods);

auto load2DTexture(char const *path) -> unsigned int;

auto loadCubemap(vector<std::string> faces) -> unsigned int;

void renderQuad();

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
	glm::vec3 position;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;

	float constant;
	float linear;
	float quadratic;
};

struct ProgramState {
	glm::vec3 clearColor = glm::vec3(0);
	bool ImGuiEnabled = false;
	Camera camera;
	bool spotLightEnabled = false;
	bool CameraMouseMovementUpdateEnabled = true;
	glm::vec3 cupPosition = glm::vec3(0.0f, 0.0f, -4.0f);
	float cupScale = 0.5f;
	PointLight pointLight;
	ProgramState() : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {};

	void SaveToFile(std::string filename);

	void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename)
{
	std::ofstream out(filename);
	out << clearColor.r << '\n'
	    << clearColor.g << '\n'
	    << clearColor.b << '\n'
	    << ImGuiEnabled << '\n'
	    << spotLightEnabled << '\n'
	    << camera.Position.x << '\n'
	    << camera.Position.y << '\n'
	    << camera.Position.z << '\n'
	    << camera.Front.x << '\n'
	    << camera.Front.y << '\n'
	    << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename)
{
	std::ifstream in(filename);
	if (in) {
		in >> clearColor.r >> clearColor.g >> clearColor.b >>
		    ImGuiEnabled >> spotLightEnabled >> camera.Position.x >>
		    camera.Position.y >> camera.Position.z >> camera.Front.x >>
		    camera.Front.y >> camera.Front.z;
	}
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

auto main() -> int
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
					      "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);
	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// tell stb_image.h to flip loaded texture's on the y-axis (before
	// loading model).
	stbi_set_flip_vertically_on_load(true);

	programState = new ProgramState;
	programState->LoadFromFile("resources/program_state.txt");
	if (programState->ImGuiEnabled) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	// Init Imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330 core");

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	Shader shaderGeometryPass("resources/shaders/g_buffer_cup.vs",
				  "resources/shaders/g_buffer_cup.fs");
	Shader shaderLightingPass("resources/shaders/deferred_shading_cup.vs",
				  "resources/shaders/deferred_shading_cup.fs");

	// build and compile shaders
	// -------------------------
	Shader platformShader("resources/shaders/platform.vs",
			      "resources/shaders/platform.fs");
	Shader grassShader("resources/shaders/grass.vs",
			   "resources/shaders/grass.fs");
	Shader skyboxShader("resources/shaders/skybox.vs",
			    "resources/shaders/skybox.fs");

	float skyboxVertices[] = {
	    // positions
	    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
	    1.0f,  -1.0f, -1.0f, 1.0f,	1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

	    -1.0f, -1.0f, 1.0f,	 -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
	    -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

	    1.0f,  -1.0f, -1.0f, 1.0f,	-1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
	    1.0f,  1.0f,  1.0f,	 1.0f,	1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

	    -1.0f, -1.0f, 1.0f,	 -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
	    1.0f,  1.0f,  1.0f,	 1.0f,	-1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

	    -1.0f, 1.0f,  -1.0f, 1.0f,	1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
	    1.0f,  1.0f,  1.0f,	 -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

	    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
	    1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

	float platformVertices[] = {
	    -0.5f, -0.5f, -0.5f, 0.0f,	0.0f,  -1.0f, 0.0f, 0.0f,
	    0.5f,  -0.5f, -0.5f, 0.0f,	0.0f,  -1.0f, 1.0f, 0.0f,
	    0.5f,  0.5f,  -0.5f, 0.0f,	0.0f,  -1.0f, 1.0f, 1.0f,
	    -0.5f, 0.5f,  -0.5f, 0.0f,	0.0f,  -1.0f, 0.0f, 1.0f,

	    -0.5f, -0.5f, 0.5f,	 0.0f,	0.0f,  1.0f,  0.0f, 0.0f,
	    0.5f,  -0.5f, 0.5f,	 0.0f,	0.0f,  1.0f,  1.0f, 0.0f,
	    0.5f,  0.5f,  0.5f,	 0.0f,	0.0f,  1.0f,  1.0f, 1.0f,
	    -0.5f, 0.5f,  0.5f,	 0.0f,	0.0f,  1.0f,  0.0f, 1.0f,

	    -0.5f, 0.5f,  0.5f,	 -1.0f, 0.0f,  0.0f,  1.0f, 0.0f,
	    -0.5f, 0.5f,  -0.5f, -1.0f, 0.0f,  0.0f,  1.0f, 1.0f,
	    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,  0.0f, 1.0f,
	    -0.5f, -0.5f, 0.5f,	 -1.0f, 0.0f,  0.0f,  0.0f, 0.0f,

	    0.5f,  0.5f,  0.5f,	 1.0f,	0.0f,  0.0f,  1.0f, 0.0f,
	    0.5f,  0.5f,  -0.5f, 1.0f,	0.0f,  0.0f,  1.0f, 1.0f,
	    0.5f,  -0.5f, -0.5f, 1.0f,	0.0f,  0.0f,  0.0f, 1.0f,
	    0.5f,  -0.5f, 0.5f,	 1.0f,	0.0f,  0.0f,  0.0f, 0.0f,

	    -0.5f, -0.5f, -0.5f, 0.0f,	-1.0f, 0.0f,  0.0f, 1.0f,
	    0.5f,  -0.5f, -0.5f, 0.0f,	-1.0f, 0.0f,  1.0f, 1.0f,
	    0.5f,  -0.5f, 0.5f,	 0.0f,	-1.0f, 0.0f,  1.0f, 0.0f,
	    -0.5f, -0.5f, 0.5f,	 0.0f,	-1.0f, 0.0f,  0.0f, 0.0f,

	    -0.5f, 0.5f,  -0.5f, 0.0f,	1.0f,  0.0f,  0.0f, 1.0f,
	    0.5f,  0.5f,  -0.5f, 0.0f,	1.0f,  0.0f,  1.0f, 1.0f,
	    0.5f,  0.5f,  0.5f,	 0.0f,	1.0f,  0.0f,  1.0f, 0.0f,
	    -0.5f, 0.5f,  0.5f,	 0.0f,	1.0f,  0.0f,  0.0f, 0.0f};

	unsigned int platformIndices[] = {0,  2,  1,  2,  0,  3,  4,  5,  6,
					  6,  7,  4,  8,  9,  10, 10, 11, 8,
					  12, 14, 13, 14, 12, 15, 16, 17, 18,
					  18, 19, 16, 20, 22, 21, 22, 20, 23};

	float grassVertices[] = {
	    // positions         // texture Coords (swapped y coordinates
	    // because texture is flipped upside down)
	    0.0f, 0.5f, 0.0f, 0.0f,  0.0f, 0.0f, -0.5f, 0.0f,
	    0.0f, 1.0f, 1.0f, -0.5f, 0.0f, 1.0f, 1.0f,

	    0.0f, 0.5f, 0.0f, 0.0f,  0.0f, 1.0f, -0.5f, 0.0f,
	    1.0f, 1.0f, 1.0f, 0.5f,  0.0f, 1.0f, 0.0f};

	float legPositions[] = {-7.0, -8.0, -10.5, -7.0, -8.0, 1.60,
				5.0,  -8.0, -10.5, 5.0,	 -8.0, 1.60};

	float grassPotPosition[] = {0.0f, 1.25f, 1.0f};
	float grassPosition[] = {-1.0f, 5.3f, 3.8f};

	// configure g-buffer framebuffer
	// ------------------------------
	unsigned int gBuffer;
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	unsigned int gPosition, gNormal, gAlbedoSpec;
	// position color buffer
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0,
		     GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, gPosition, 0);
	// normal color buffer
	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0,
		     GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
			       GL_TEXTURE_2D, gNormal, 0);
	// color + specular color buffer
	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
			       GL_TEXTURE_2D, gAlbedoSpec, 0);
	// tell OpenGL which color attachments we'll use (of this framebuffer)
	// for rendering
	unsigned int attachments[3] = {
	    GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
	glDrawBuffers(3, attachments);
	// create and attach depth buffer (renderbuffer)
	unsigned int rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH,
			      SCR_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				  GL_RENDERBUFFER, rboDepth);
	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// platform
	unsigned int platformVAO, platformVBO, platformEBO;
	glGenVertexArrays(1, &platformVAO);
	glGenBuffers(1, &platformVBO);
	glGenBuffers(1, &platformEBO);

	glBindVertexArray(platformVAO);

	glBindBuffer(GL_ARRAY_BUFFER, platformVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(platformVertices),
		     platformVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(platformIndices),
		     platformIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
			      (void *)nullptr);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
			      (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
			      (void *)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// grass
	unsigned int grassVAO, grassVBO;
	glGenVertexArrays(1, &grassVAO);
	glGenBuffers(1, &grassVBO);
	glBindVertexArray(grassVAO);
	glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(grassVertices), grassVertices,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			      (void *)nullptr);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			      (void *)(3 * sizeof(float)));
	glBindVertexArray(0);

	// skybox
	unsigned int skyboxVAO, skyboxVBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
			      (void *)nullptr);

	vector<std::string> faces{
	    FileSystem::getPath("resources/textures/hotelroom/posx.jpg"),
	    FileSystem::getPath("resources/textures/hotelroom/negx.jpg"),
	    FileSystem::getPath("resources/textures/hotelroom/posy.jpg"),
	    FileSystem::getPath("resources/textures/hotelroom/negy.jpg"),
	    FileSystem::getPath("resources/textures/hotelroom/posz.jpg"),
	    FileSystem::getPath("resources/textures/hotelroom/negz.jpg")};

	unsigned int cubemapTexture = loadCubemap(faces);

	skyboxShader.use();
	skyboxShader.setInt("skybox", 0);

	platformShader.use();
	unsigned int platformDiffuse = load2DTexture(
	    FileSystem::getPath(
		"resources/textures/Stylized_Crate_002_basecolor.jpg")
		.c_str());
	platformShader.setInt("material.diffuse", 0);
	unsigned int platformSpecular = load2DTexture(
	    FileSystem::getPath(
		"resources/textures/Stylized_Crate_002_metallic.jpg")
		.c_str());
	platformShader.setInt("material.specular", 1);
	unsigned int legDiffuse = load2DTexture(
	    FileSystem::getPath("resources/textures/toy_box_diffuse.png")
		.c_str());
	unsigned int land = load2DTexture(
	    FileSystem::getPath("resources/textures/pot.png").c_str());
	unsigned int plastic = load2DTexture(
	    FileSystem::getPath("resources/textures/saksija.jpg").c_str());

	unsigned int cupsDiffuse = load2DTexture(
	    FileSystem::getPath("resources/objects/cup/coffee_cup.jpg")
		.c_str());

	stbi_set_flip_vertically_on_load(false);
	grassShader.use();
	unsigned int grass = load2DTexture(
	    FileSystem::getPath("resources/textures/grass.png").c_str());
	grassShader.setInt("texture1", 3);
	stbi_set_flip_vertically_on_load(true);

	// load models
	// -----------
	Model cupObject("resources/objects/cup/coffee_cup.obj");
	// cupObject.SetShaderTextureNamePrefix("material.");

	PointLight &pointLight = programState->pointLight;
	pointLight.position = glm::vec3(4.0f, 4.0, 0.0);
	pointLight.ambient = glm::vec3(0.5, 0.5, 0.5);
	pointLight.diffuse = glm::vec3(1.0, 1.0, 1.0);
	pointLight.specular = glm::vec3(1.5, 1.5, 1.5);

	pointLight.constant = 1.0f;
	pointLight.linear = 0.09f;
	pointLight.quadratic = 0.032f;

	shaderLightingPass.use();
	shaderLightingPass.setInt("gPosition", 0);
	shaderLightingPass.setInt("gNormal", 1);
	shaderLightingPass.setInt("gAlbedoSpec", 2);

	// draw in wireframe
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window)) {
		// per-frame time logic
		// --------------------
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(programState->clearColor.r,
			     programState->clearColor.g,
			     programState->clearColor.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_CULL_FACE);

		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glm::mat4 projection = glm::perspective(
		    glm::radians(programState->camera.Zoom),
		    (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = programState->camera.GetViewMatrix();
		glm::mat4 model = glm::mat4(1.0f);
		shaderGeometryPass.use();
		shaderGeometryPass.setMat4("projection", projection);
		shaderGeometryPass.setMat4("view", view);

		model = glm::mat4(1.0f);
		model = glm::translate(model, programState->cupPosition);
		model = glm::scale(model, glm::vec3(programState->cupScale));
		shaderGeometryPass.setMat4("model", model);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, cupsDiffuse);
		cupObject.Draw(shaderGeometryPass);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// 2. lighting pass: calculate lighting by iterating over a
		// screen filled quad pixel-by-pixel using the gbuffer's
		// content.
		// -----------------------------------------------------------------------------------------------------------------------
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shaderLightingPass.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
		// send light relevant uniforms

		shaderLightingPass.setVec3("lights[0].Position",
					   programState->pointLight.position);
		shaderLightingPass.setVec3("lights[0].Color",
					   glm::vec3(0.5, 0.5, 0.5));
		// update attenuation parameters and calculate radius
		const float linear = 0.7;
		const float quadratic = 1.8;
		shaderLightingPass.setFloat("lights[0].Linear", linear);
		shaderLightingPass.setFloat("lights[0].Quadratic", quadratic);

		shaderLightingPass.setVec3("viewPos",
					   programState->camera.Position);
		// finally render quad
		renderQuad();

		// 2.5. copy content of geometry's depth buffer to default
		// framebuffer's depth buffer
		// ----------------------------------------------------------------------------------
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
				  0); // write to default framebuffer
		// blit to default framebuffer. Note that this may or may not
		// work as the internal formats of both the FBO and default
		// framebuffer have to match. the internal formats are
		// implementation defined. This works on all of my systems, but
		// if it doesn't on yours you'll likely have to write to the
		// depth buffer in another shader stage (or somehow see to match
		// the default framebuffer's internal format with the FBO's
		// internal format).
		glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH,
				  SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		platformShader.use();

		platformShader.setVec3("dirLight.direction", -0.2f, -1.0f,
				       -0.3f);
		platformShader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
		platformShader.setVec3("dirLight.diffuse", 0.15f, 0.15f, 0.15f);
		platformShader.setVec3("dirLight.specular", 0.3f, 0.3f, 0.3f);

		platformShader.setVec3("pointLight.position",
				       pointLight.position);
		platformShader.setVec3("pointLight.ambient",
				       pointLight.ambient);
		platformShader.setVec3("pointLight.diffuse",
				       pointLight.diffuse);
		platformShader.setVec3("pointLight.specular",
				       pointLight.specular);
		platformShader.setFloat("pointLight.constant",
					pointLight.constant);
		platformShader.setFloat("pointLight.linear", pointLight.linear);
		platformShader.setFloat("pointLight.quadratic",
					pointLight.quadratic);
		platformShader.setVec3("viewPos",
				       programState->camera.Position);
		platformShader.setFloat("material.shininess", 32.0f);

		platformShader.setVec3("spotLight.position",
				       programState->camera.Position);
		platformShader.setVec3("spotLight.direction",
				       programState->camera.Front);
		platformShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
		if (programState->spotLightEnabled) {
			platformShader.setVec3("spotLight.diffuse", 1.0f, 1.0f,
					       1.0f);
			platformShader.setVec3("spotLight.specular", 1.0f, 1.0f,
					       1.0f);
		} else {
			platformShader.setVec3("spotLight.diffuse", 0.0f, 0.0f,
					       0.0f);
			platformShader.setVec3("spotLight.specular", 0.0f, 0.0f,
					       0.0f);
		}
		platformShader.setFloat("spotLight.constant", 1.0f);
		platformShader.setFloat("spotLight.linear", 0.09);
		platformShader.setFloat("spotLight.quadratic", 0.032);
		platformShader.setFloat("spotLight.cutOff",
					glm::cos(glm::radians(12.5f)));
		platformShader.setFloat("spotLight.outerCutOff",
					glm::cos(glm::radians(15.0f)));

		model = glm::mat4(1.0f); // make sure to initialize matrix to
					 // identity matrix first
		model = glm::translate(model, glm::vec3(-1.0f, -1.0f, -4.5f));
		model = glm::scale(model, glm::vec3(15.0, 2.0, 15.0));

		platformShader.setMat4("model", model);
		platformShader.setMat4("view", view);
		platformShader.setMat4("projection", projection);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, platformDiffuse);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, platformSpecular);

		// don't forget to enable shader before setting uniforms
		glBindVertexArray(
		    platformVAO); // seeing as we only have a single VAO there's
				  // no need to bind it every time, but we'll do
				  // so to keep things a bit more organized
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		// legs
		for (int i = 0; i < 4; i++) {
			model = glm::mat4(1.0f);
			model = glm::translate(
			    model, glm::vec3(legPositions[i * 3],
					     legPositions[i * 3 + 1],
					     legPositions[i * 3 + 2]));
			model = glm::scale(model, glm::vec3(2.0, 15.0, 2.0));
			platformShader.setMat4("model", model);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, legDiffuse);
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT,
				       nullptr);
		}

		// pot
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(grassPotPosition[0],
							grassPotPosition[1],
							grassPotPosition[2]));
		model = glm::scale(model, glm::vec3(2.5, 2.5, 2.5));
		platformShader.setMat4("model", model);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, plastic);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
		// land
		model = glm::mat4(1.0f);
		model =
		    glm::translate(model, glm::vec3(grassPotPosition[0],
						    grassPotPosition[1] + 1.28f,
						    grassPotPosition[2]));
		model = glm::scale(model, glm::vec3(2.5, 0.05, 2.5));
		platformShader.setMat4("model", model);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, land);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
		glBindTexture(GL_TEXTURE_2D, 0);

		// grass
		glDisable(GL_CULL_FACE);
		grassShader.use();
		glBindVertexArray(grassVAO);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, grass);
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(grassPosition[0],
							grassPosition[1],
							grassPosition[2]));
		model = glm::rotate(model, 45.0f, glm::vec3(0.0f, 2.0f, 0.0f));
		model = glm::scale(model, glm::vec3(5.5, 5.5, 5.5));
		grassShader.setMat4("model", model);
		grassShader.setMat4("view", view);
		grassShader.setMat4("projection", projection);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		// draw skybox as last
		glDepthFunc(GL_LEQUAL); // change depth function so depth test
					// passes when values are equal to depth
					// buffer's content
		skyboxShader.use();
		view = glm::mat4(
		    glm::mat3(programState->camera
				  .GetViewMatrix())); // remove translation from
						      // the view matrix
		skyboxShader.setMat4("view", view);
		skyboxShader.setMat4("projection", projection);
		// skybox cube
		glBindVertexArray(skyboxVAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		glDepthFunc(GL_LESS); // set depth function back to default

		if (programState->ImGuiEnabled)
			DrawImGui(programState);

		// glfw: swap buffers and poll IO events (keys pressed/released,
		// mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteVertexArrays(1, &platformVAO);
	glDeleteBuffers(1, &platformVBO);
	glDeleteBuffers(1, &platformEBO);
	programState->SaveToFile("resources/program_state.txt");
	delete programState;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
	return 0;
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
	if (quadVAO == 0) {
		float quadVertices[] = {
		    // positions        // texture Coords
		    -1.0f, 1.0f, 0.0f,	0.0f, 1.0f, -1.0f, -1.0f,
		    0.0f,  0.0f, 0.0f,	1.0f, 1.0f, 0.0f,  1.0f,
		    1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices),
			     &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
				      5 * sizeof(float), (void *)nullptr);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
				      5 * sizeof(float),
				      (void *)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		programState->camera.ProcessKeyboard(FORWARD, deltaTime * 5);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		programState->camera.ProcessKeyboard(BACKWARD, deltaTime * 5);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		programState->camera.ProcessKeyboard(LEFT, deltaTime * 5);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		programState->camera.ProcessKeyboard(RIGHT, deltaTime * 5);
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that
	// width and height will be significantly larger than specified on
	// retina displays.
	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset =
	    lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	if (programState->CameraMouseMovementUpdateEnabled)
		programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
	programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	{
		static float f = 0.0f;
		ImGui::Begin("Hello window");
		ImGui::Text("Hello text");
		ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
		ImGui::ColorEdit3("Background color",
				  (float *)&programState->clearColor);
		ImGui::DragFloat3("Cup position",
				  (float *)&programState->cupPosition);
		ImGui::DragFloat("Cup scale", &programState->cupScale, 0.05,
				 0.1, 4.0);

		ImGui::DragFloat("pointLight.constant",
				 &programState->pointLight.constant, 0.05, 0.0,
				 1.0);
		ImGui::DragFloat("pointLight.linear",
				 &programState->pointLight.linear, 0.05, 0.0,
				 1.0);
		ImGui::DragFloat("pointLight.quadratic",
				 &programState->pointLight.quadratic, 0.05, 0.0,
				 1.0);
		ImGui::End();
	}

	{
		ImGui::Begin("Camera info");
		const Camera &c = programState->camera;
		ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x,
			    c.Position.y, c.Position.z);
		ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
		ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y,
			    c.Front.z);
		ImGui::Checkbox(
		    "Camera mouse update",
		    &programState->CameraMouseMovementUpdateEnabled);
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
		  int mods)
{
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
		programState->ImGuiEnabled = !programState->ImGuiEnabled;
		if (programState->ImGuiEnabled) {
			programState->CameraMouseMovementUpdateEnabled = false;
			glfwSetInputMode(window, GLFW_CURSOR,
					 GLFW_CURSOR_NORMAL);
		} else {
			glfwSetInputMode(window, GLFW_CURSOR,
					 GLFW_CURSOR_DISABLED);
		}
	}
	if (key == GLFW_KEY_F && action == GLFW_PRESS) {
		programState->spotLightEnabled =
		    !programState->spotLightEnabled;
	}
}
auto load2DTexture(char const *path) -> unsigned int
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char *data =
	    stbi_load(path, &width, &height, &nrComponents, 0);
	if (data) {
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
			     GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR);

		stbi_image_free(data);
	} else {
		std::cout << "Texture failed to load at path: " << path
			  << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}
auto loadCubemap(vector<std::string> faces) -> unsigned int
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(false);
	for (unsigned int i = 0; i < faces.size(); i++) {
		unsigned char *data = stbi_load(faces[i].c_str(), &width,
						&height, &nrChannels, 0);
		if (data) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
				     GL_RGB, width, height, 0, GL_RGB,
				     GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		} else {
			std::cout << "Cubemap texture failed to load at path: "
				  << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
			GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
			GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
			GL_CLAMP_TO_EDGE);
	stbi_set_flip_vertically_on_load(true);

	return textureID;
}