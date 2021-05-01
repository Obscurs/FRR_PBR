#version 330

layout (location = 0) in vec3 vert;
layout (location = 1) in vec3 normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 normal_matrix;

smooth out vec3 Normal;
smooth out vec3 Position;
//smooth out vec3 CameraPos;

void main(void)  {
  Normal = mat3(model) * normal;
  Position = vec3(model * vec4(vert, 1.0));
  //mat4 inverseView = inverse(view);
  //CameraPos = vec3(inverseView[3][0],inverseView[3][1],inverseView[3][2]);
  //CameraPos = -(view * model * vec4(vert, 1)).xyz;
  gl_Position = projection * view * vec4(Position, 1.0);
}
