//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
phong basic.vs phong.fs
plain basic.vs plain.fs

\perturbNormal

// From https://github.com/glslify/glsl-perturb-normal/blob/master/cotangent-frame.glsl
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv)
{
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx(p);
	vec3 dp2 = dFdy(p);
	vec2 duv1 = dFdx(uv);
	vec2 duv2 = dFdy(uv);

	// solve the linear system
	vec3 dp2perp = cross(dp2, N);
	vec3 dp1perp = cross(N, dp1);
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame 
	float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
	return mat3(T * invmax, B * invmax, N);
}

// assume N, the interpolated vertex normal and 
// WP the world position
vec3 perturbNormal(vec3 N, vec3 WP, vec2 uv, vec3 normal_pixel)
{
	mat3 TBN = cotangent_frame(N, WP, uv);
	return normalize(TBN * normal_pixel);
}

\computeShadow

float isShadow(mat4 u_shadow_vp, vec3 v_world_position, sampler2D u_shadowmap, float u_shadow_bias) 
{
	vec4 proj_pos = u_shadow_vp * vec4(v_world_position, 1.0);
	proj_pos.z -= u_shadow_bias;
	proj_pos /= proj_pos.w; //normalize [-1,1]
	proj_pos = proj_pos * 0.5 + 0.5;
	float map_depth = texture(u_shadowmap, proj_pos.xy).r;
	float shadow = 1.0;
	if (proj_pos.z > map_depth){
		shadow = 0.0;
	}

	return shadow;
}



\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_pos;

uniform mat4 u_model;
uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;

uniform float u_time;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	
	//store the color in the varying var to use it from the pixel shader
	v_color = a_color;

	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}

\quad.vs

#version 330 core

in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;

void main()
{	
	v_uv = a_coord;
	gl_Position = vec4( a_vertex, 1.0 );
}


\flat.fs

#version 330 core

uniform vec4 u_color;

out vec4 FragColor;

void main()
{
	FragColor = u_color;
}


\texture.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, v_uv );

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = color;
}


\skybox.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture( u_texture, E );
	FragColor = color;
}


\multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, uv );

	if(color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);

	FragColor = color;
	NormalColor = vec4(N,1.0);
}


\depth.fs

#version 330 core

uniform vec2 u_camera_nearfar;
uniform sampler2D u_texture; //depth map
in vec2 v_uv;
out vec4 FragColor;

void main()
{
	float n = u_camera_nearfar.x;
	float f = u_camera_nearfar.y;
	float z = texture(u_texture,v_uv).x;
	if( n == 0.0 && f == 1.0 )
		FragColor = vec4(z);
	else
		FragColor = vec4( n * (z + 1.0) / (f + n - z * (f - n)) );
}


\instanced.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;

in mat4 u_model;

uniform vec3 u_camera_pos;

uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( a_vertex, 1.0) ).xyz;
	
	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}


\phong.fs

#version 330 core
#include "perturbNormal"
#include "computeShadow"

//From basic.vs:
in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
// in vec3 v_tangent;

//From renderMeshWithMaterial:
uniform mat4 u_model; //object's model
uniform mat4 u_viewprojection; //camera's vp 
uniform float u_time;
uniform vec3 u_camera_pos;

const int MAX_LIGHTS = 4;
uniform vec3 u_ambient_light;
uniform int u_num_lights;
uniform float u_shininess;

uniform vec3 u_light_pos[MAX_LIGHTS];
uniform vec3 u_light_color[MAX_LIGHTS];
uniform float u_intensity[MAX_LIGHTS];
uniform int u_light_type[MAX_LIGHTS];
uniform vec3 u_light_front[MAX_LIGHTS];
uniform vec2 u_light_cone[MAX_LIGHTS];

const int MAX_SHADOWS = 2; //only directional and spot lights will cast shadows
// uniform sampler2D u_shadowmap;
// uniform mat4 u_shadow_vp;
uniform float u_shadow_bias;
uniform sampler2D u_shadowmaps[MAX_SHADOWS];
uniform mat4 u_shadow_vps[MAX_SHADOWS];

//From material->bind:
uniform vec4 u_color; //color of the material
uniform sampler2D u_texture; //object's texture
uniform float u_alpha_cutoff; //threshold to decide wheter to paint or not 
uniform sampler2D u_normalmap;

out vec4 FragColor;

