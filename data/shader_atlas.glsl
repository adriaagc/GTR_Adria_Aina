//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
phong basic.vs phong.fs


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

\basic.vs

#version 330 core

in vec3 a_vertex; //3D coords of the vertex in local space
in vec3 a_normal; //direction the vertex is facing
in vec2 a_coord; //UV coords used to map a 2D texture
in vec4 a_color; //vertex color assigned

uniform vec3 u_camera_pos;
uniform mat4 u_model;
uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
//These variables will be interpolated and passed to the fragment shader
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

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

const int MAX_LIGHTS = 4;
//Light Uniforms:
uniform vec3 u_ambient_light; //constant
uniform vec3 u_light_pos[MAX_LIGHTS]; //array
uniform float u_intensity[MAX_LIGHTS]; //array
uniform vec3 u_light_color[MAX_LIGHTS]; //array
uniform vec3 u_light_front[MAX_LIGHTS]; //array
uniform int u_light_type[MAX_LIGHTS]; //array
uniform float u_shininess; //constant for the moment
uniform int u_num_lights;

//Camera Uniforms:
uniform vec3 u_camera_position; // eye of the camera

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, v_uv); //This will be our k = k_a = k_d = k_s

	if(color.a < u_alpha_cutoff)
		discard;

	//common multiple:
	vec3 k = color.rgb;
	//variables:
	float d, attenuation, RV;
	vec3 light_intensity, L, N, R, V, phong, diffuse_light_comp, specular_light_comp;

	//ambient:
	phong = u_ambient_light * k;

	for(int i = 0; i<MAX_LIGHTS; i++) {
		if (i < u_num_lights) {
			if (u_light_type[i]==1){ //point light
				//attenuated light intensity:
				d = distance(u_light_pos[i], v_world_position); //distance from point to light source in world space
				attenuation = u_intensity[i] / pow(d,2);
				light_intensity = u_light_color[i] * attenuation;

				//diffuse:
				L = normalize(u_light_pos[i] - v_world_position);
				N = normalize(v_normal);
				diffuse_light_comp = max(dot(N,L), 0.0) * light_intensity * k;
			
				//specular:
				R = normalize(reflect(-L,N));
				V = normalize(u_camera_position - v_world_position);
				RV = max(dot(R,V), 0.0);
				specular_light_comp = pow(RV, u_shininess) * light_intensity * k ;

				phong += diffuse_light_comp + specular_light_comp;
			}
			else if (u_light_type[i]==3){ //directional light
				//attenuated light intensity:
				light_intensity = u_light_color[i]; //* attenuation;

				//diffuse:
				L = normalize(-u_light_front[i]);
				N = normalize(v_normal);
				diffuse_light_comp = max(dot(N,L), 0.0) * light_intensity * k;
			
				//specular:
				R = normalize(reflect(-L,N));
				V = normalize(u_camera_position - v_world_position);
				RV = max(dot(R,V), 0.0);
				specular_light_comp = pow(RV, u_shininess) * light_intensity * k;

				phong += diffuse_light_comp + specular_light_comp;
			}
			else{ //spot light DE MOMENT SENSE CAP CANVI if (u_light_type[i]==2) 
				//attenuated light intensity:
				d = distance(u_light_pos[i], v_world_position); //distance from point to light source in world space
				attenuation = u_intensity[i] / pow(d,2);
				light_intensity = u_light_color[i] * attenuation;

				//diffuse:
				L = normalize(u_light_pos[i] - v_world_position);
				N = normalize(v_normal);
				diffuse_light_comp = max(dot(N,L), 0.0) * light_intensity * k;
			
				//specular:
				R = normalize(reflect(-L,N));
				V = normalize(u_camera_position - v_world_position);
				RV = max(dot(R,V), 0.0);
				specular_light_comp = pow(RV, u_shininess) * light_intensity * k ;

				phong += diffuse_light_comp + specular_light_comp;
			}
		}	
	}
	FragColor = vec4(phong, 1.0);
}
