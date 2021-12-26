#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 0) in vec2 aTextureCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TextureCoord;

void main()
{
    TextureCoord = aTextureCoord;
    gl_Position = projection * view * model * vec4(aPos, 1.0f);
}