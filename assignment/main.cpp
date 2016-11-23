#define _CRT_SECURE_NO_DEPRECATE

//Some Windows Headers (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include "maths_funcs.h"

// Assimp includes

#include <assimp/cimport.h> // C importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

#include "../SOIL.h"


/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_NAME "../Meshes/plane.obj"
#define MESH_NAME_2 "../Meshes/tree_1.obj"
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

std::vector<float> g_vp, g_vn, g_vt;
std::vector<float> c_vp, c_vn, c_vt;
int g_point_count = 0;
int c_point_count = 0;

unsigned int g_vao = 1;
unsigned int c_vao = 2;

GLuint loc1, loc2, loc3;

// Macro for indexing vertex buffer
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

using namespace std;
GLuint shaderProgramID;

//float eyeX = 0.0f; float eyeY = 0.0f; float eyeZ = -5.0f;
//float lookX = 0.0f; float lookY = 0.0f; float lookZ = 0.0f;
//float upX = 0.0f; float upY = 1.0f; float upZ = 0.0f;

GLuint width = 1200;
GLuint height = 900;

GLfloat rotate_y = 0.0f;

GLfloat speed = 0.2;

vec3 cameraPos = vec3(0.0f, 5.0f, 15.0f);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);

GLfloat yaw = -90.0f;
GLfloat pitch = 0.0f;
GLfloat lastX = width / 2.0;
GLfloat lastY = height / 2.0;


#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

bool load_mesh(const char* file_name) {
	const aiScene* scene = aiImportFile(file_name, aiProcess_Triangulate); // TRIANGLES!
	fprintf(stderr, "Loading mesh: %s\n", file_name);
	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return false;
	}
	printf("  %i animations\n", scene->mNumAnimations);
	printf("  %i cameras\n", scene->mNumCameras);
	printf("  %i lights\n", scene->mNumLights);
	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	if (file_name == MESH_NAME)
		g_point_count = 0;
	if (file_name == MESH_NAME_2)
		c_point_count = 0;

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);

		if (file_name == MESH_NAME)
			g_point_count += mesh->mNumVertices;
		if (file_name == MESH_NAME_2)
			c_point_count += mesh->mNumVertices;

		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				//printf ("      vp %i (%f,%f,%f)\n", v_i, vp->x, vp->y, vp->z);
				if (file_name == MESH_NAME) {
					g_vp.push_back(vp->x);
					g_vp.push_back(vp->y);
					g_vp.push_back(vp->z);
				}
				if (file_name == MESH_NAME_2) {
					c_vp.push_back(vp->x);
					c_vp.push_back(vp->y);
					c_vp.push_back(vp->z);
				}
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				//printf ("      vn %i (%f,%f,%f)\n", v_i, vn->x, vn->y, vn->z);
				if (file_name == MESH_NAME) {
					g_vn.push_back(vn->x);
					g_vn.push_back(vn->y);
					g_vn.push_back(vn->z);
				}
				if (file_name == MESH_NAME_2) {
					c_vn.push_back(vn->x);
					c_vn.push_back(vn->y);
					c_vn.push_back(vn->z);
				}
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				//printf ("      vt %i (%f,%f)\n", v_i, vt->x, vt->y);
				if (file_name == MESH_NAME) {
					g_vt.push_back(vt->x);
					g_vt.push_back(vt->y);
				}
				if (file_name == MESH_NAME_2) {
					c_vt.push_back(vt->x);
					c_vt.push_back(vt->y);
				}
			}
			if (mesh->HasTangentsAndBitangents()) {
				// NB: could store/print tangents here
			}
		}
	}

	aiReleaseImport(scene);
	return true;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS

// Create a NULL-terminated string by reading the provided file
char* readShaderSource(const char* shaderFile) {
	FILE* fp = fopen(shaderFile, "rb"); //!->Why does binary flag "RB" work and not "R"... wierd msvc thing?

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
		exit(0);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024];
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		fprintf(stderr, "Error creating shader program\n");
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, "../Shaders/simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(shaderProgramID, "../Shaders/simpleFragmentShader.txt", GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { 0 };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Invalid shader program: '%s'\n", ErrorLog);
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS

//
//https://www.opengl.org/discussion_boards/showthread.php/185119-Understanding-VAO-s-VBO-s-and-drawing-two-objects
//
void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.

	load_mesh(MESH_NAME);
	unsigned int g_vp_vbo = 0;
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	glGenBuffers(1, &g_vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, g_point_count * 3 * sizeof(float), &g_vp[0], GL_STATIC_DRAW);
	unsigned int g_vn_vbo = 0;
	glGenBuffers(1, &g_vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, g_point_count * 3 * sizeof(float), &g_vn[0], GL_STATIC_DRAW);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	unsigned int vt_vbo = 0;
	//	glGenBuffers (1, &vt_vbo);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glBufferData (GL_ARRAY_BUFFER, g_point_count * 2 * sizeof (float), &g_vt[0], GL_STATIC_DRAW);

	g_vao = 1;
	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, g_vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, g_vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	glEnableVertexAttribArray (loc3);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);

}

