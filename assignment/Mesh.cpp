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

using namespace std;

class Mesh {
private:
	char* mesh_name;
	std::vector<float> g_vp, g_vn, g_vt;
	GLuint loc1, loc2, loc3;
	unsigned int g_vao;
	int g_point_count = 0;
	int shaderProgramID;
public:
	mat4 local;
	mat4 global;

	Mesh::Mesh(char* name, int shader) {
		this->mesh_name = name;
		this->shaderProgramID = shader;
		this->g_vao = 0;
	}

	bool Mesh::load(const char* file_name) {
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

		g_point_count = 0;

		for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
			const aiMesh* mesh = scene->mMeshes[m_i];
			printf("    %i vertices in mesh\n", mesh->mNumVertices);

			g_point_count += mesh->mNumVertices;
			for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
				if (mesh->HasPositions()) {
					const aiVector3D* vp = &(mesh->mVertices[v_i]);
					//printf ("      vp %i (%f,%f,%f)\n", v_i, vp->x, vp->y, vp->z);
					g_vp.push_back(vp->x);
					g_vp.push_back(vp->y);
					g_vp.push_back(vp->z);
				}
				if (mesh->HasNormals()) {
					const aiVector3D* vn = &(mesh->mNormals[v_i]);
					//printf ("      vn %i (%f,%f,%f)\n", v_i, vn->x, vn->y, vn->z);
					g_vn.push_back(vn->x);
					g_vn.push_back(vn->y);
					g_vn.push_back(vn->z);

				}
				if (mesh->HasTextureCoords(0)) {
					const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
					//printf ("      vt %i (%f,%f)\n", v_i, vt->x, vt->y);
					g_vt.push_back(vt->x);
					g_vt.push_back(vt->y);
				}
				if (mesh->HasTangentsAndBitangents()) {
					// NB: could store/print tangents here
				}
			}
		}

		aiReleaseImport(scene);
		return true;
	}

	void Mesh::generateObjectBufferMesh() {
		/*----------------------------------------------------------------------------
		LOAD MESH HERE AND COPY INTO BUFFERS
		----------------------------------------------------------------------------*/

		//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
		//Might be an idea to do a check for that before generating and binding the buffer.
		load(this->mesh_name);
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
		unsigned int vt_vbo = 0;
		glGenBuffers(1, &vt_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
		glBufferData(GL_ARRAY_BUFFER, g_point_count * 2 * sizeof(float), &g_vt[0], GL_STATIC_DRAW);


		
		glGenVertexArrays(1, &g_vao);
		glBindVertexArray(g_vao);

		glEnableVertexAttribArray(loc1);
		glBindBuffer(GL_ARRAY_BUFFER, g_vp_vbo);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(loc2);
		glBindBuffer(GL_ARRAY_BUFFER, g_vn_vbo);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		//	This is for texture coordinates which you don't currently need, so I have commented it out
		glEnableVertexAttribArray(loc3);
		glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
		glVertexAttribPointer(loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
};