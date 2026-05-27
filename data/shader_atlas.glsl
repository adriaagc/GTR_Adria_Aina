//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
phong basic.vs phong.fs
plain basic.vs plain.fs
fill_gbuffer basic.vs fill_gbuffer.fs
deferred_phong quad.vs deferred_phong.fs
ambient_render quad.vs ambient_render.fs
volume_render basic.vs volume_render.fs
sphere_render basic.vs sphere_render.fs
deferred_cook quad.vs deferred_cook.fs
phong_cook basic.vs phong_cook.fs
ambient_occlusion quad.vs ambient_occlusion.fs
blur quad.vs blur.fs
tonemapper quad.vs tonemapper.fs 
tonemapperND quad.vs tonemapperND.fs
render_screen quad.vs render_screen.fs
camera_motion_blur quad.vs camera_motion_blur.fs
object_motion_blur quad.vs object_motion_blur.fs
fill_vbuffer quad.vs fill_vbuffer.fs
fill_vbuffer2 basic.vs fill_vbuffer2.fs

\gamma_functions

//From sRGB to linear:
vec3 degamma(vec3 c){
	return pow(c,vec3(2.2));
}

//From linear to sRGB:
vec3 gamma(vec3 c){
	return pow(c,vec3(1.0/2.2));
}

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

\lighting_functions

struct diffuseSpecular {
    vec3 d; //diffuse term
    vec3 s; //specular term
};

diffuseSpecular point_light_reflection(vec3 light_pos, vec3 world_pos, float intensity, vec3 light_color, vec3 N, vec3 camera_pos, float shininess) 
{
	diffuseSpecular res; 
//DIFFUSE:
	vec3 L_vec = light_pos - world_pos;
	vec3 L = normalize(L_vec);
	float dist = length(L_vec);
	float attenuation = intensity/pow(dist,2);
	vec3 light_intensity = attenuation * light_color;
	vec3 diffuse_contrib = clamp(dot(L,N), 0.0, 1.0) * light_intensity;
	res.d = diffuse_contrib;

//SPECULAR:
	vec3 R = reflect(-L, N);
	vec3 V = normalize(camera_pos - world_pos);
	float RV = clamp(dot(R,V), 0.0, 1.0);
	vec3 specular_contrib = pow(RV, shininess) * light_intensity;
	res.s = specular_contrib;

	return res;
}

diffuseSpecular directional_light_reflection(float shadow, float intensity, float shininess, vec3 light_color, vec3 light_front, vec3 N, vec3 camera_pos, vec3 world_pos)
{
	diffuseSpecular res;

//There is no attenuation:
	vec3 light_intensity = intensity * light_color;

//DIFFUSE
	vec3 L = normalize(light_front); // -L is the direction of the light 
	vec3 diffuse_contrib = clamp(dot(N,L), 0.0, 1.0) * light_intensity;
	res.d = shadow*diffuse_contrib;

//SPECULAR
	vec3 R = reflect(-L,N);
	vec3 V = normalize(camera_pos - world_pos);
	float RV = clamp(dot(R,V), 0.0, 1.0);
	vec3 specular_contrib = pow(RV, shininess) * light_intensity;
	res.s = shadow*specular_contrib;

	return res;
}

diffuseSpecular spot_light_reflection(vec3 light_pos, vec3 world_pos, vec3 light_front, vec2 light_cone, float intensity, vec3 light_color, float shadow, vec3 N, float shininess, vec3 camera_pos)
{
	diffuseSpecular res;
	vec3 light_intensity;

	vec3 L_vec = light_pos - world_pos;
	vec3 L = normalize(L_vec);
	vec3 D = normalize(-light_front); //cone center direction
	float LD = dot(-L,D);
	float alpha_max = cos(light_cone.y);
	if(LD >= alpha_max){
		float alpha_min = cos(light_cone.x);
		float dist = length(L_vec); //distance from point to light source
		float attenuation_distance = intensity / pow(dist,2); // attenuation of light by distance between point and light source
		float attenuation_angle = clamp((LD - alpha_max) / (alpha_min - alpha_max), 0.0, 1.0);
		float attenuation = attenuation_distance * attenuation_angle;
		light_intensity = light_color * attenuation;
	}
	else{
		light_intensity = vec3(0.0);
		shadow = 0.0;
	}
//DIFFUSE:
	res.d = clamp(dot(L,N), 0.0, 1.0) * light_intensity * shadow;
//SPECULAR:
	vec3 R = reflect(-L,N);
	vec3 V =  normalize(camera_pos - world_pos);
	float RV = clamp(dot(R,V), 0.0, 1.0);
	res.s = pow(RV, shininess) * light_intensity * shadow;

	return res;
}

\PBR_functions

#define PI 3.14159265359
#define eps 0.001

vec3 fresnelReflection(vec3 L, vec3 V, vec3 albedo, float metalness)
{
	vec3 F0 = mix(vec3(0.04), albedo, metalness);
	vec3 H = normalize(L + V);
	vec3 F = F0 + (1.0-F0) * pow(1.0 - dot(H, V), 5.0);
	return F;
}

float normalDistribution(float roughness, vec3 N, vec3 L, vec3 V)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	vec3 H = normalize(L + V);
	float NH = clamp(dot(N, H), 0.0, 1.0);
	float NH2 = NH * NH;

	float denom = NH2 * (alpha2 - 1.0) + 1.0;
	return alpha2 / max((PI * denom * denom), eps); //avoid dividing by 0
}

