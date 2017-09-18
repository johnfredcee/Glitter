
// System Headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Standard Headers
#include <cstdio>
#include <cstdlib>

// Project headers
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <picojson.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp> 

// Local Headers
#include "glitter.hpp"
#include "texture.hpp"
#include "buffer.hpp"
#include "shader.hpp"


void* load_font(char* fontfile, stbtt_fontinfo *font)
{
    FILE* fontf = fopen(fontfile, "rb");
    if (fontf == nullptr)
        return nullptr;
    fseek(fontf, 0, SEEK_END);
    unsigned long fontfsize = ftell(fontf);
    fseek(fontf, 0, SEEK_SET);
    void *fontdata = malloc(fontfsize);
    fread(fontdata, 1, fontfsize, fontf);
    fclose(fontf);
    int offset = stbtt_GetFontOffsetForIndex((const unsigned char*) fontdata, 0);
    stbtt_InitFont(font, (const unsigned char*) fontdata, offset);
    return fontdata;
}

char* load_text(char* textfile)
{
    FILE* textf = fopen(textfile, "rb");
    if (textf == nullptr)
        return nullptr;
    fseek(textf, 0, SEEK_END);
    unsigned long textfsize = ftell(textf);
    void *textdata = malloc(textfsize);
    fseek(textf, 0, SEEK_SET);
    fread(textdata, 1, textfsize, textf);
    fclose(textf);
    return (char*) textdata;
}

int main(int argc, char * argv[]) {

    stbtt_fontinfo working_font;
    glm::mat4 proj = glm::ortho(0.0f, 640.0f, 0.0f, 480.0f, -1.0f, -1.0f);    
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
    fprintf(stderr, "OpenGL %s\n", glGetString(GL_VERSION));

    // load a font
    void *font_data = load_font("Glitter\\fonts\\SourceCodePro-Regular.ttf", &working_font);
    if (font_data == nullptr) {
        fprintf(stderr, "Failed to load font");
        return EXIT_FAILURE;
    }
    // load text
    char* text = load_text("Glitter\\text\\test.txt");
    if (text == nullptr) {
        fprintf(stderr, "Failed to load text");
        return EXIT_FAILURE;
    }
    float scale = stbtt_ScaleForPixelHeight(&working_font, 15);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&working_font, &ascent, &descent, &line_gap);
    ascent *= scale;
    descent *= scale;
    int baseline = (int) roundf(ascent);
    int height = (int) roundf(ascent - descent);
    int width = 0;

    int ch = 0;
    while (text[ch])
    {
        // TO DO -- probably don't want to use codepoints; want to use glyph 
        /* how wide is this character */
        int ax;
        stbtt_GetCodepointHMetrics(&working_font, text[ch], &ax, 0);
        width += ax * scale;        

        /* add kerning */
        int kern;
        kern = stbtt_GetCodepointKernAdvance(&working_font, text[ch], text[ch + 1]);
        width += kern * scale;        
        ch++;
    }
    stbtt__bitmap gbm;
    gbm.w = width;
    gbm.h = height;
    gbm.pixels = (unsigned char*) malloc(gbm.w * gbm.h);

    ch = 0;
    int x = 0;
    /* work out height and width of texture to render to */
    while (text[ch]) {
        /* get bounding box for character (may be offset to account for chars that dip above or below the line */
        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(&working_font, text[ch], scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

        /* compute y (different characters have different heights */
        int y = ascent + c_y1;
        
        /* render character (stride and offset is important here) */
        int byteOffset = x + (y  * width);        
        stbtt_MakeCodepointBitmap(&working_font, gbm.pixels + byteOffset, c_x2 - c_x1, c_y2 - c_y1, width, scale, scale, text[ch]);        

        /* how wide is this character */
        int ax;
        stbtt_GetCodepointHMetrics(&working_font, text[ch], &ax, 0);
        x += ax * scale;        

        /* add kerning */
        int kern;
        kern = stbtt_GetCodepointKernAdvance(&working_font, text[ch], text[ch + 1]);
        x += kern * scale;

        ch++;
    }

    Glitter::Texture tex(width, height);
    x = 0;
    int y = 0;
    uint32_t* texData(tex);
    uint32_t  background = 0x0000FF00;
    uint32_t  foreground = 0xFF222200;
    
    for( y = 0; y < height; y++)
    {
        for( x = 0; x < width; x++)
        {
            uint8_t *pixptr = gbm.pixels + x + (y * width);
            uint8_t pix = *pixptr;
            int alpha = (float) (pix) * (1.0f / 255.0f);
            int red = 1.0f * alpha;
            int green = 0.2f * alpha;
            int blue = 1.0f - (1.0f - 0.2f) * alpha; 
             uint32_t colorval = 0;
             uint8_t colorbyte = (uint8_t) (red * 255.0f);
             colorval = red << 24;
             colorbyte = (uint8_t) (green * 255.0f);
             colorval |= green << 16;
             colorbyte = (uint8_t) (blue * 255.0f);
             colorval |= blue << 16;
             *(texData + x + (y * width)) = colorval;
        }   
    }

    Glitter::Buffer<glm::vec3> quad(4);
    quad.addElement(glm::vec3{ -1.0f, -1.0f, 0.0f});
    quad.addElement(glm::vec3{ -1.0f,  1.0f, 0.0f});
    quad.addElement(glm::vec3{  1.0f,  1.0f, 0.0f});
    quad.addElement(glm::vec3{  1.0f, -1.0f, 0.0f});
    quad.bind();

    Glitter::Buffer<glm::vec2> uv(4);
    uv.addElement(glm::vec2{ 0.0f, 0.0f});
    uv.addElement(glm::vec2{ 0.0f, 1.0f});
    uv.addElement(glm::vec2{ 1.0f, 1.0f});
    uv.addElement(glm::vec2{ 1.0f, 0.0f});
    uv.bind();

    glViewport(0,0, 640, 480);

    Glitter::Shader boring;
    boring.attach("boring.vert");
    boring.attach("boring.frag");
    boring.link();

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
