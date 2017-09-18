
#version 400
uniform sampler2D diffuseTex;

in vec2 UV;

out vec4 FragColor;

void main() {
    FragColor = texture(diffuseTex, UV);
}

