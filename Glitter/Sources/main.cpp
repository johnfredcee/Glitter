// Local Headers
#include "glitter.hpp"

// System Headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Standard Headers
#include <cstdio>
#include <cstdlib>
//#include <map>
//#include <memory>
//#include <vector>
//#include <assimp/importer.hpp>
//#include <assimp/postprocess.h>
//#include <assimp/scene.h>
#include "ofbx.h"
#include "renderable.h"

ofbx::IScene* g_scene = nullptr;

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
