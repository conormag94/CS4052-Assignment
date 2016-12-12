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
#include <random>

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
#define MIN_X -27.0f
#define MAX_X 27.5f
#define MIN_Z -27.5f
#define MAX_Z 27.5f
#define NUM_SPAWNS 10
#define NUM_COLLIDABLES 2
#define TIME_UNTIL_MORNING 60


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/
// Mesh Variables
const int num_meshes = 7;
char* mesh_names[num_meshes] = { 
	"../Meshes/plane_2.obj", 
	"../Meshes/tower.obj", 
	"../Meshes/windmill2.obj", 
	"../Meshes/zombie.obj", 
	"../Meshes/gun.obj",
	"../Meshes/bullet.obj",
	"../Meshes/house.obj"
};

vec3 spawn_points[NUM_SPAWNS] = {
	vec3(0.0f, 0.0f, 0.0f),
	vec3(2.0f, 0.0f, 0.0f),
	vec3(4.0f, 0.0f, 0.0f),
	vec3(6.0f, 0.0f, 0.0f),
	vec3(8.0f, 0.0f, 0.0f),
	vec3(-8.0f, 0.0f, 0.0f),
	vec3(-6.0f, 0.0f, 0.0f),
	vec3(-4.0f, 0.0f, 0.0f),
	vec3(-2.0f, 0.0f, 0.0f),
	vec3(20.0f, 0.0f, 0.0f),
};

std::vector<float> g_vp[num_meshes], g_vn[num_meshes], g_vt[num_meshes];
int g_point_count[num_meshes];// = { 0, 0, 0, 0, 0 };
unsigned int g_vao[num_meshes];// = { 1, 2, 3, 4, 5 };

GLuint loc1, loc2, loc3;

bool fired;
bool freecam_enabled = false;
bool sound_enabled = false;
bool game_over = false;
bool time_started = false;
bool is_player_alive = true;

long elapsed_time;
long start_time;

int enemies_spawned = 0;

// Texture Stuff
int tWidth, tHeight;
unsigned char* image;
GLuint texture1, tex_atlas;

// Macro for indexing vertex buffer
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

using namespace irrklang;
ISoundEngine *SoundEngine = createIrrKlangDevice();

using namespace std;

GLuint shaderProgramID;

GLuint width = 1920;
GLuint height =1080;

GLfloat rotate_y = 0.0f;

GLfloat speed = 0.15;
int score = 0;

const int ALIVE_STATE = 1;
const int DEAD_STATE = -1;

int crosshair_id, score_id, player_info_id, enemy_info_id, bullet_info_id;
int player_hb_id, game_over_id, time_id;

// Camera starting position
vec3 cameraPos = vec3(0.0f, 1.5f, 10.0f);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);

GLfloat sky_red = 0.2f;
GLfloat sky_green = 0.2f;
GLfloat sky_blue = 0.2f;
GLfloat fog_density = 0.1f;

vec3 plane_translation = vec3(0.0, 0.0, 0.0);

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

BoundingBox collidables[NUM_COLLIDABLES];

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
	temp.speed = 80.0f;
	bullets.push_back(temp);
}

void move_bullets(float delta) {
	int len = bullets.size();
	for (int i = 0; i < len; i++) {
		vec3 distance_to_move = (bullets[i].bullet_front * (delta * bullets[i].speed));
		bullets[i].bullet_position += distance_to_move;
	}
}

// Pick random spawn locations from array of possible spawn point
vec3 get_spawn_point() {
	std::random_device rd;     // only used once to initialise (seed) engine
	std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)
	std::uniform_int_distribution<int> uni(0, NUM_SPAWNS - 1); // guaranteed unbiased

	auto random_integer = uni(rng);

	return spawn_points[random_integer];
}

