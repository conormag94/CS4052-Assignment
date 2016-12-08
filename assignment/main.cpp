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
#include "text.h"
#include <irrKlang.h>


/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_NAME "../Meshes/plane.obj"
#define AMMO_CAPACITY 15

/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/
// Mesh Variables
const int num_meshes = 5;
char* mesh_names[num_meshes] = { "../Meshes/plane_2.obj", "../Meshes/tower.obj", "../Meshes/windmill2.obj", "../Meshes/enemy.obj", "../Meshes/gun.obj" };
std::vector<float> g_vp[num_meshes], g_vn[num_meshes], g_vt[num_meshes];
int g_point_count[num_meshes] = { 0, 0, 0, 0, 0};
unsigned int g_vao[num_meshes] = { 1, 2, 3, 4, 5};

GLuint loc1, loc2, loc3;

bool warped;
bool fired;
bool freecam_enabled = false;

// Texture Stuff
int tWidth, tHeight;
unsigned char* image;
GLuint texture1, texture2;

// Macro for indexing vertex buffer
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

using namespace irrklang;
ISoundEngine *SoundEngine = createIrrKlangDevice();

using namespace std;

GLuint shaderProgramID;

GLuint width = 1920;
GLuint height =1080;

GLfloat rotate_y = 0.0f;
vec3 enemyPos = vec3(0.0, 0.0, 0.0);

GLfloat speed = 0.15;
int score = 0;

const int ALIVE_STATE = 1;
const int DEAD_STATE = -1;

int crosshair_id, score_id, player_info_id, enemy_info_id, bullet_info_id;
int player_hb_id;

// Camera starting position
vec3 cameraPos = vec3(0.0f, 1.5f, 20.0f);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);

vec3 translateTest = vec3(0.0, 0.0, 0.0);

GLfloat yaw = -90.0f;
GLfloat pitch = 0.0f;
GLfloat lastX = width / 2.0;
GLfloat lastY = height / 2.0;


class BoundingBox
{
public:
	vec3 min;
	vec3 max;
};

class Bullet
{
public:
	vec3 bullet_position;
	vec3 bullet_front;
	float speed;
	bool is_active;
};

class Enemy
{
public:
	vec3 enemy_position;
	vec3 enemy_front;
	float speed;
	BoundingBox hitbox;
	int enemy_state;
};

Bullet testBullet;	

//Bullet bullets[5];
std::vector<Bullet> bullets;
std::vector<Enemy> enemies;

BoundingBox player_hitbox;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

bool load_mesh(const char* file_name, int mesh_index) {
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

	g_point_count[mesh_index] = 0;

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);

		g_point_count[mesh_index] += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				//printf ("      vp %i (%f,%f,%f)\n", v_i, vp->x, vp->y, vp->z);
				g_vp[mesh_index].push_back(vp->x);
				g_vp[mesh_index].push_back(vp->y);
				g_vp[mesh_index].push_back(vp->z);
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				//printf ("      vn %i (%f,%f,%f)\n", v_i, vn->x, vn->y, vn->z);
				g_vn[mesh_index].push_back(vn->x);
				g_vn[mesh_index].push_back(vn->y);
				g_vn[mesh_index].push_back(vn->z);

			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				//printf ("      vt %i (%f,%f)\n", v_i, vt->x, vt->y);
				g_vt[mesh_index].push_back(vt->x);
				g_vt[mesh_index].push_back(vt->y);
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
void generateObjectBufferMesh(int mesh_index) {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.
	char* mesh_name = mesh_names[mesh_index];
	load_mesh(mesh_name, mesh_index);
	unsigned int g_vp_vbo = 0;
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	glGenBuffers(1, &g_vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, g_point_count[mesh_index] * 3 * sizeof(float), &g_vp[mesh_index][0], GL_STATIC_DRAW);
	unsigned int g_vn_vbo = 0;
	glGenBuffers(1, &g_vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, g_point_count[mesh_index] * 3 * sizeof(float), &g_vn[mesh_index][0], GL_STATIC_DRAW);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	unsigned int vt_vbo = 0;
	glGenBuffers (1, &vt_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	glBufferData (GL_ARRAY_BUFFER, g_point_count[mesh_index] * 2 * sizeof (float), &g_vt[mesh_index][0], GL_STATIC_DRAW);


	g_vao[mesh_index] = mesh_index + 1;
	glGenVertexArrays(1, &g_vao[mesh_index]);
	glBindVertexArray(g_vao[mesh_index]);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, g_vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, g_vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	////	This is for texture coordinates which you don't currently need, so I have commented it out
	glEnableVertexAttribArray (loc3);
	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);

}

