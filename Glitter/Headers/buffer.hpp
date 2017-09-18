
#pragma once

namespace Glitter
{
    template <typename T>
    class Buffer
    {
        using ElementType = T;
        GLsizei   count;
        std::uint8_t  *data;
        GLsizei   bytes;
        GLsizei   index;
        GLuint    buffer;
    public:
        Buffer(GLsizei inCount) : 
        count(inCount), bytes(inCount * ElementType::length() * sizeof(ElementType::value_type)), index(0)
        {        
            data = (std::uint8_t*) malloc(bytes);
            glGenBuffers(1, &buffer);
        }
    
        bool full() const
        {
            return index == count;
        }
    
        void reset()
        {
            index = 0;
        }
    
        GLsizei addElement(const ElementType& element)
        {
            assert(index < count);
            ((ElementType*)data)[index] = element;
            index++;
            return index;
        }

        void bind()
        {
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);  
        }

        void update(uint8_t* inData)
        {
            memcpy(data, inData, bytes);        
            bind();
        }

        ~Buffer() 
        {
            glDeleteBuffers(1, &buffer);       
            free(data);
        }
    
    };
    
}

