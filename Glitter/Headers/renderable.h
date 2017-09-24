

#pragma once

#include <cassert>
#include <memory>
#include <array>
#include <vector>
#include <unordered_map>
#include <initializer_list>

/*
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_DOUBLE                         0x140A
*/


std::size_t glTypeSize(GLenum glType)
{
    std::array<std::size_t, 8> sizes = { sizeof(int8_t), sizeof(uint8_t),
                                         sizeof(int16_t), sizeof(uint16_t),
                                         sizeof(int32_t), sizeof(uint32_t),
                                        sizeof(float), sizeof(double)  };

    int index = glType - GL_BYTE;
    assert(index >= 0);
    assert(index < sizes.size());
    return sizes[index];
};


/*
 * Encapsulates a data buffer of a given OpenGL element, with a number of components of a given type
 */
class RenderAttributeData
{
public:
    GLenum      type;
    int         components;
    std::size_t count;
    std::size_t size() const { return glTypeSize(type) * components * count; };
private:   
    uint8_t* data;
public:
    RenderAttributeData(GLenum inType, int inComponents, std::size_t inCount) 
    : type(inType), components(inComponents), count(inCount)
    {
      data = new uint8_t[size()];
    }

    ~RenderAttributeData()
    {
        delete [] data;
    }
    void* get() const { return (void*) data; }
   
};


using RenderData = std::unordered_map<std::string, RenderAttributeData>;
using RenderDataInitInfo = RenderData::value_type;

/**
 * Encapsulates a collection of buffers that add up to something we can draw
 */
class Renderable
{
private:
    RenderData          data;
    GLuint              arrayObject;
    std::vector<GLuint> bufferObjects;
public:
    Renderable(std::initializer_list<RenderDataInitInfo> initlist)
    {
        GLuint bufferHandle;
        int index;
        glGenVertexArrays(1 , &arrayObject );
        glBindVertexArray(arrayObject);
        for( auto&& kv : initlist )
        {
            data.emplace(kv);
            glGenBuffers(1 , &bufferHandle);
            glBindBuffer( GL_ARRAY_BUFFER , bufferHandle );
            const RenderAttributeData& buffer = kv.second;
            glBufferData(GL_ARRAY_BUFFER, buffer.size(), buffer.get(), GL_STATIC_DRAW);
            bufferObjects.push_back(bufferHandle);
            glVertexAttribPointer(index, buffer.components, buffer.type, GL_FALSE, 0, 0);
            index++;
            glEnableVertexAttribArray(index);
        }
    }
    ~Renderable()
    {
        glDeleteVertexArrays(1, &arrayObject);
        for( auto buffer : bufferObjects )
        {
            glDeleteBuffers(1, &buffer);
        }
    }
};
