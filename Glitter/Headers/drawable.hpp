
#pragma once

namespace Glitter
{
    template <typename T> 
    struct ToGlType
    {
    };

    template<> 
    struct ToGlType<float>
    {
        static const GLenum value = GL_FLOAT;        
    };

    template<>
    struct ToGlType<double>
    {
        static const GLenum value = GL_DOUBLE;        
    };

    class RenderData
    {
    private:
        int   bufferCount;
        int   elementCount;
        GLuint VAO;
    public:
        
        RenderData() : bufferCount(0)
        {
            glGenVertexArrays(1, &VAO);
        }
    
        template< template<typename T> typename Buffer > 
        void addBuffer(Buffer& inBuffer)
        {
            inBuffer.init();
            inBuffer.bind();
            glBindVertexArray(VAO);
            glEnableVertexAttribArray(bufferCount);
            glVertexAttribPointer(bufferCount, Buffer::ElementType::length(), 
                                  ToGlType<Buffer::ElementType::value_type>::value, GL_FALSE, 
                                  sizeof(Buffer::ElementType::value_type) * Buffer::ElementType::length(), 
                                  (GLvoid*) 0);
            elementCount = inBuffer.elementCount();
        }

        void endBuffers()
        {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        void bind()
        {
            glBindVertexArray(VAO);           
        }

        void unbind()
        {
            glBindVertexArray(VAO);           
        }

        void draw(GLenum primitiveType)
        {
            glDrawArrays(primitiveType, 0, elementCount);
        }
    };
}