void spawn_enemy() {
	float xpos = (float)(enemies.size()); //TODO: Change this to random spot
	Enemy temp;
	vec3 spawn_point = get_spawn_point();
	printf("Spawning Enemy %d @ <%f, %f, %f>\n", enemies_spawned++, spawn_point.v[0], spawn_point.v[1], spawn_point.v[2]);
	temp.enemy_position = spawn_point;//vec3(xpos, 0.0, 0.0);
	temp.enemy_front = vec3(0.0, 0.0, 1.0);
	BoundingBox _box;
	_box.min = vec3(temp.enemy_position.v[0] - 0.4f, 0, temp.enemy_position.v[2] - 0.4);
	_box.max = vec3(temp.enemy_position.v[0] + 0.4f, 1.65f, temp.enemy_position.v[2] + 0.4f);
	temp.hitbox = _box;
	temp.speed = 2.0f;
	temp.enemy_state = ALIVE_STATE;
	enemies.push_back(temp);
}

void move_enemies(float delta) {
	for (int i = 0; i < enemies.size(); i++) {
		if (enemies[i].enemy_state == ALIVE_STATE) {
			vec3 dir = vec3(cameraPos.v[0] - enemies[i].enemy_position.v[0], 0.0f, cameraPos.v[2] - enemies[i].enemy_position.v[2]);
			
			enemies[i].enemy_front = normalise(dir);
			enemies[i].enemy_position += (enemies[i].enemy_front * (delta * enemies[i].speed));
			BoundingBox _box;
			_box.min = vec3(enemies[i].enemy_position.v[0] - 0.4f, 0, enemies[i].enemy_position.v[2] - 0.4f);
			_box.max = vec3(enemies[i].enemy_position.v[0] + 0.4f, 1.65f, enemies[i].enemy_position.v[2] + 0.4f);
			enemies[i].hitbox = _box;
		}
	}
}

void updateScore(int score_increase) {
	score += score_increase;
	char new_score[50];
	sprintf(new_score, "Zombies Killed: %d\n", score);
	update_text(score_id, new_score);
}

void update_time() {
	int time = TIME_UNTIL_MORNING - (elapsed_time / 1000);
	char t[50];
	sprintf(t, "Time until morning: %d\n", time);
	update_text(time_id, t);
}