float geometryTerm(vec3 N, vec3 V, vec3 L, float roughness)
{
	float alpha = roughness * roughness;
	float k = alpha / 2.0;
	
	float NV = clamp(dot(N, V), 0.0, 1.0);
	float den = max(NV * (1.0 - k) + k, eps);
	float gv = NV / den;

	float NL = clamp(dot(N, L), 0.0, 1.0);
	float den2 = max(NL * (1.0 - k) + k, eps);
	float gl = NL / den2;

	return gv * gl; //avoid dividing by 0
}

vec3 cookTorrance(vec3 L, vec3 V, vec3 albedo, float metalness, float roughness, vec3 N)
{
	vec3 fr = fresnelReflection(L, V, albedo, metalness);
	float nd = normalDistribution(roughness, N, L, V);
	float g = geometryTerm(N, V, L, roughness);
	vec3 num = fr * nd * g;

	float NL = max(dot(N,L),0.0);
	float NV = max(dot(N,V),0.0);
	float den = max(4.0 * NL * NV, eps);

	vec3 spec = num / den;
	vec3 diff = albedo / PI;

	return diff + spec;
} 

\linear_depth

float depthToLinear(float z, float near, float far)
{
	return near * (z + 1.0) / (far + near - z * (far - near));
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
uniform mat4 u_prev_viewprojection;
uniform mat4 u_prev_model;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;
out vec4 v_current_position;
out vec4 v_prev_position;

uniform float u_time;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	vec3 v_prev_world_position = (u_prev_model * vec4( v_position, 1.0) ).xyz;
	
	//store the color in the varying var to use it from the pixel shader
	v_color = a_color;

	//store the texture coordinates
	v_uv = a_coord;

	//calcule the vertex in objet space at current and previous frames
	v_prev_position = u_prev_viewprojection * vec4(v_prev_world_position, 1.0);
	v_current_position = u_viewprojection * vec4(v_world_position, 1.0);

	//calcule the position of the vertex using the matrices
	gl_Position = v_current_position;
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



//-------------------------------------------------------------------------//
//-------------------------------------------------------------------------//

\phong.fs

#version 330 core
#include "perturbNormal"
#include "computeShadow"
#include "lighting_functions"
#include "gamma_functions"

//From basic.vs:
in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
// in vec3 v_tangent;

//From renderMeshWithMaterial:
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

	vec4 color = vec4(degamma(u_color.xyz), u_color.a);
	vec4 albedo_color = texture(u_texture, v_uv);
	color *= vec4(degamma(albedo_color.xyz), albedo_color.a); //color multiplied by texture color (with alpha cutoff already applied)
	if (color.a < u_alpha_cutoff) 
		discard;

	vec3 k = color.rgb; //k = k_a = k_s = k_d;

//VARIABLES TO BE USED:
	int s = 0; //to know in which shadowmap we are
	float shadow;

//AMBIENT LIGHT:
	vec3 phong = degamma(u_ambient_light) * k;

//NORMALS WITH NORMALMAPS:
	vec3 nm_color = normalize((texture(u_normalmap, v_uv).xyz * 2.0) - 1.0); //color sampled from the normalmap converted to a range [-1,1]
	vec3 N = perturbNormal(normalize(v_normal), v_world_position, v_uv, nm_color);

	diffuseSpecular res;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	// vec3 light_color = vec3(0.0);

	for(int i = 0; i<MAX_LIGHTS; i++) {
		if (i < u_num_lights) {
			// light_color = degamma(u_light_color[i]);
	//POINT LIGHT
			if (u_light_type[i] == 1) {
				res = point_light_reflection(u_light_pos[i], v_world_position, u_intensity[i], degamma(u_light_color[i]), N, u_camera_pos, u_shininess);
			} 
	//DIRECTIONAL LIGHT
			else if (u_light_type[i] == 3) {
			//SHADOWS:
				shadow = isShadow(u_shadow_vps[s], v_world_position, u_shadowmaps[s], u_shadow_bias);
				s += 1;

				res = directional_light_reflection(shadow, u_intensity[i], u_shininess,  degamma(u_light_color[i]), u_light_front[i], N, u_camera_pos, v_world_position);
			}
	//SPOT LIGHT
			else {
			//SHADOWS:
				shadow = isShadow(u_shadow_vps[s], v_world_position, u_shadowmaps[s], u_shadow_bias);
				s += 1;

				res = spot_light_reflection(u_light_pos[i], v_world_position, u_light_front[i], u_light_cone[i], u_intensity[i], degamma(u_light_color[i]), shadow, N, u_shininess, u_camera_pos);
			}
		}
		diffuse += res.d;
		specular += res.s;
	}

	phong += k*(diffuse + specular);
	FragColor = vec4(phong, color.a);
}


//-------------------------------------------------------------------------//
//							SHADOWMAPS 									   //
//-------------------------------------------------------------------------//

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


//-------------------------------------------------------------------------//
//								FILL THE G-BUFFER  						   //
//-------------------------------------------------------------------------//

\fill_gbuffer.fs

#version 330 core
#include "perturbNormal"
#include "gamma_functions"

in vec2 v_uv; // to sample textures
in vec3 v_normal; // normal interpolated
in vec3 v_world_position;
in vec4 v_current_position;
in vec4 v_prev_position;

// Uniforms from mateiral->bind:
uniform sampler2D u_texture; // color texture
uniform sampler2D u_normalmap;
uniform sampler2D u_MetalicRoughness;
uniform float u_alpha_cutoff;
uniform vec4 u_color;

