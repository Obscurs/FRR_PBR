#version 330

smooth in vec3 Normal;
smooth in vec3 Position;
//flat in vec3 cameraPos;

uniform samplerCube irradiance_map;
uniform samplerCube prefilter_map;
uniform sampler2D brdfLUT;
uniform float metalness;
uniform float roughness;
uniform vec3 camera_pos;

//float roughness = 0.2;
//float metallic = 0.9;
vec3 albedo = vec3(0.5,0.0,0.5);

out vec4 frag_color;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}  
void main (void) {
 vec3 N = normalize(Normal);
 vec3 V = normalize(camera_pos - Position);
 vec3 R = reflect(-V, N);



 vec3 F0 = vec3(0.04); 
 F0 = mix(F0, albedo, metalness);


 vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
 vec3 kS = F;
 vec3 kD = 1.0 - kS;
 kD *= (1.0 - metalness);	  
 vec3 irradiance = texture(irradiance_map, N).rgb;
 vec3 diffuse      = irradiance * albedo;
 

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilter_map, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular);

    
 vec3 color = ambient;

 // HDR tonemapping
 color = color / (color + vec3(1.0));
 // gamma correct
 color = pow(color, vec3(1.0/2.2)); 

 frag_color = vec4(color, 1.0);
 //frag_color = vec4(Normal*10, 1.0);

}