// Prints object positions on screen for debugging
void positionInfo() {
	char temp[50];
	sprintf(temp, "P: (%f, %f, %f)\n", cameraPos.v[0], cameraPos.v[1], cameraPos.v[2]);
	update_text(player_info_id, temp);
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

// Check if two objects are colliding
bool _intersect(BoundingBox a, BoundingBox b) {
	/*return (
		(a.min.v[0] <= b.max.v[0] && a.max.v[0] >= b.min.v[0]) &&	
		(a.min.v[1] <= b.max.v[1] && a.max.v[1] >= b.min.v[1]) &&
		(a.min.v[2] <= b.max.v[2] && a.max.v[2] >= b.min.v[2])
	);*/
	bool res = (
		a.max.v[0] > b.min.v[0] &&
		a.min.v[0] < b.max.v[0] &&
		a.max.v[1] > b.min.v[1] &&
		a.min.v[1] < b.max.v[1] &&
		a.max.v[2] > b.min.v[2] &&
		a.min.v[2] < b.max.v[2]
		);
	return res;
}

bool p_int(vec3 pos, BoundingBox o) {
	vec3 min = o.min;
	vec3 max = o.max;
	return (
		pos.v[0] >= min.v[0] &&
		pos.v[0] <= max.v[0] &&
		pos.v[1] >= min.v[1] &&
		pos.v[1] <= max.v[1] &&
		pos.v[2] >= min.v[2] &&
		pos.v[2] <= max.v[2]
		);
}

// Check if a bullet has colliding with an enemy
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

void add_collidable(int index, vec3 min, vec3 max) {
	BoundingBox temp;
	temp.min = min;
	temp.max = max;
	collidables[index] = temp;
}

void do_bullet_collision() {
	for (int i = 0; i < bullets.size(); i++) {
		Bullet b = bullets[i];
		if (b.is_active) {
			for (int j = 0; j < enemies.size(); j++) {
				Enemy e = enemies[j];
				if (e.enemy_state == ALIVE_STATE && collided_with(b, e)) {
					updateScore(1);
					enemies[j].enemy_state = DEAD_STATE;
					bullets[i].is_active = false;
				}
			}
		}
	}
}

//bool player_can_move() {
//	bool res = true;
//	for (size_t i = 0; i < collidables.size(); i++) {
//		printf("%d/%d: ", i, collidables.size());
//		printf("min %f, %f", collidables[i].min.v[0], collidables[i].max.v[2]);
//		if (_intersect(player_hitbox, collidables[i])) {
//			printf("Collision\n");
//			res = false;
//		}
//	}
//	return res;
//}

void do_object_collision() {
	for (int i = 0; i < enemies.size(); i++) {
		if (enemies[i].enemy_state == ALIVE_STATE && _intersect(enemies[i].hitbox, player_hitbox)) {
			is_player_alive = false;
			game_over = true;
		}
	}
}

bool still_in_bounds(vec3 new_position) {
	return (
		new_position.v[0] > MIN_X &&
		new_position.v[0] < MAX_X &&
		new_position.v[2] > MIN_Z &&
		new_position.v[2] < MAX_Z
	);
}

void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(sky_red, sky_green, sky_blue, 1.0f);
	//glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glUseProgram(shaderProgramID);
	

	if (game_over) {
		if (sky_blue < 0.8f)
			sky_blue += 0.001f;
		if (sky_green < 0.7f)
			sky_green += 0.001f;
		fog_density -= 0.0005f;
		if (fog_density <= 0.0f)
			fog_density = 0.0f;
	}
	/*if (sky_blue < 0.8f)
		sky_blue += 0.00001f;
	else if (sky_green < 0.7f)
		sky_green += 0.0001f;*/

	glUniform1f(glGetUniformLocation(shaderProgramID, "sky_red"), sky_red);
	glUniform1f(glGetUniformLocation(shaderProgramID, "sky_green"), sky_green);
	glUniform1f(glGetUniformLocation(shaderProgramID, "sky_blue"), sky_blue);
	glUniform1f(glGetUniformLocation(shaderProgramID, "fog_density"), fog_density);

	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");


	// ********** Plane (Root of the Hierarchy) **********
	mat4 view = identity_mat4();
	mat4 persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model = identity_mat4();
	model = translate(model, plane_translation);
	view = look_at(cameraPos, cameraPos + cameraFront, cameraUp);

	//glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_atlas);

	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);

	// Bind the plane's VAO and draw
	glBindVertexArray(g_vao[0]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[0]);

	// ********** House **********
	glBindTexture(GL_TEXTURE_2D, tex_atlas);

	mat4 houseLocal = identity_mat4();
	houseLocal = scale(houseLocal, vec3(0.5, 0.5, 0.5));
	houseLocal = rotate_y_deg(houseLocal, 180.0f);
	houseLocal = translate(houseLocal, vec3(0.0f, 0.0f, 20.0f));

	mat4 houseGlobal = model * houseLocal;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, houseGlobal.m);

	glBindVertexArray(g_vao[6]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[6]);

	// ********** Windmill Tower **********
	glBindTexture(GL_TEXTURE_2D, tex_atlas);
	glUniform1i(glGetUniformLocation(shaderProgramID, "theTexture"), 0);

	mat4 towerLocal = identity_mat4();
	towerLocal = rotate_y_deg(towerLocal, -45.0f);
	towerLocal = translate(towerLocal, vec3(-20.0f, 0.0f, -20.0f));
	
	mat4 towerGlobal = model * towerLocal;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, towerGlobal.m);

	glBindVertexArray(g_vao[1]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[1]);

	// ********** Windmill fan **********
	mat4 windmillLocal = identity_mat4();
	windmillLocal = rotate_x_deg(windmillLocal, rotate_y);
	windmillLocal = translate(windmillLocal, vec3(2.0, 7.0, 0.0));

	mat4 windmillGlobal = towerGlobal * windmillLocal;
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, windmillGlobal.m);

	glBindVertexArray(g_vao[2]);
	glDrawArrays(GL_TRIANGLES, 0, g_point_count[2]);

	// ********** Enemies **********
	for (int i = 0; i < enemies.size(); i++) {
		if (enemies[i].enemy_state == ALIVE_STATE) {
			mat4 enemyLocal = identity_mat4();
			float enemy_rot = direction_to_heading(enemies[i].enemy_front * -1.0f);
			enemyLocal = rotate_y_deg(enemyLocal, -90.0f);
			enemyLocal = rotate_y_deg(enemyLocal, enemy_rot);

			//enemyLocal = scale(enemyLocal, vec3(0.5, 0.5, 0.5));
			enemyLocal = translate(enemyLocal, enemies[i].enemy_position);

			mat4 enemyGlobal = model * enemyLocal;
			glUniformMatrix4fv(matrix_location, 1, GL_FALSE, enemyGlobal.m);

			glBindVertexArray(g_vao[3]);
			glDrawArrays(GL_TRIANGLES, 0, g_point_count[3]);
		}
	}

	// ********** Bullets **********
	for (int i = 0; i < bullets.size(); i++) {
		if (bullets[i].is_active) {
			mat4 bulletLocal = identity_mat4();
			// Rotate bullets to always face away from player
			float bullet_rot = direction_to_heading(cameraFront * -1.0f);
			bulletLocal = rotate_y_deg(bulletLocal, -90.0f);
			bulletLocal = rotate_y_deg(bulletLocal, bullet_rot);
			bulletLocal = scale(bulletLocal, vec3(0.5, 0.5, 0.5));
			//bulletLocal = rotate_y_deg(bulletLocal, 90.0f);
			bulletLocal = translate(bulletLocal, bullets[i].bullet_position);

			mat4 bulletGlobal = bulletLocal;
			glUniformMatrix4fv(matrix_location, 1, GL_FALSE, bulletGlobal.m);

			glBindVertexArray(g_vao[5]);
			glDrawArrays(GL_TRIANGLES, 0, g_point_count[5]);
		}
	}
	
	// ********** Gun **********
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

	elapsed_time = glutGet(GLUT_ELAPSED_TIME);

	if (!game_over)
		update_time();

	if (TIME_UNTIL_MORNING - (elapsed_time / 1000) == 0) {
		game_over = true;
	}


	if (!game_over) {
		rotate_y += 10.0f * delta;
		// Check Enemy, Player collisions
		do_object_collision();
		do_bullet_collision();
		move_enemies(delta);
		move_bullets(delta);
		
	}
	else {
		if (is_player_alive)
			game_over_id = add_text("You survived the night", -0.3, 0.2, 50.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		else 
			game_over_id = add_text("Game Over\n You Died", -0.1, 0.2, 50.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	}
	//hitbox_info();

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	if (sound_enabled)
		SoundEngine->play2D("../Audio/breakout.mp3", GL_TRUE);

	fired = false;
	elapsed_time = 0;
	
	/*BoundingBox temp;
	temp.min = vec3(-4.0, 0, 17.0);
	temp.max = vec3(4.0, 4, 23.0);
	collidables[0] = temp;*/

	player_hitbox.min = vec3 (cameraPos.v[0] - 0.5f, 0, cameraPos.v[2] - 0.5f);
	player_hitbox.max = vec3(cameraPos.v[0] + 0.5f, 1.5f, cameraPos.v[2] + 0.5f);

	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();

	init_text_rendering("../freemono.png", "../freemono.meta", width, height);
	// x and y are -1 to 1
	// size_px is the maximum glyph size in pixels (try 100.0f)
	// r,g,b,a are red,blue,green,opacity values between 0.0 and 1.0
	// if you want to change the text later you will use the returned integer as a parameter
	crosshair_id = add_text("+", -0.01f, 0.05f, 35.0f, 1.0f, 1.0f, 1.0f, 0.75f);
	score_id = add_text("Zombies Killed: 0", -0.95f, -0.85f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	time_id = add_text("Time until Morning: 45", -0.95f, -0.75f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	player_info_id = add_text("Player Position", -0.95f, 0.85f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	//enemy_info_id = add_text("Enemy Position", -0.95f, -0.75f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	//bullet_info_id = add_text("Bullet Position", -0.95f, -0.65f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	//player_hb_id = add_text("Player HB Position", 0.3f, -0.65f, 35.0f, 1.0f, 1.0f, 1.0f, 1.0f);

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

	glGenTextures(1, &tex_atlas);
	glBindTexture(GL_TEXTURE_2D, tex_atlas); // All upcoming GL_TEXTURE_2D operations now have effect on our texture object
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
	for (int i = 0; i < num_meshes; i++) {
		generateObjectBufferMesh(i);
	}

	add_collidable(0, vec3(-4.0, 0, 17.0), vec3(4.0, 4, 23.0));
	add_collidable(1, vec3(-3, 0, 3), vec3(3, 4, -3.0));

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
	
	GLfloat cameraSpeed = speed;
	vec3 new_pos = cameraPos;
	switch (key)
	{
	case 'w':
		new_pos = cameraPos + (cameraFront * cameraSpeed);
		break;
	case 's':
		new_pos = cameraPos - (cameraFront * cameraSpeed);
		break;
	case 'a':
		new_pos = cameraPos - (normalise(cross(cameraFront, cameraUp)) * cameraSpeed);
		break;
	case 'd':
		new_pos = cameraPos + (normalise(cross(cameraFront, cameraUp)) * cameraSpeed);
		break;
	case 'e':
		spawn_enemy();
		break;
	case 'f':
		freecam_enabled = (freecam_enabled == true) ? false : true;
		break;
	case ' ':
		resetCamera();
		break;
	case 27:
		exit(0);
		break;
	case 'u':
		plane_translation.v[0] -= 1.0f;
		break;
	case 'i':
		plane_translation.v[0] += 1.0f;
		break;

	}
	// Keeps user at ground level
	

	BoundingBox new_hitbox;
	new_hitbox.min = vec3(new_pos.v[0] - 0.5f, 0, new_pos.v[2] - 0.5f);
	new_hitbox.max = vec3(new_pos.v[0] + 0.5f, 1.5f, new_pos.v[2] + 0.5f);

	bool can_move = true;// = player_can_move();
	BoundingBox temp;
	temp.min = vec3(-4.25, 0, 16.5);
	temp.max = vec3(4.1, 4, 23.1);
	/*for (int i = 0; i < NUM_COLLIDABLES; i++) {
		BoundingBox t = collidables[i];
		if (_intersect(new_hitbox, t))
			can_move = false;
	}*/
	if (_intersect(new_hitbox, collidables[1]))
		can_move = false;
	char *result = can_move ? "" : "******COLLISION******\n";
	printf("%s", result);
	if (can_move) {
		cameraPos = new_pos;
		player_hitbox.min = vec3(cameraPos.v[0] - 0.5f, 0, cameraPos.v[2] - 0.5f);
		player_hitbox.max = vec3(cameraPos.v[0] + 0.5f, 1.5f, cameraPos.v[2] + 0.5f);
	}
	else {
		printf("Collision");
	}
	if (!freecam_enabled)
		cameraPos.v[1] = 1.5f;
	positionInfo();
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
		spawn_bullet(); // Will replace the below code
		if (sound_enabled)
			SoundEngine->play2D("../Audio/pew.wav", GL_FALSE);
		fired = true;
		//positionInfo();
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
	glutMotionFunc(mouse);
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
	SoundEngine->drop();
	return 0;
}