// Uniforms motion blur:
uniform float u_delta_time;
uniform float u_exposure;
uniform vec2 u_viewport_size;
uniform float u_max_velocity;

// Replace out vec4 FragColor with:
layout(location = 0) out vec4 gbuffer_albedo; // store color info here
layout(location = 1) out vec4 gbuffer_normal_mat; // store normal info here
layout(location = 2) out vec4 gbuffer_metallic_roughness; // store metallic and roughness info here
layout(location = 3) out vec4 gbuffer_velocity; // store velocity buffer

void main()
{
	vec4 color = vec4(degamma(u_color.xyz), u_color.a);
	vec4 albedo_color = texture(u_texture, v_uv);
	color *= vec4(degamma(albedo_color.xyz), albedo_color.a);
	if(color.a < u_alpha_cutoff) 
		discard;

	vec3 nm_color = texture(u_normalmap, v_uv).xyz * 2.0 - 1.0; //color sampled from the normalmap converted to a range [-1,1]
	vec3 N = normalize(perturbNormal(normalize(v_normal), v_world_position, v_uv, nm_color)); // rarnge [-1, 1]
	N = 0.5 * N + 0.5; // to texture range [0, 1]
	vec4 normal = vec4(N, 1.0); // if alpha = 0 -> transparent
	gbuffer_albedo = vec4(color.rgb, color.a); //save color in linear space. 
	gbuffer_normal_mat = normal;

	//METALNESS PARAMETER
	vec4 metallic_roughness = texture(u_MetalicRoughness, v_uv);
	gbuffer_metallic_roughness = metallic_roughness;

	//VELOCITY BUFFER
	vec2 a = (v_current_position.xy / v_current_position.w); // [-1, 1]
	vec2 b = (v_prev_position.xy / v_prev_position.w);
	vec2 velocity = (a - b) * 0.5; // texture coord's -> + 0.5 cancels out
	// velocity *= (u_exposure / max(u_delta_time, 0.0001));
	// velocity *= u_viewport_size * 0.5;
	// // Clamp very large velocities
	// float len = length(velocity);
	// velocity /= max(1.0, len / u_max_velocity);

	// // Optional encoding scale
	// velocity *= 0.5;

	gbuffer_velocity = vec4(velocity, 0.0, 1.0); 
}

//-------------------------------------------------------------------------//
//								DEFERRED RENDERING 						   //
//-------------------------------------------------------------------------//

\deferred_phong.fs

#version 330 core
#include "computeShadow"
#include "lighting_functions"
#include "gamma_functions"


// From quad.vs:
in vec2 v_uv; // to sample textures

// Camera Uniforms:
uniform mat4 u_inv_viewprojection;
uniform vec3 u_camera_pos;

//Lighting Uniforms:
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

// ShadowMaps:
const int MAX_SHADOWS = 2; //only directional and spot lights will cast shadows
uniform float u_shadow_bias;
uniform sampler2D u_shadowmaps[MAX_SHADOWS];
uniform mat4 u_shadow_vps[MAX_SHADOWS];

// GBuffer textures:
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

//Ambient Occlusion:
uniform sampler2D u_ambient_occlusion;

// Color Output:
out vec4 FragColor;

void main()
{	
// Compute fragment world position: 
	float depth = texture(u_gbuffer_depth, v_uv).r; // texture range [0,1] stored in first channel
	if (depth >= 1.0) discard; // If the depth is so large then it belongs to the background or skybox!
	float depth_clip = 2.0 * depth - 1.0; // clip space range [-1, 1]
	vec2 uv_clip = 2.0 * v_uv - 1.0;
	vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_viewprojection * clip_coords; // from clip space to world space in homogeneous coord's
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w; // convert to cartesian coord's

// Extract Normal
	vec3 normal = 2.0 * texture(u_gbuffer_normal, v_uv).xyz - 1.0; //back to [-1, 1] range
	vec3 N = normalize(normal);

// Fragment's initial color -> without lighting
	vec4 color = texture(u_gbuffer_color, v_uv); //Already in linear space. 
	vec3 k = color.rgb; //k = k_a = k_s = k_d

//AMBIENT LIGHT:
	float ao_term = texture(u_ambient_occlusion, v_uv).r;
	vec3 phong = degamma(u_ambient_light) * k * ao_term;

	// VARIABLES:
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float shadow;
	int s = 0; // to know in which shadowmap we are.
	diffuseSpecular res;

	for (int i = 0; i < MAX_LIGHTS; i++) {
		if(i >= u_num_lights) break;
		vec3 light_color = degamma(u_light_color[i]);
	//POINT LIGHT
		if (u_light_type[i] == 1) {
			res = point_light_reflection(u_light_pos[i], world_pos, u_intensity[i], light_color, N, u_camera_pos, u_shininess);
		}
	//DIRECTIONAL LIGHT 
		else if (u_light_type[i] == 3) {
		//SHADOWS:
			shadow = isShadow(u_shadow_vps[s], world_pos, u_shadowmaps[s], u_shadow_bias);
			s += 1;
			res = directional_light_reflection(shadow, u_intensity[i], u_shininess, light_color, u_light_front[i], N, u_camera_pos, world_pos);
		}
	//SPOT LIGHT 
		else {
		//SHADOWS:
			shadow = isShadow(u_shadow_vps[s], world_pos, u_shadowmaps[s], u_shadow_bias);
			s += 1;
			res = spot_light_reflection(u_light_pos[i], world_pos, u_light_front[i], u_light_cone[i], u_intensity[i], light_color, shadow, N, u_shininess, u_camera_pos);
		}
		diffuse += res.d;
		specular += res.s;
	}
	phong += k*(diffuse + specular);
	FragColor = vec4(phong, color.a);
}