#pragma endregion VBO_FUNCTIONS

void spawn_bullet() {
	Bullet temp;
	vec3 _cameraPos = cameraPos;
	temp.bullet_position = _cameraPos + cameraFront;
	temp.bullet_front = cameraFront;
	temp.speed = 50.0f;
	bullets.push_back(temp);
}

void move_bullets(float delta) {
	int len = bullets.size();
	for (int i = 0; i < len; i++) {
		vec3 distance_to_move = (bullets[i].bullet_front * (delta * bullets[i].speed));
		bullets[i].bullet_position += distance_to_move;
	}
}

void spawn_enemy() {
	float xpos = (float)(enemies.size());
	Enemy temp;
	temp.enemy_position = vec3(xpos, 0.0, 0.0);
	temp.enemy_front = vec3(0.0, 0.0, 1.0);
	BoundingBox _box;
	_box.min = vec3(temp.enemy_position.v[0] - 0.5f, 0, temp.enemy_position.v[2] - 0.5f);
	_box.max = vec3(temp.enemy_position.v[0] + 0.5f, 1, temp.enemy_position.v[2] + 0.5f);
	temp.hitbox = _box;
	temp.speed = 5.0f;
	temp.enemy_state = ALIVE_STATE;
	enemies.push_back(temp);
}

void move_enemies(float delta) {
	for (int i = 0; i < enemies.size(); i++) {
		if (enemies[i].enemy_state == ALIVE_STATE) {
			enemies[i].enemy_position += (enemies[i].enemy_front * (delta * enemies[i].speed));
			BoundingBox _box;
			_box.min = vec3(enemies[i].enemy_position.v[0] - 0.5f, 0, enemies[i].enemy_position.v[2] - 0.5f);
			_box.max = vec3(enemies[i].enemy_position.v[0] + 0.5f, 1, enemies[i].enemy_position.v[2] + 0.5f);
			enemies[i].hitbox = _box;
		}
	}
}

void updateScore(int score_increase) {
	score += score_increase;
	char new_score[50];
	sprintf(new_score, "Score: %d\n", score);
	update_text(score_id, new_score);
}

// Prints object positions on screen for debugging
void positionInfo() {
	char temp[50];
	sprintf(temp, "P: (%f, %f, %f)\n", cameraPos.v[0], cameraPos.v[1], cameraPos.v[2]);
	update_text(player_info_id, temp);
	sprintf(temp, "E: (%f, %f, %f)\n", enemyPos.v[0], enemyPos.v[1], enemyPos.v[2]);
	update_text(enemy_info_id, temp);
}

void bulletInfo() {
	char temp[50];
	sprintf(temp, "B: (%f, %f, %f)\n", testBullet.bullet_position.v[0], testBullet.bullet_position.v[1], testBullet.bullet_position.v[2]);
	update_text(bullet_info_id, temp);
}

void hitbox_info() {
	char temp[50];
	sprintf(temp, "Min: (%f, %f, %f)\n", player_hitbox.min.v[0], player_hitbox.min.v[1], player_hitbox.min.v[2]);
	update_text(player_hb_id, temp);
}


bool intersect(BoundingBox a, BoundingBox b) {
	return (
		(a.min.v[0] <= b.max.v[0] && a.max.v[0] >= b.min.v[0]) &&	
		(a.min.v[1] <= b.max.v[1] && a.max.v[1] >= b.min.v[1]) &&
		(a.min.v[2] <= b.max.v[2] && a.max.v[2] >= b.min.v[2])
	);
	
}

bool collided_with(Bullet b, Enemy e) {
	vec3 min = e.hitbox.min;
	vec3 max = e.hitbox.max;
	vec3 pos = b.bullet_position;
	return (
		pos.v[0] >= min.v[0] &&
		pos.v[0] <= max.v[0] &&
		pos.v[1] >= min.v[1] &&
		pos.v[1] <= max.v[1] &&
		pos.v[2] >= min.v[2] &&
		pos.v[2] <= max.v[2]
	);

}
void doCollision() {
	for (int i = 0; i < bullets.size(); i++) {
		Bullet b = bullets[i];
		for (int j = 0; j < enemies.size(); j++){
			Enemy e = enemies[j];
			if (e.enemy_state == ALIVE_STATE && collided_with(b, e)) {
				updateScore(101);
				enemies[j].enemy_state = DEAD_STATE;
			}
		}
	}
}