void main() 
{

	vec4 color = u_color;
	color *= texture(u_texture, v_uv);
	if (color.a < u_alpha_cutoff) 
		discard;

	vec3 k = color.rgb; //k = k_a = k_s = k_d

//VARIABLES TO BE USED:
	vec3 L_vec, L, N, light_intensity, diffuse_contrib, R, V, specular_contrib, D;
	float d, attenuation, RV, LD, alpha_max, alpha_min, attenuation_distance, attenuation_angle, shadow;
	int s = 0; //to know in which shadowmap we are

//AMBIENT LIGHT:
	vec3 phong = u_ambient_light * k;

//NORMALS WITH NORMALMAPS:
	vec3 nm_color = normalize((texture(u_normalmap, v_uv).xyz * 2.0) - 1.0); //color sampled from the normalmap converted to a range [-1,1]
	N = perturbNormal(normalize(v_normal), v_world_position, v_uv, nm_color);

	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	
	for(int i = 0; i<MAX_LIGHTS; i++) {
		if (i < u_num_lights) {
		
	//POINT LIGHT
			if (u_light_type[i] == 1) {
			//DIFFUSE:
				L_vec = u_light_pos[i] - v_world_position;
				L = normalize(L_vec);
				d = length(L_vec);
				attenuation = u_intensity[i]/pow(d,2);
				light_intensity = attenuation * u_light_color[i];
				diffuse_contrib = clamp(dot(L,N), 0.0, 1.0) * light_intensity;
				diffuse += diffuse_contrib;

			//SPECULAR:
				R = reflect(-L, N);
				V = normalize(u_camera_pos - v_world_position);
				RV = clamp(dot(R,V), 0.0, 1.0);
				specular_contrib = pow(RV, u_shininess) * light_intensity;
				specular += specular_contrib;
			} 
			
	//DIRECTIONAL LIGHT
			else if (u_light_type[i] == 3) {
			//There is no attenuation:
				light_intensity = u_intensity[i] * u_light_color[i];

			//SHADOWS:
				shadow = isShadow(u_shadow_vps[s], v_world_position, u_shadowmaps[s], u_shadow_bias);
				s += 1;
		
			//DIFFUSE
				L = normalize(u_light_front[i]); // -L is the direction of the light 
				diffuse_contrib = clamp(dot(N,L), 0.0, 1.0) * light_intensity;
				diffuse += shadow*diffuse_contrib;
			
			//SPECULAR
				R = reflect(-L,N);
				V = normalize(u_camera_pos - v_world_position);
				RV = clamp(dot(R,V), 0.0, 1.0);
				specular_contrib = pow(RV, u_shininess) * light_intensity;
				specular += shadow*specular_contrib;
			}
	//SPOT LIGHT
			else {
				L_vec = u_light_pos[i] - v_world_position;
				L = normalize(L_vec);
				D = normalize(-u_light_front[i]); //cone center direction
				LD = dot(-L,D);
				alpha_max = cos(u_light_cone[i].y);
				if(LD >= alpha_max){
					alpha_min = cos(u_light_cone[i].x);
					d = length(L_vec); //distance from point to light source
					attenuation_distance = u_intensity[i] / pow(d,2); // attenuation of light by distance between point and light source
					attenuation_angle = clamp((LD - alpha_max) / (alpha_min - alpha_max), 0.0, 1.0);
					attenuation = attenuation_distance * attenuation_angle;
					light_intensity = u_light_color[i] * attenuation;

				//SHADOWS: only if the pixel is illuminated by the spot light
					shadow = isShadow(u_shadow_vps[s], v_world_position, u_shadowmaps[s], u_shadow_bias);
				}
				else{
					light_intensity = vec3(0.0);
					shadow = 0.0;
				}

				s += 1;

			//DIFFUSE
				diffuse_contrib = clamp(dot(L,N), 0.0, 1.0) * light_intensity;
				diffuse += shadow*diffuse_contrib;

			//SPECULAR
				R = reflect(-L,N);
				RV = clamp(dot(R,V), 0.0, 1.0);
				specular_contrib = pow(RV, u_shininess) * light_intensity;
				specular += shadow*specular_contrib;
			}
		}

	}

	phong += k*(diffuse + specular);
	FragColor = vec4(phong, color.a);

}

\plain.fs

#version 330 core

in vec2 v_uv; // to sample the texture

uniform sampler2D u_texture;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	//Alpha testing:
	vec4 color = texture(u_texture, v_uv);
	if(color.a < u_alpha_cutoff)
		discard;
	
	FragColor = vec4(0.0, 0.0, 0.0, 1.0); // since glColorMask->false, any color wil be discarded
}