//-------------------------------------------------------------------------//
//						AMBIENT & DIRECTIONAL LIGHT						   //
//-------------------------------------------------------------------------//

\ambient_render.fs

#version 330 core
#include "computeShadow"
#include "lighting_functions"
#include "gamma_functions"

//From quad.vs:
in vec2 v_uv; //in texture space [0,1]

//From cpp:
uniform vec3 u_camera_pos;
uniform mat4 u_inv_viewprojection;

//GBuffer:
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_gbuffer_normal;

//Ambient Occlusion:
uniform sampler2D u_ambient_occlusion;

//Light:
uniform vec3 u_ambient_light;
uniform float u_intensity;
uniform vec3 u_light_color;
uniform vec3 u_light_front;
uniform float u_shininess;

//Shadowmaps:
uniform mat4 u_shadow_vps;
uniform sampler2D u_shadowmaps;
uniform float u_shadow_bias;

// Replace out vec4 FragColor with:
// layout(location = 0) out vec4 lighting_FBO; // store color info here
out vec4 FragColor;

void main()
{
	float depth = texture(u_gbuffer_depth, v_uv).r; // texture range [0,1] stored in first channel
	if (depth >= 1.0) discard; // If the depth is so large then it belongs to the background or skybox!
	vec4 color = texture(u_gbuffer_color, v_uv); //no need for alpha cutoff already done when filling the gbuffer
	vec3 k = color.rgb;//already in linear space

// Compute fragment world position: 
	float depth_clip = 2.0 * depth - 1.0; // clip space range [-1, 1]
	vec2 uv_clip = 2.0 * v_uv - 1.0; //same, range [-1, 1]
	vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_viewprojection * clip_coords; // from clip space to world space in homogeneous coord's
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w; // convert to cartesian coord's

//SHADOWS:
	float shadow = isShadow(u_shadow_vps, world_pos, u_shadowmaps, u_shadow_bias);

// Extract Normal
	vec3 normal = 2.0 * texture(u_gbuffer_normal, v_uv).xyz - 1.0; //back to [-1, 1] range
	vec3 N = normalize(normal);

//DIRECTIONAL LIGHT:
	diffuseSpecular res = directional_light_reflection(shadow, u_intensity, u_shininess, degamma(u_light_color), u_light_front, N, u_camera_pos, world_pos);

//AMBIENT OCCLUSION:
	float ao_term = texture(u_ambient_occlusion, v_uv).r;
	vec3 ao_light = degamma(u_ambient_light) * ao_term;

//AMBIENT + DIRECTIONAL W/ SHADOWS:
	vec3 phong = k*(ao_light + res.d + res.s);
	FragColor = vec4(phong, color.a);
}

//-------------------------------------------------------------------------//
//								LIGHT VOLUMES     						   //
//-------------------------------------------------------------------------//

\volume_render.fs

# version 330 core
#include "computeShadow"
#include "lighting_functions"
#include "gamma_functions"

// From basic.vs:
in vec2 v_uv; // to sample textures

// Camera Uniforms:
uniform mat4 u_inv_viewprojection;
uniform vec3 u_camera_pos;

//Lighting Uniforms:
uniform vec3 u_ambient_light;
uniform float u_shininess;
uniform vec3 u_light_pos;
uniform vec3 u_light_color;
uniform float u_intensity;
uniform int u_light_type;
uniform vec3 u_light_front;
uniform vec2 u_light_cone;

// ShadowMaps:
uniform float u_shadow_bias;
uniform sampler2D u_shadowmaps;
uniform mat4 u_shadow_vps;

// GBuffer textures:
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;

uniform vec2 u_res_inv;


// Color Output:
out vec4 FragColor;


void main()
{
	vec2 screen_size_uv = gl_FragCoord.xy * u_res_inv; // to sample the gbuffer textures

// Compute fragment world position: 
	float depth = texture(u_gbuffer_depth, screen_size_uv).r; // texture range [0,1] stored in first channel
	if (depth >= 1.0) discard; // If the depth is so large then it belongs to the background or skybox!
	float depth_clip = 2.0 * depth - 1.0; // clip space range [-1, 1]
	vec2 uv_clip = 2.0 * screen_size_uv - 1.0;
	vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_viewprojection * clip_coords; // from clip space to world space in homogeneous coord's
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w; // convert to cartesian coord's

// Extract Normal:
	vec3 normal = 2.0 * texture(u_gbuffer_normal, screen_size_uv).xyz - 1.0; //back to [-1, 1] range
	vec3 N = normalize(normal);

// Fragment's initial color -> without lighting
	vec4 color = texture(u_gbuffer_color, screen_size_uv); //already in linear space. 
	vec3 k = color.rgb; //k = k_a = k_s = k_d
	
// Define some variables:
	diffuseSpecular res;
	vec3 phong;
	float shadow;

//POINT LIGHT:
	if (u_light_type == 1) {
			res = point_light_reflection(u_light_pos, world_pos, u_intensity, degamma(u_light_color), N, u_camera_pos, u_shininess);		
		}
//SPOT LIGHT
	else {
		//SHADOWS:
			shadow = isShadow(u_shadow_vps, world_pos, u_shadowmaps, u_shadow_bias);
			res = spot_light_reflection(u_light_pos, world_pos, u_light_front, u_light_cone, u_intensity, degamma(u_light_color), shadow, N, u_shininess, u_camera_pos);
	}
	
	phong = k*(res.d + res.s);

	FragColor = vec4(phong, color.a);
}


