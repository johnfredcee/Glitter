// Local Headers
#include "glitter.hpp"

// System Headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Standard Headers
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>
//#include <assimp/importer.hpp>
//#include <assimp/postprocess.h>
//#include <assimp/scene.h>
#include "ofbx.h"
#include "renderable.h"

using AABB = CPM_GLM_AABB_NS::AABB;

ofbx::IScene* g_scene = nullptr;

struct FBXImporter
{
	enum class Orientation
	{
		Y_UP,
		Z_UP,
		Z_MINUS_UP,
		X_MINUS_UP,
		X_UP
	};

	struct RotationKey
	{
		glm::quat rot;
		float time;
		uint16_t frame;
	};

	struct TranslationKey
	{
		glm::vec3 pos;
		float time;
		uint16_t frame;
	};

	struct Skin
	{
		float weights[4];
		int16_t joints[4];
		int count = 0;
	};

	struct ImportAnimation
	{
		struct Split
		{
			int from_frame = 0;
			int to_frame = 0;
			std::string name;
		};

		const ofbx::AnimationStack* fbx = nullptr;
		const ofbx::IScene* scene = nullptr;
		std::vector<Split> splits;
	    std::string output_filename;
		bool import = true;
		int root_motion_bone_idx = -1;
	};

	struct ImportTexture
	{
		enum Type
		{
			DIFFUSE,
			NORMAL,
			COUNT
		};

		const ofbx::Texture* fbx = nullptr;
		bool import = true;
		bool to_dds = true;
		bool is_valid = false;
		std::string path;
		std::string src;
	};

	struct ImportMaterial
	{
		const ofbx::Material* fbx = nullptr;
		bool import = true;
		bool alpha_cutout = false;
		ImportTexture textures[ImportTexture::COUNT];
		char shader[20];
	};

	
	struct ImportMesh
	{
		struct vertex 
		{
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec3 tangent;
			glm::vec4 color;
			glm::vec2 uv;
		};

		ImportMesh()
		{
		}

		const ofbx::Mesh* fbx = nullptr;
		const ofbx::Material* fbx_mat = nullptr;
		bool import = true;
		bool import_physics = false;
		int lod = 0;
		std::vector<vertex> vertices;
		std::vector<int> indices;
		float radius_squared;
		AABB aabb;
	};

	using vertex = ImportMesh::vertex;

    const ofbx::Mesh* getAnyMeshFromBone(const ofbx::Object* node) const
	{
		for (int i = 0; i < meshes.size(); ++i)
		{
			const ofbx::Mesh* mesh = meshes[i].fbx;

			auto* skin = mesh->getGeometry()->getSkin();
			if (!skin) continue;

			for (int j = 0, c = skin->getClusterCount(); j < c; ++j)
			{
				if (skin->getCluster(j)->getLink() == node) return mesh;
			}
		}
		return nullptr;
	}

