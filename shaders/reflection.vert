#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 normal_matrix;

smooth out vec3 Normal;
smooth out vec3 Position;
//flat out vec3 CameraPos;

void main(void)  {
  Normal = mat3(transpose(inverse(model))) * normal;
  Position = vec3(model * vec4(vert, 1.0));
  gl_Position = projection * view * vec4(Position, 1.0);
}