\sphere_render.fs

out vec4 FragColor;

void main()
{
	FragColor = vec4(1.0, 0.0, 0.0, 1.0); //paint sphere red
}

//-------------------------------------------------------------------------//
//		 COOK-TORRANCE BRDF MODEL DEFERRED RENDERING					   //
//-------------------------------------------------------------------------//

\deferred_cook.fs

#version 330 core
#include "computeShadow"
#include "PBR_functions"
#include "gamma_functions"


// From quad.vs:
in vec2 v_uv; // to sample textures

// Camera Uniforms:
uniform mat4 u_inv_viewprojection;
uniform vec3 u_camera_pos;

//Lighting Uniforms:
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

// ShadowMaps:
const int MAX_SHADOWS = 2; //only directional and spot lights will cast shadows
uniform float u_shadow_bias;
uniform sampler2D u_shadowmaps[MAX_SHADOWS];
uniform mat4 u_shadow_vps[MAX_SHADOWS];

// GBuffer textures:
uniform sampler2D u_gbuffer_color;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_gbuffer_metallic_roughness;

//Ambient Occlusion:
uniform sampler2D u_ambient_occlusion;

// Color Output:
out vec4 FragColor;

void main()
{	
// Compute fragment world position: 
	float depth = texture(u_gbuffer_depth, v_uv).r; // texture range [0,1] stored in first channel
	if (depth >= 1.0) discard; // If the depth is so large then it belongs to the background or skybox!
	float depth_clip = 2.0 * depth - 1.0; // clip space range [-1, 1]
	vec2 uv_clip = 2.0 * v_uv - 1.0;
	vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, depth_clip, 1.0);
	vec4 not_norm_world_pos = u_inv_viewprojection * clip_coords; // from clip space to world space in homogeneous coord's
	vec3 world_pos = not_norm_world_pos.xyz / not_norm_world_pos.w; // convert to cartesian coord's

//Extract metallic and roughness parameters:
	vec4 metallic_roughness = texture(u_gbuffer_metallic_roughness, v_uv);
	float metallic = metallic_roughness.b;
	float roughness = metallic_roughness.g;

// Extract Normal
	vec3 normal = 2.0 * texture(u_gbuffer_normal, v_uv).xyz - 1.0; //back to [-1, 1] range
	vec3 N = normalize(normal);

// Fragment's initial color -> without lighting
	vec4 color = texture(u_gbuffer_color, v_uv);
	vec3 k = color.rgb; //k = k_a = k_s = k_d

// Define some variables:
 	vec3 L_vec, L, light_intensity, diffuse_contrib, R, V, specular_contrib, D;
	float d, attenuation, RV, LD, alpha_max, alpha_min, attenuation_distance, attenuation_angle, shadow;

//AMBIENT LIGHT:
	float ao_term = texture(u_ambient_occlusion, v_uv).r;
	vec3 BRDFcolor = degamma(u_ambient_light) * k * ao_term;
	vec3 outgoing_light = vec3(0.0);

	//CUMULATIVE VARIABLES:
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	int s = 0; // to know in which shadowmap we are.


	for (int i = 0; i < MAX_LIGHTS; i++) {
		if(i >= u_num_lights) break;

	// POINT LIGHT
		if (u_light_type[i] == 1) {
		// Li(p,L)
			L_vec = u_light_pos[i] - world_pos;
			L = normalize(L_vec);
			d = length(L_vec);
			attenuation = u_intensity[i]/pow(d,2);
			light_intensity = attenuation * degamma(u_light_color[i]); // what reaches the point
			V = normalize(u_camera_pos - world_pos);
			
			float LN = clamp(dot(L,N), 0.0, 1.0);
			
			outgoing_light = cookTorrance(L, V, k, metallic, roughness, N) * light_intensity * LN;	
		}
	
	//DIRECTION LIGHT 
		else if (u_light_type[i] == 3) {
		//There is no attenuation:
			light_intensity = u_intensity[i] * degamma(u_light_color[i]);

		//SHADOWS:
			shadow = isShadow(u_shadow_vps[s], world_pos, u_shadowmaps[s], u_shadow_bias);
			s += 1;

		//Li(p,L)
			L = normalize(u_light_front[i]);
			V = normalize(u_camera_pos - world_pos);

			float LN = clamp(dot(L,N), 0.0, 1.0);
			outgoing_light = cookTorrance(L, V, k, metallic, roughness, N) * light_intensity * LN * shadow;	
		}

	//SPOT LIGHT 
		else {
			L_vec = u_light_pos[i] - world_pos;
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
				light_intensity = degamma(u_light_color[i]) * attenuation;
			//SHADOWS: only if the pixel is illuminated by the spot light
				shadow = isShadow(u_shadow_vps[s], world_pos, u_shadowmaps[s], u_shadow_bias);
			}
			else {
				light_intensity = vec3(0.0);
				shadow = 0.0;				
			}

			s += 1;

		//Li(p,L)
			V = normalize(u_camera_pos - world_pos);
			float LN = clamp(dot(L,N), 0.0, 1.0);
			outgoing_light = cookTorrance(L, V, k, metallic, roughness, N) * light_intensity * LN * shadow;
		}

		BRDFcolor += outgoing_light;
	}

	FragColor = vec4(BRDFcolor, color.a);
}


//-------------------------------------------------------------------------//
//				 COOK-TORRANCE BRDF MODEL SINGLE PASS RENDERING			   //
//-------------------------------------------------------------------------//