    static ofbx::Matrix makeOFBXIdentity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }


	static ofbx::Matrix getBindPoseMatrix(const ofbx::Mesh* mesh, const ofbx::Object* node)
	{
		if (!mesh) return makeOFBXIdentity();

		auto* skin = mesh->getGeometry()->getSkin();

		for (int i = 0, c = skin->getClusterCount(); i < c; ++i)
		{
			const ofbx::Cluster* cluster = skin->getCluster(i);
			if (cluster->getLink() == node)
			{
				return cluster->getTransformLinkMatrix();
			}
		}
		assert(false);
		return makeOFBXIdentity();
	}

	static glm::vec3 getTranslation(const ofbx::Matrix& mtx)
	{
		return glm::vec3((float)mtx.m[12], (float)mtx.m[13], (float)mtx.m[14]);
	}


	static glm::quat getRotation(const ofbx::Matrix& mtx)
	{
		glm::mat4x4 m = glm::make_mat4x4(mtx.m);
		glm::quat q = glm::quat_cast(m);
		return q;
	}

	static float getScaleX(const ofbx::Matrix& mtx)
	{
		glm::vec3 v(float(mtx.m[0]), float(mtx.m[4]), float(mtx.m[8]));

		return v.length();
	}

	static int getDepth(const ofbx::Object* bone)
	{
		int depth = 0;
		while (bone)
		{
			++depth;
			bone = bone->getParent();
		}
		return depth;
	}

	bool isSkinned(const ofbx::Mesh& mesh) const { return !ignore_skeleton && mesh.getGeometry()->getSkin() != nullptr; }

    glm::vec3 fixRootOrientation(const glm::vec3& v) const
	{
		switch (root_orientation)
		{
			case Orientation::Y_UP: return glm::vec3(v.x, v.y, v.z);
			case Orientation::Z_UP: return glm::vec3(v.x, v.z, -v.y);
			case Orientation::Z_MINUS_UP: return glm::vec3(v.x, -v.z, v.y);
			case Orientation::X_MINUS_UP: return glm::vec3(v.y, -v.x, v.z);
			case Orientation::X_UP: return glm::vec3(-v.y, v.x, v.z);
		}
		assert(false);
		return glm::vec3(v.x, v.y, v.z);
	}


	glm::quat fixRootOrientation(const glm::quat& v) const
	{
		switch (root_orientation)
		{
			case Orientation::Y_UP: return glm::quat(v.x, v.y, v.z, v.w);
			case Orientation::Z_UP: return glm::quat(v.x, v.z, -v.y, v.w);
			case Orientation::Z_MINUS_UP: return glm::quat(v.x, -v.z, v.y, v.w);
			case Orientation::X_MINUS_UP: return glm::quat(v.y, -v.x, v.z, v.w);
			case Orientation::X_UP: return glm::quat(-v.y, v.x, v.z, v.w);
		}
		assert(false);
		return glm::quat(v.x, v.y, v.z, v.w);
	}


	glm::vec3 fixOrientation(const glm::vec3& v) const
	{
		switch (orientation)
		{
			case Orientation::Y_UP: return glm::vec3(v.x, v.y, v.z);
			case Orientation::Z_UP: return glm::vec3(v.x, v.z, -v.y);
			case Orientation::Z_MINUS_UP: return glm::vec3(v.x, -v.z, v.y);
			case Orientation::X_MINUS_UP: return glm::vec3(v.y, -v.x, v.z);
			case Orientation::X_UP: return glm::vec3(-v.y, v.x, v.z);
		}
		assert(false);
		return glm::vec3(v.x, v.y, v.z);
	}


	glm::quat fixOrientation(const glm::quat& v) const
	{
		switch (orientation)
		{
			case Orientation::Y_UP: return glm::quat(v.x, v.y, v.z, v.w);
			case Orientation::Z_UP: return glm::quat(v.x, v.z, -v.y, v.w);
			case Orientation::Z_MINUS_UP: return glm::quat(v.x, -v.z, v.y, v.w);
			case Orientation::X_MINUS_UP: return glm::quat(v.y, -v.x, v.z, v.w);
			case Orientation::X_UP: return glm::quat(-v.y, v.x, v.z, v.w);
		}
		assert(false);
		return glm::quat(v.x, v.y, v.z, v.w);
	}


	static int getMaterialIndex(const ofbx::Mesh& mesh, const ofbx::Material& material)
	{
		for (int i = 0, c = mesh.getMaterialCount(); i < c; ++i)
		{
			if (mesh.getMaterial(i) == &material) return i;
		}
		return -1;
	}

	void clearSources()
	{
		for (ofbx::IScene* scene : scenes) scene->destroy();
		scenes.clear();
		meshes.clear();
		materials.clear();
		animations.clear();
		bones.clear();
	}

	void postprocessMeshes()
	{
		for (int mesh_idx = 0; mesh_idx < meshes.size(); ++mesh_idx)
		{
			ImportMesh& import_mesh = meshes[mesh_idx];
			import_mesh.vertices.clear();
			import_mesh.indices.clear();

			const ofbx::Mesh& mesh = *import_mesh.fbx;
			const ofbx::Geometry* geom = import_mesh.fbx->getGeometry();
			int vertex_count = geom->getVertexCount();
			const ofbx::Vec3* vertices = geom->getVertices();
			const ofbx::Vec3* normals = geom->getNormals();
			const ofbx::Vec3* tangents = geom->getTangents();
			const ofbx::Vec4* colors = import_vertex_colors ? geom->getColors() : nullptr;
			const ofbx::Vec2* uvs = geom->getUVs();

			glm::mat4 transform_matrix = glm::mat4x4(); 
			glm::mat4 geometry_matrix = glm::make_mat4x4(mesh.getGeometricMatrix().m);
			glm::mat4 global_transform = glm::make_mat4x4(mesh.getGlobalTransform().m);
			transform_matrix = global_transform * geometry_matrix;
			if (center_mesh) transform_matrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

			// IAllocator& allocator = app.getWorldEditor().getAllocator();
			// OutputBlob blob(allocator);
			// int vertex_size = getVertexSize(mesh);
			// import_mesh.vertex_data.reserve(vertex_count * vertex_size);
		
			// work out skinning later
			// Array<Skin> skinning(allocator);
			// Skin skinning;
			// bool is_skinned = isSkinned(mesh);
			// if (is_skinned) fillSkinInfo(skinning, &mesh);

			AABB aabb; // = {{0, 0, 0}, {0, 0, 0}};
			float radius_squared = 0;

			int material_idx = getMaterialIndex(mesh, *import_mesh.fbx_mat);
			assert(material_idx >= 0);

			// int first_subblob[256];
			// for (int& subblob : first_subblob) subblob = -1;
			// std::vector<int> subblobs;
			// subblobs.reserve(vertex_count);

			const int* materials = geom->getMaterials();
			for (int i = 0; i < vertex_count; ++i)
			{
				if (materials && materials[i / 3] != material_idx) continue;

				vertex v;
				ofbx::Vec3 cp = vertices[i];
				// premultiply control points here, so we can have constantly-scaled meshes without scale in bones
				glm::vec3 pos = glm::vec3(transform_matrix * glm::vec4(cp.x * mesh_scale, cp.y * mesh_scale, cp.y * mesh_scale, 1.0f));
				v.pos = fixOrientation(pos);
				
				float sq_len = glm::length2(pos);
				radius_squared = glm::min(radius_squared, sq_len);

				aabb.mMin.x = glm::min(aabb.mMin.x, pos.x);
				aabb.mMin.y = glm::min(aabb.mMin.y, pos.y);
				aabb.mMin.z = glm::min(aabb.mMin.z, pos.z);
				aabb.mMax.x = glm::max(aabb.mMax.x, pos.x);
				aabb.mMax.y = glm::max(aabb.mMax.y, pos.y);
				aabb.mMax.z = glm::max(aabb.mMax.z, pos.z);


				if (normals)
				{
					glm::vec3 normal = glm::vec3(transform_matrix * glm::vec4(normals[i].x, normals[i].y, normals[i].z, 0.0f));
					normal = glm::normalize(normal);
					v.normal = fixOrientation(normal);
				} 

				if (uvs)
				{
					v.uv = glm::vec2(uvs[i].x, uvs[i].y);
				}

				if (colors)
				{
					v.color = glm::vec4(colors[i].x, colors[i].y, colors[i].z, colors[i].w);
				}

				if (tangents)
				{
					glm::vec3 tangent = glm::vec3(transform_matrix * glm::vec4(tangents[i].x, tangents[i].y, tangents[i].z, 0.0f));
					tangent = glm::normalize(tangent);
					v.tangent = fixOrientation(tangent);		
				}
				
				// worry about skinning later
				//if (is_skinned) writeSkin(skinning[i], &blob);
				import_mesh.vertices.push_back(v);
				import_mesh.indices.push_back(i);
			} // for each vertex
			import_mesh.aabb = aabb;
			import_mesh.radius_squared = radius_squared;
		}
		// for (int mesh_idx = meshes.size() - 1; mesh_idx >= 0; --mesh_idx)
		// {
		// 	if (meshes[mesh_idx].indices.empty()) meshes.eraseFast(mesh_idx);
		// }
	}


	void gatherMeshes(ofbx::IScene* scene)
	{
		int min_lod = 2;
		int c = scene->getMeshCount();
		int start_index = meshes.size();
		for (int i = 0; i < c; ++i)
		{
			const ofbx::Mesh* fbx_mesh = (const ofbx::Mesh*)scene->getMesh(i);
			if (fbx_mesh->getGeometry()->getVertexCount() == 0) continue;
			for (int j = 0; j < fbx_mesh->getMaterialCount(); ++j)
			{
				ImportMesh mesh;;
				mesh.fbx = fbx_mesh;
				mesh.fbx_mat = fbx_mesh->getMaterial(j);
				//esh.lod = detectMeshLOD(mesh);
				//min_lod = min(min_lod, mesh.lod);
				meshes.push_back(mesh);
			}
		}
		if (min_lod != 1) return;
		for (int i = start_index, n = meshes.size(); i < n; ++i)
		{
			--meshes[i].lod;
		}
	}

	std::vector<ImportMaterial> materials;
	std::vector<ImportMesh> meshes;
	std::vector<ImportAnimation> animations;
	std::vector<const ofbx::Object*> bones;
	std::vector<ofbx::IScene*> scenes;
	float lods_distances[4] = {-10, -100, -1000, -10000};
    float mesh_scale = 1.0f;
	float time_scale = 1.0f;
	float position_error = 0.1f;
	float rotation_error = 0.01f;
	float bounding_shape_scale = 1.0f;
	bool to_dds = false;
	bool center_mesh = false;
	bool ignore_skeleton = false;
	bool import_vertex_colors = true;
	bool make_convex = false;
	bool create_billboard_lod = false;
	Orientation orientation = Orientation::Y_UP;
	Orientation root_orientation = Orientation::Y_UP;
	

};


