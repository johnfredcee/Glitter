
#pragma once

namespace Glitter
{
    template <typename T>
    class Buffer
    {
    public:        
        using ElementType = T;
    private:
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

        void unbind()
        {
            glBindBuffer(GL_ARRAY_BUFFER,0);
        }

        void bind()
        {
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);  
        }

        void init()
        {
            glGenBuffers(1, &buffer);
        }

        void update(uint8_t* inData)
        {
            memcpy(data, inData, bytes);        
            bind();
        }

        GLuint elementCount() const
        {
            return count;            
        }

        ~Buffer() 
        {
            glDeleteBuffers(1, &buffer);       
            free(data);
        }
    
    };
    
}