\phong_cook.fs

#version 330 core
#include "perturbNormal"
#include "computeShadow"
#include "PBR_functions"
#include "gamma_functions"

//From basic.vs:
in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
// in vec3 v_tangent;

//From renderMeshWithMaterial:
// uniform mat4 u_model; //object's model
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
uniform float u_shadow_bias;
uniform sampler2D u_shadowmaps[MAX_SHADOWS];
uniform mat4 u_shadow_vps[MAX_SHADOWS];

//From material->bind:
uniform vec4 u_color; //color of the material
uniform sampler2D u_texture; //object's texture
uniform float u_alpha_cutoff; //threshold to decide wheter to paint or not 
uniform sampler2D u_normalmap;

//Metalic Texture
uniform sampler2D u_MetalicRoughness; 



out vec4 FragColor;

void main() 
{

	vec4 color = vec4(degamma(u_color.xyz), u_color.a);
	vec4 albedo_color = texture(u_texture, v_uv);
	color *= vec4(degamma(albedo_color.xyz), albedo_color.a); //color multiplied by texture color (with alpha cutoff already applied)
	if (color.a < u_alpha_cutoff) 
		discard;

	vec3 k = color.rgb; //k = k_a = k_s = k_d

//VARIABLES TO BE USED:
	vec3 L_vec, L, N, light_intensity, diffuse_contrib, R, V, specular_contrib, D;
	float d, attenuation, RV, LD, alpha_max, alpha_min, attenuation_distance, attenuation_angle, shadow;
	int s = 0; //to know in which shadowmap we are

//AMBIENT LIGHT:
	vec3 BRDFcolor = degamma(u_ambient_light) * k;

//NORMALS WITH NORMALMAPS:
	vec3 nm_color = normalize((texture(u_normalmap, v_uv).xyz * 2.0) - 1.0); //color sampled from the normalmap converted to a range [-1,1]
	N = perturbNormal(normalize(v_normal), v_world_position, v_uv, nm_color);

//METALIC TEXTURE:
	vec4 metallic_roughness = texture(u_MetalicRoughness, v_uv);
	float roughness = metallic_roughness.g;
	float metallic  = metallic_roughness.b;

	vec3 outgoing_light = vec3(0.0);
	
	for(int i = 0; i<MAX_LIGHTS; i++) {
		if (i < u_num_lights) {
		
	//POINT LIGHT
			if (u_light_type[i] == 1) {
			// Li(p,L)
				L_vec = u_light_pos[i] - v_world_position;
				L = normalize(L_vec);
				d = length(L_vec);
				attenuation = u_intensity[i]/pow(d,2);
				light_intensity = attenuation * degamma(u_light_color[i]); // what reaches the point
				V = normalize(u_camera_pos - v_world_position);
			
				float LN = clamp(dot(L,N), 0.0, 1.0);
			
				outgoing_light = cookTorrance(L, V, k, metallic, roughness, N) * light_intensity * LN;
			} 
			
	//DIRECTIONAL LIGHT
			else if (u_light_type[i] == 3) {
			//There is no attenuation:
				light_intensity = u_intensity[i] * degamma(u_light_color[i]);

			//SHADOWS:
				shadow = isShadow(u_shadow_vps[s], v_world_position, u_shadowmaps[s], u_shadow_bias);
				s += 1;
		
			//Li(p,L)
			
			V = normalize(u_camera_pos - v_world_position);
			L=normalize(u_light_front[i]);
			float LN = clamp(dot(L,N), 0.0, 1.0);
			outgoing_light = cookTorrance(L, V, k, metallic, roughness, N) * light_intensity * LN * shadow;	
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
					light_intensity = degamma(u_light_color[i]) * attenuation;

				//SHADOWS: only if the pixel is illuminated by the spot light
					shadow = isShadow(u_shadow_vps[s], v_world_position, u_shadowmaps[s], u_shadow_bias);
				}
				else{
					light_intensity = vec3(0.0);
					shadow = 0.0;
				}

				s += 1;

				//Li(p,L)
				V = normalize(u_camera_pos - v_world_position);
				float LN = clamp(dot(L,N), 0.0, 1.0);
				outgoing_light = cookTorrance(L, V, k, metallic, roughness, N) * light_intensity * LN * shadow;
			}
			BRDFcolor += outgoing_light;
		}
	}

	FragColor = vec4(BRDFcolor, color.a);
	// FragColor = vec4(metallic, roughness, 0.0, 1.0); //Em surt groc per tant llegeixo sermpre textures blanques. 
}

\ambient_occlusion.fs

#version 330 core
#include "linear_depth"

in vec2 v_uv; //we can get the uv coordinates from the quad

const int MAX_POINTS = 30;
uniform vec2 u_res_inv;
uniform sampler2D u_gbuffer_depth;
uniform sampler2D u_gbuffer_normal;
uniform sampler2D u_gbuffer_metallic_roughness;
uniform int u_sample_count;
uniform float u_sample_radius;
uniform vec3 u_sample_pos[MAX_POINTS];
uniform mat4 u_proj_mat;
uniform mat4 u_inv_proj_mat;
uniform mat4 u_view_mat;
uniform bool isHemi;
uniform float near;
uniform float far;
uniform bool isBaked;
uniform bool isAO;

out vec4 FragColor;