void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.7f, 0.7f, 0.8f, 1.0f);
	//glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glUseProgram(shaderProgramID);

	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");


	// Root of the Hierarchy
	mat4 view = identity_mat4();
	mat4 persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model = identity_mat4();
	model = translate(model, translateTest);
	view = look_at(cameraPos, cameraPos + cameraFront, cameraUp);

	//glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture1);

	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);

	// Bind the plane's VAO and draw
	glBindVertexArray(g_vao[0]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[0]);


	// ************WINDMILL TOWER*********************
	glBindTexture(GL_TEXTURE_2D, texture2);
	glUniform1i(glGetUniformLocation(shaderProgramID, "theTexture"), 0);

	mat4 towerLocal = identity_mat4();
	towerLocal = rotate_y_deg(towerLocal, -90.0f);
	towerLocal = translate(towerLocal, vec3(-5.0f, 0.0f, -20.0f));
	
	mat4 towerGlobal = model * towerLocal;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, towerGlobal.m);

	glBindVertexArray(g_vao[1]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[1]);

	// *************WINDMILL FAN*************************
	mat4 windmillLocal = identity_mat4();
	windmillLocal = rotate_x_deg(windmillLocal, rotate_y);
	windmillLocal = translate(windmillLocal, vec3(2.0, 7.0, 0.0));

	mat4 windmillGlobal = towerGlobal * windmillLocal;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, windmillGlobal.m);

	glBindVertexArray(g_vao[2]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[2]);

	// *************ENEMY***************************
	//mat4 enemyLocal = identity_mat4();
	//enemyLocal = scale(enemyLocal, vec3(0.5, 0.5, 0.5));
	//enemyLocal = translate(enemyLocal, enemyPos);
	////enemyLocal = translate(enemyLocal, vec3(12.0, 0.0, -3.0));
	//
	//
	//mat4 enemyGlobal = model * enemyLocal;
	//glUniformMatrix4fv(matrix_location, 1, GL_FALSE, enemyGlobal.m);

	//glBindVertexArray(g_vao[3]);
	//glDrawArrays(GL_TRIANGLES, 0, g_point_count[3]);
	for (int i = 0; i < enemies.size(); i++) {
		if (enemies[i].enemy_state == ALIVE_STATE) {
			mat4 enemyLocal = identity_mat4();
			enemyLocal = scale(enemyLocal, vec3(0.5, 0.5, 0.5));
			enemyLocal = translate(enemyLocal, enemies[i].enemy_position);
			//enemyLocal = translate(enemyLocal, vec3(12.0, 0.0, -3.0));


			mat4 enemyGlobal = model * enemyLocal;
			glUniformMatrix4fv(matrix_location, 1, GL_FALSE, enemyGlobal.m);

			glBindVertexArray(g_vao[3]);
			glDrawArrays(GL_TRIANGLES, 0, g_point_count[3]);
		}
	}

	if (fired) {
		for (int i = 0; i < bullets.size(); i++) {
			mat4 bulletLocal = identity_mat4();
			bulletLocal = scale(bulletLocal, vec3(0.02, 0.02, 0.02));
			bulletLocal = translate(bulletLocal, bullets[i].bullet_position);
			//bulletLocal = scale(bulletLocal, vec3(0.25, 0.25, 0.25));

			mat4 bulletGlobal = bulletLocal;
			glUniformMatrix4fv(matrix_location, 1, GL_FALSE, bulletGlobal.m);

			glBindVertexArray(g_vao[3]);
			glDrawArrays(GL_TRIANGLES, 0, g_point_count[3]);
		}
	}
	
	// ***********GUN*******************
	glBindTexture(GL_TEXTURE_2D, texture1);
	mat4 gunView = identity_mat4();
	mat4 gunPersp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 gunModel = identity_mat4();

	gunModel = rotate_y_deg(gunModel, 180.0);

	// Gun on RHS
	gunModel = translate(gunModel, vec3(0.05, -0.05, -0.15));
	

	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, gunPersp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, gunView.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, gunModel.m);


	glBindVertexArray(g_vao[4]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[4]);


	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);

	draw_texts ();

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

	if (fired) {
		bulletInfo();
		//doCollision(testBullet);
		//if (bullets.size() > 0)
			//move_bullets(delta);

	}
	for (int i = 0; i < enemies.size(); i++) {
		if (intersect(enemies[i].hitbox, player_hitbox))
			updateScore(-score);
	}
	// rotate the model slowly around the y axis
	rotate_y += 10.0f * delta;
	enemyPos.v[2] += 2.0f * delta;
	move_enemies(delta);
	move_bullets(delta);
	doCollision();
	hitbox_info();

	//enemyPos.v[0] += 0.01f;
	//vec3 distance_moved = (testBullet.bullet_front * (delta * 50.0f));
	//testBullet.bullet_position += distance_moved;// testBullet.bullet_front;

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	SoundEngine->play2D("../Audio/breakout.mp3", GL_TRUE);

	warped = false;
	fired = false;

	player_hitbox.min = vec3 (cameraPos.v[0] - 0.5f, 0, cameraPos.v[2] - 0.5f);
	player_hitbox.max = vec3(cameraPos.v[0] + 0.5f, cameraPos.v[1] + 0.5f, cameraPos.v[2] + 0.5f);

	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();

	init_text_rendering("../freemono.png", "../freemono.meta", width, height);
	// x and y are -1 to 1
	// size_px is the maximum glyph size in pixels (try 100.0f)
	// r,g,b,a are red,blue,green,opacity values between 0.0 and 1.0
	// if you want to change the text later you will use the returned integer as a parameter
	crosshair_id = add_text("+", -0.01f, 0.05f, 35.0f, 1.0f, 1.0f, 1.0f, 0.75f);
	score_id = add_text("Score: 0", -0.95f, 0.9f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	player_info_id = add_text("Player Position", -0.95f, -0.85f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	enemy_info_id = add_text("Enemy Position", -0.95f, -0.75f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	bullet_info_id = add_text("Bullet Position", -0.95f, -0.65f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	player_hb_id = add_text("Player HB Position", 0.5f, -0.65f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);

	glGenTextures(1, &texture1);
	glBindTexture(GL_TEXTURE_2D, texture1); // All upcoming GL_TEXTURE_2D operations now have effect on our texture object
											// Set our texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// Set texture wrapping to GL_REPEAT
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set texture filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	image = SOIL_load_image("../Textures/green.png", &tWidth, &tHeight, 0, SOIL_LOAD_RGBA);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tWidth, tHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);
	SOIL_free_image_data(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &texture2);
	glBindTexture(GL_TEXTURE_2D, texture2); // All upcoming GL_TEXTURE_2D operations now have effect on our texture object
											// Set our texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// Set texture wrapping to GL_REPEAT
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set texture filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	image = SOIL_load_image("../Textures/palette.png", &tWidth, &tHeight, 0, SOIL_LOAD_RGBA);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tWidth, tHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);
	SOIL_free_image_data(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	// load mesh into a vertex buffer array
	generateObjectBufferMesh(0);
	generateObjectBufferMesh(1);
	generateObjectBufferMesh(2);
	generateObjectBufferMesh(3);
	generateObjectBufferMesh(4);

	glBindVertexArray(0);
	glEnable(GL_CULL_FACE);
}


#pragma region MOVEMENT_CALLBACKS
void resetCamera() {
	cameraPos = vec3(10.0f, 1.5f, 2.0f);
	cameraFront = vec3(0.0f, 0.0f, -1.0f);
	cameraUp = vec3(0.0f, 1.0f, 0.0f);
}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	printf("Pos: %f, %f, %f | Front: %f, %f, %f\n",
		testBullet.bullet_position.v[0], testBullet.bullet_position.v[1], testBullet.bullet_position.v[2],
		testBullet.bullet_front.v[0], testBullet.bullet_front.v[1], testBullet.bullet_front.v[2]);

	
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
	case 'e':
		spawn_enemy();
		break;
	case 'f':
		freecam_enabled = (freecam_enabled == true) ? false : true;
		break;
	case ' ':
		resetCamera();
		//updateScore(10);
		break;
	case 27:
		exit(0);
		break;
	case 'u':
		translateTest.v[0] -= 1.0f;
		break;
	case 'i':
		translateTest.v[0] += 1.0f;
		break;

	}
	// Keeps user at ground level
	if (!freecam_enabled)
		cameraPos.v[1] = 1.5f;
	positionInfo();
	player_hitbox.min = vec3(cameraPos.v[0] - 0.5f, 0, cameraPos.v[2] - 0.5f);
	player_hitbox.max = vec3(cameraPos.v[0] + 0.5f, cameraPos.v[1] + 0.5f, cameraPos.v[2] + 0.5f);

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

	GLfloat mouseSensitivity = 0.3;
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

void mouseClick(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		updateScore(10);
		vec3 _cameraPos = cameraPos;
		//_cameraPos.v[0] += 0.4;
		//_cameraPos.v[1] -= 0.4;
		
		spawn_bullet(); // Will replace the below code

		testBullet.bullet_position = _cameraPos + cameraFront;
		//testBullet.bullet_position.v[1] -= 0.4;
		//testBullet.bullet_position.v[0] += 0.4;
		testBullet.bullet_front = cameraFront;
		fired = true;
		positionInfo();
	}
}
#pragma endregion MOVEMENT_CALLBACKS


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
	glutMouseFunc(mouseClick);
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










