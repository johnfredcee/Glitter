
#pragma once

namespace Glitter
{
    class Texture
    {
    private:
        GLuint   texture;
        uint8_t* data;
        int x, y;
        int c;
    public:
        Texture(int inX, int inY)
        : x(inX), y(inY), c(4)
        {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            data =  (uint8_t*) stbi__malloc(x * y * c);
        }
    
        operator uint32_t*() 
        {
            return (uint32_t*) data;
        }
        
        void update(uint8_t *inData)
        {
            memcpy(data, inData, x*y*c);
            glTexImage2D(GL_TEXTURE_2D, // target
                0,  // level, 0 = base, no minimap,
                GL_RGBA, // internalformat
                x,  // width
                y,  // height
                0,  // border, always 0 in OpenGL ES
                GL_RGBA,  // format
                GL_UNSIGNED_BYTE, // type
                (void*) data);        
        }
    
        void bind(int unit = 0)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            //glUniform1i(uniform_mytexture, /*GL_TEXTURE*/0);
            glBindTexture(GL_TEXTURE_2D, texture);
        }
    
        ~Texture()
        {
            glDeleteTextures(1 ,& texture);        
            stbi_image_free(data);
        }
    };
}