//TODO: Make this less bad
void generateConeBufferMesh() {
	load_mesh(MESH_NAME_2);
	unsigned int c_vp_vbo = 0;
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	glGenBuffers(1, &c_vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, c_vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, c_point_count * 3 * sizeof(float), &c_vp[0], GL_STATIC_DRAW);
	unsigned int c_vn_vbo = 0;
	glGenBuffers(1, &c_vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, c_vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, c_point_count * 3 * sizeof(float), &c_vn[0], GL_STATIC_DRAW);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	unsigned int vt_vbo = 0;
	//	glGenBuffers (1, &vt_vbo);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glBufferData (GL_ARRAY_BUFFER, g_point_count * 2 * sizeof (float), &g_vt[0], GL_STATIC_DRAW);

	c_vao = 2;
	glGenVertexArrays(1, &c_vao);
	glBindVertexArray(c_vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, c_vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, c_vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	glEnableVertexAttribArray (loc3);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

#pragma endregion VBO_FUNCTIONS


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);


	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");


	// Root of the Hierarchy
	mat4 view = identity_mat4();
	mat4 persp_proj = perspective(60.0, (float)width / (float)height, 0.1, 1000.0);
	mat4 model = identity_mat4();
	view = look_at(cameraPos, cameraPos + cameraFront, cameraUp);

	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);

	// Bind the plane's VAO and draw
	glBindVertexArray(g_vao);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count);

	// Tree 1 ---------------------------------------
	mat4 treeLocal = identity_mat4();
	treeLocal = rotate_x_deg(treeLocal, -90.0);
	treeLocal = translate(treeLocal, vec3(8.0, 2.5, 2.0));
	treeLocal = scale(treeLocal, vec3(0.5, 0.5, 0.5));

	mat4 treeGlobal = treeLocal * model;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, treeGlobal.m);

	// Bind the Child Object's VAO and draw
	glBindVertexArray(c_vao);
	glDrawArrays(GL_TRIANGLES, 0, c_point_count);


	// Tree 2 ---------------------------------------
	mat4 treeLocal2 = identity_mat4();
	treeLocal2 = rotate_x_deg(treeLocal2, -90.0);
	treeLocal2 = translate(treeLocal2, vec3(12.0, 2.5, 2.0));
	treeLocal2 = scale(treeLocal2, vec3(0.5, 0.5, 0.5));

	mat4 treeGlobal2 = treeLocal2 * model;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, treeGlobal2.m);

	// Bind the Child Object's VAO and draw
	glBindVertexArray(c_vao);
	glDrawArrays(GL_TRIANGLES, 0, c_point_count);

	// Tree 3 ---------------------------------------
	mat4 treeLocal3 = identity_mat4();
	treeLocal3 = rotate_x_deg(treeLocal3, -90.0);
	treeLocal3 = translate(treeLocal3, vec3(10.0, 2.5, 4.0));
	treeLocal3 = scale(treeLocal3, vec3(0.5, 0.5, 0.5));

	mat4 treeGlobal3 = treeLocal3 * model;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, treeGlobal3.m);

	// Bind the Child Object's VAO and draw
	glBindVertexArray(c_vao);
	glDrawArrays(GL_TRIANGLES, 0, c_point_count);

	glBindVertexArray(0);

	glutSwapBuffers();
}


void updateScene() {

	// Placeholder code, if you want to work with framerate
	// Wait until at least 16ms passed since start of last frame (Effectively caps framerate at ~60fps)
	static DWORD  last_time = 0;
	DWORD  curr_time = timeGetTime();
	float  delta = (curr_time - last_time) * 0.001f;
	if (delta > 0.03f)
		delta = 0.03f;
	last_time = curr_time;

	// rotate the model slowly around the y axis
	rotate_y += 0.02f;

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();

	// load mesh into a vertex buffer array
	generateObjectBufferMesh();

	generateConeBufferMesh();

	glBindVertexArray(0);
	glEnable(GL_CULL_FACE);
}

void resetCamera() {
	cameraPos = vec3(0.0f, 5.0f, 10.0f);
	cameraFront = vec3(0.0f, 0.0f, -1.0f);
	cameraUp = vec3(0.0f, 1.0f, 0.0f);
}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {

	GLfloat cameraSpeed = speed;
	switch (key)
	{
	case 'w':
		cameraPos += cameraFront * cameraSpeed;
		break;
	case 's':
		cameraPos -= cameraFront * cameraSpeed;
		break;
	case 'a':
		cameraPos -= normalise(cross(cameraFront, cameraUp)) * cameraSpeed;
		break;
	case 'd':
		cameraPos += normalise(cross(cameraFront, cameraUp)) * cameraSpeed;
		break;
	case ' ':
		resetCamera();
		break;
	case 27:
		exit(0);
		break;
	}

}


bool firstMouse = true;
void mouse(int x, int y) {
	

	if (firstMouse) {
		lastX = x;
		lastY = y;
		firstMouse = false;
	}

	

	GLfloat xoffset = x - lastX;
	GLfloat yoffset = lastY - y;

	lastX = x;
	lastY = y;

	GLfloat mouseSensitivity = 0.5;
	xoffset *= mouseSensitivity;
	yoffset *= mouseSensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	GLfloat to_radians = (3.14159 / 180.0f);

	GLfloat front_x = cos(yaw * to_radians) * cos(pitch * to_radians);
	GLfloat front_y = sin(pitch * to_radians);
	GLfloat front_z = sin(yaw * to_radians) * cos(pitch * to_radians);

	vec3 newFront = normalise(vec3(front_x, front_y, front_z));
	cameraFront = newFront;

}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Hello Triangle");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
	glutPassiveMotionFunc(mouse);
	glutSetCursor(GLUT_CURSOR_NONE);
	glutWarpPointer(width / 2, height / 2);


	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}

	// Set up your objects and shaders
	init();

	// Begin infinite event loop
	glutMainLoop();
	return 0;
}