void main()
{
	//White texture if no AO == do nothing
	if(!isAO) {
		FragColor = vec4(1.0);
		return;
	}

	vec2 uv = v_uv + 0.5 * u_res_inv; //center the uv coords in the middle of the pixel
	float depth = texture(u_gbuffer_depth, uv).r;	

//Skip if we are in the sky
	if (depth >= 1.0) {
		FragColor = vec4(1.0); // white
		return;
	}

	vec3 N = normalize(texture(u_gbuffer_normal, uv).xyz * 2.0 - 1.0); //les necessito en coordenades de [-1,1] que son 
	N = (u_view_mat * vec4(N, 0.0)).xyz; // we want to rotate a direction so we are not interested in translations -> last coord 0.0.
	//ara la N en view space. 

	// TBN matrix
	vec3 v = vec3(0.0, 1.0, 0.0);
	vec3 T = normalize(v - N * dot(v,N)); // tangent
	vec3 B = cross(N,T); // bitangent
	mat3 rotmat = mat3(T, B, N); 

	vec4 clip_coords = vec4(uv.x, uv.y, depth, 1.0); // homogeneous coords
	clip_coords.xyz = clip_coords.xyz * 2.0 - 1.0; //convert to NDC screen and depth sapce [0,1] --> [-1,1]

	vec4 view_sample_origin = u_inv_proj_mat * clip_coords; // recover the original 3D point in camera sapce
	view_sample_origin /= view_sample_origin.w; // dehomogenize

	float ao_term = 0.0;
	for (int i = 0; i < u_sample_count; i++) {
		vec3 view_sample = u_sample_pos[i];
		if (isHemi) {
			if (view_sample.z < 0){ // to avoid having to generate again the random points
				view_sample.z *= -1.0;
			}
			view_sample = rotmat * view_sample;
		}
		view_sample *= u_sample_radius;
		view_sample += view_sample_origin.xyz; // sphere is now centered at the 3D point in camera space previously computed

		vec4 proj_sample = u_proj_mat * vec4(view_sample, 1.0); // the projection matrix is 4x4 (View space to Clip space) [-1,1]
		proj_sample /= proj_sample.w;
		proj_sample = clamp(proj_sample, -1.0, 1.0);

		vec2 sample_uv = proj_sample.xy * 0.5 + 0.5; // texture range [0,1]
		float sample_depth = texture(u_gbuffer_depth, sample_uv).r; // [0,1]
		sample_depth = sample_depth * 2.0 - 1.0; // to [-1,1]

		// If point is not occluded: 
		if (proj_sample.z < sample_depth) { 
			ao_term += 1.0;
		}
	}

	ao_term /= u_sample_count; // [0, 1]. Therefore, it is the probability of the point being occluded
	if (isBaked) {
		float baked_ao = texture(u_gbuffer_metallic_roughness, uv).r;
		ao_term = min(ao_term, baked_ao);
	}
	FragColor = vec4(ao_term);
}

\blur.fs

in vec2 v_uv;

uniform sampler2D u_input_texture;
uniform vec2 u_texture_size_inv;

out vec4 FragColor;

// 33x33 kernel
const int M = 3; 		 // half_window = 3
const int N = 2 * M + 1; // window_size = 7

// sigma = 10
const float coeffs[N] = float[N](
	0.00443184, // i = -3
    0.05399097, // i = -2
    0.24197072, // i = -1
    0.39894228, // i =  0 (Centro)
    0.24197072, // i =  1
    0.05399097, // i =  2
    0.00443184  // i =  3
);

void main()
{
	vec4 sum = vec4(0.0);

	for (int i = 0; i < N; ++i)
	{
		for (int j = 0; j < N; ++j)
		{
			vec2 uv = v_uv + u_texture_size_inv * vec2(float(i - M), float(j - M)); // shift the uv to get the values from the nwighboring pixels
			sum += coeffs[i] * coeffs[j] * texture(u_input_texture, uv);
		}
	}

	FragColor = sum;
}

\tonemapper.fs

#version 330 core

in vec2 v_uv;

uniform sampler2D u_texture;
uniform sampler2D u_depth;
uniform float u_scale;
uniform float u_average_lum;
uniform float u_lumwhite2;
uniform float u_igamma;

out vec4 FragColor;

