#version 330

smooth in vec3 Normal;
smooth in vec3 Position;

uniform samplerCube reflection_map;
uniform vec3 camera_pos;


vec3 color = vec3(0.6);

out vec4 frag_color;

void main (void) {
 vec3 I = normalize(Position - camera_pos);
 vec3 R = reflect(I, normalize(Normal));
 vec3 auxColor = texture(reflection_map, R).rgb;

//Tone mapping
auxColor = auxColor / (auxColor + vec3(1.0));
//Gamma correction
auxColor = pow(auxColor, vec3(1.0/2.2)); 

 frag_color = vec4(auxColor, 1.0);


}
