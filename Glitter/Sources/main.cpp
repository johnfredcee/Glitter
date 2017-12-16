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
		ImportMesh()
		{
		}

		const ofbx::Mesh* fbx = nullptr;
		const ofbx::Material* fbx_mat = nullptr;
		bool import = true;
		bool import_physics = false;
		int lod = 0;
		//OutputBlob vertex_data;
		std::vector<int> indices;
		float radius_squared;
	};

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


	void clearSources()
	{
		for (ofbx::IScene* scene : scenes) scene->destroy();
		scenes.clear();
		meshes.clear();
		materials.clear();
		animations.clear();
		bones.clear();
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

	FILE* fp = fopen("Data\\test_FBX2013_Y.fbx", "rb");
	if (fp)
	{ 
		fseek(fp, 0, SEEK_END);
		long file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		auto* content = new ofbx::u8[file_size];
		fread(content, 1, file_size, fp);
		g_scene = ofbx::load((ofbx::u8*)content, file_size);
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