void main()
{
	float depth = texture(u_depth, v_uv).r;
	if (depth >= 1.0) {
		discard;
	}
    vec4 color = texture2D(u_texture, v_uv);
    vec3 rgb = color.xyz;

    float lum = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    float L = (u_scale / u_average_lum) * lum;
    float Ld = (L * (1.0 + L / u_lumwhite2)) / (1.0 + L);

    rgb = (rgb / lum) * Ld;
    rgb = max(rgb, vec3(0.001));
    rgb = pow(rgb, vec3(u_igamma));

    FragColor = vec4(rgb, color.a);
	// FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}

\tonemapperND.fs

#version 330 core
#include "gamma_functions"

in vec2 v_uv;

uniform sampler2D u_texture;
uniform sampler2D u_depth;
uniform bool isTonemapper;

out vec4 FragColor;

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float G = 11.2;

vec3 Uncharted2TonemapPartial(vec3 x) {
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F; 
}

void main()
{
	float depth = texture(u_depth, v_uv).r;
	// discard if skybox
	if (depth >= 1.0) {
		discard;
	}
    
	vec3 color = texture(u_texture, v_uv).rgb; // already sotored in linear
	if (!isTonemapper) {
		vec4 out_color = texture(u_texture, v_uv);
		FragColor = vec4(gamma(out_color.xyz), out_color.a);
		return;
	}
	// vec3 color = texture(u_texture,v_uv).rgb;
    vec3 tonemapped_color = Uncharted2TonemapPartial(color*2.0);
	vec3 W = vec3(11.2f);
	vec3 white_scale = vec3(1.0f) / Uncharted2TonemapPartial(W);
	
	FragColor = vec4(gamma(tonemapped_color * white_scale), 1.0);
}

\render_screen.fs

#version 330 core
#include "gamma_functions"

in vec2 v_uv;
uniform sampler2D u_texture;
uniform sampler2D u_depth;
out vec4 FragColor;

void main()
{
	float depth = texture(u_depth, v_uv).r;
	// discard if skybox
	if (depth >= 1.0) {
		discard;
	}

	FragColor = vec4(gamma(texture(u_texture, v_uv).xyz), 1.0);
}

\camera_motion_blur.fs

#version 330 core

#include "gamma_functions"

in vec2 v_uv;

uniform mat4 u_currentToPrevMat; // prev_viewproj * inv_viewproj
uniform sampler2D u_texture;
uniform sampler2D u_depth;
uniform sampler2D u_velocity;
uniform int currentFps;
uniform int nSamples;

out vec4 FragColor;

void main(){
	float depth = texture(u_depth, v_uv).r;
	if (depth >= 1.0) {
		discard;
	}
	// float depth_clip = 2.0 * depth - 1.0; // clip space range [-1, 1]
	// vec2 uv_clip = 2.0 * v_uv - 1.0;

	// //get previous screen space position
	// vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, depth_clip, 1.0);
	// vec4 not_norm_screen_prev_pos = u_currentToPrevMat * clip_coords; // from clip space to world space in homogeneous coord's
	// vec3 previous = not_norm_screen_prev_pos.xyz / not_norm_screen_prev_pos.w; // convert to cartesian coord's

	// previous = previous * 0.5 + 0.5; // to [0,1], texture coordinates

	// vec2 blur_vector = previous.xy - v_uv; // range [-1, 1]

	// vec2 normal = normalize(blur_vector);
	// // Map values to be between 0 and 1.
	// normal.x = (normal.x + 1) * 0.5;
	// normal.y = (normal.y + 1) * 0.5;
	// // Convert to array of color values.
	// FragColor = vec4(normal.x, normal.y, 0.0, 1.0);
	// return;

	vec2 blur_vector = texture(u_velocity, v_uv).rg;

	float blurScale = currentFps / 60.0;

	// perform blur:
	vec4 result = texture(u_texture, v_uv);
	for (int i = 1; i < nSamples; ++i) {
	// get offset in range [-0.5, 0.5]:
    	vec2 offset = blurScale * blur_vector * (float(i) / float(nSamples - 1) - 0.5);
  
	// sample & add to result:
    	result += texture(u_texture, v_uv + offset); // already in linear
   	}
 
	result /= float(nSamples);
	FragColor = vec4(result.xyz, result.a);
}


\object_motion_blur.fs

#version 330 core
#include "gamma_functions"

in vec2 v_uv;

const int MAX_SAMPLES = 10;

uniform sampler2D u_texture;
uniform sampler2D u_velocity;
uniform sampler2D u_depth;
uniform int currentFps;
// uniform int nSamples;

out vec4 FragColor;

void main() {

	float depth = texture(u_depth, v_uv).r;
	if (depth >= 1.0) {
		discard;
	}
	vec2 velocity = texture(u_velocity, v_uv).rg;
	float velocity_scale = currentFps / 60.0; //our target fps is 60
	vec2 texelSize = 1.0 / vec2(textureSize(u_texture, 0)); // 0: texture size at mipmap level 0 
	float speed = length(velocity / texelSize);
  	int nSamples = clamp(int(speed), 1, MAX_SAMPLES);

   	vec4 res = texture(u_texture, v_uv); // already in linear
   	for (int i = 1; i < nSamples; ++i) {
    	vec2 offset = velocity * (float(i) / float(nSamples - 1) - 0.5);
    	res += texture(u_texture, v_uv + offset); 
   	}
   	FragColor = res / float(nSamples);
}

\fill_vbuffer.fs

#version 330 core 

in vec2 v_uv;

uniform sampler2D u_depth;
uniform mat4 u_inv_viewprojection;
uniform mat4 u_prev_viewprojection;

out vec2 FragColor;

void main() {

	float depth = texture(u_depth, v_uv).r;
	if (depth >= 1.0) {
		discard; //belongs to the background / skybox
	}

	vec2 clip_coords = v_uv * 2.0 - 1.0; // [-1, 1]
	vec4 H = vec4(clip_coords, depth, 1.0); // clip space
	vec4 D = u_inv_viewprojection * H; // world space
	vec4 world_pos = D / D.w; // dehomogenize
	vec2 current_pos = H.xy; // [-1, 1]
	current_pos = 0.5 * current_pos + 0.5; // [0, 1]
	vec4 prev_clip = u_prev_viewprojection * world_pos; // clip space in prev frame
	prev_clip /= prev_clip.w; // dehomogenize
	vec2 prev_pos = 0.5 * prev_clip.xy + 0.5; // [0, 1]
	//VELOCITY BUFFER
	vec2 velocity = current_pos.xy - prev_pos.xy;
	FragColor = velocity;
}


\fill_vbuffer2.fs

#version 330 core

in vec4 v_current_position;
in vec4 v_prev_position;

out vec2 FragColor;

void main() {
	vec2 a = v_current_position.xy / v_current_position.w;
	vec2 b = v_prev_position.xy / v_prev_position.w;

	FragColor = (a - b) * 0.5;
}