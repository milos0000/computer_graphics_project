#version 330 core
out vec4 FragColor;

in vec2 TextureCoord;

uniform sampler2D Texture1;

void main()
{
    FragColor = texture(Texture1, TextureCoord);
}