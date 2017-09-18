
#version 400
layout(location = 0) in vec3  VertexPosition;
layout(location = 1) in vec2  VertexUV;

out vec2 UV;

void main()
{
    UV = VertexUV;
    gl_Position = vec4( VertexPosition, 1.0 );
}