int main(int argc, char * argv[]) {

    // Load GLFW and Create a Window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    auto mWindow = glfwCreateWindow(mWidth, mHeight, "OpenGL", nullptr, nullptr);

    // Check for Valid Context
    if (mWindow == nullptr) {
        fprintf(stderr, "Failed to Create OpenGL Context");
        return EXIT_FAILURE;
    }

    // Create Context and Load OpenGL Functions
    glfwMakeContextCurrent(mWindow);
    gladLoadGL();
    //Assimp::Importer assimpLoader;
    //aiScene const * scene = assimpLoader.ReadFile("Data\\test_FBX2013_Y.fbx",
    //                                        aiProcessPreset_TargetRealtime_MaxQuality |
    //                                        aiProcess_OptimizeGraph                   |
    //                                        aiProcess_FlipUVs);

    //if (!scene)
    //{
    //    fprintf(stderr, "%s\n", assimpLoader.GetErrorString());
    //} 

	FILE* fp = fopen("Data\\Fbx\\test_FBX2013_Y.fbx", "rb");
	if (fp)
	{ 
		fseek(fp, 0, SEEK_END);
		long file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		auto* content = new ofbx::u8[file_size];
		fread(content, 1, file_size, fp);
		g_scene = ofbx::load((ofbx::u8*)content, file_size);
		FBXImporter importer;
		importer.gatherMeshes(g_scene);
		importer.postprocessMeshes();
		delete[] content;
		fclose(fp);
	}

    // Rendering Loop
    while (glfwWindowShouldClose(mWindow) == false) {
        if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(mWindow, true);

        // Background Fill Color
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Flip Buffers and Draw
        glfwSwapBuffers(mWindow);
        glfwPollEvents();
    }   glfwTerminate();
    return EXIT_SUCCESS;
}
