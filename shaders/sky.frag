#version 330

smooth in vec3 world_vertex;

uniform samplerCube specular_map;

out vec4 frag_color;

void main (void) {
 vec3 V = normalize(world_vertex);
 vec3 envColor = texture(specular_map, V).rgb;
 envColor = envColor / (envColor + vec3(1.0));
 envColor = pow(envColor, vec3(1.0/2.2)); 
 frag_color = vec4(envColor, 1.0);
}
