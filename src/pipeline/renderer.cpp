#include "renderer.h"

#include <algorithm> //sort

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"
#include "../gfx/fbo.h"
#include "../pipeline/prefab.h"
#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"

#include "scene.h"


using namespace SCN;

//some globals
GFX::Mesh sphere;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename)) // Loads all shaders from a file, exists if fails
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f); // Generates sphere vertices
	sphere.uploadToVRAM(); // Uploads to GPU memory

	for (int i = 0; i < MAX_SHADOWS; i++) {
		shadow_fbos[i] = new GFX::FBO();
		shadow_fbos[i]->setDepthOnly(shadow_map_resolution, shadow_map_resolution);
		shadow_maps[i] = shadow_fbos[i]->depth_texture;
	}	
}

Renderer::~Renderer() {
	for (int i = 0; i < MAX_SHADOWS; i++) {
		if (shadow_fbos[i]) {
			delete shadow_fbos[i];
		}
	}
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

void Renderer::parseNode(Node* node) {
	if (!node) {
		return; //not analyze empty nodes
	}

	if (node->material && node->material->alpha_mode == BLEND) {
		//node->material->shininess = this->shininess;
		translucent_list.push_back({
			.mesh = node->mesh,
			.material = node->material,
			.model = node->getGlobalMatrix()
			});
	}
	else {
		opaque_list.push_back({
			.mesh = node->mesh,
			.material = node->material,
			.model = node->getGlobalMatrix()
			});
	}

	for (Node* child : node->children) {
		parseNode(child);
	}
}

void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam) {
	// HERE =====================
	// TODO: GENERATE RENDERABLES
	// ==========================
	//render_list.clear(); // Clear the previous frame
	opaque_list.clear();
	translucent_list.clear();
	light_list.clear();
	lights.clear();
	light_cameras.clear();

	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		if (entity->getType() == eEntityType::PREFAB) {
			PrefabEntity* e = (PrefabEntity*) entity;
			parseNode(&(entity->root));
		}

		if (entity->getType() == eEntityType::LIGHT) {
			LightEntity* l = (LightEntity*)entity; // cast from BaseEntity to LightEntity
			light_list.push_back({
				.color = l->color,
				.pos = l->root.getGlobalMatrix().getTranslation(),
				.intensity = l->intensity,
				.front = l->root.getGlobalMatrix().frontVector(),
				.l_type = l->light_type,
				.cone_info = l->cone_info

			});
			lights.push_back(l);
		}
	}
	
}

char Renderer::isInsideFrustum(sRenderable* obj, Camera* camera)
{
	if (!obj->mesh)
		return CLIP_OUTSIDE;

	Vector3f local_center = obj->mesh->box.center; // center of the bbox that encloses the mesh.
	//float local_rad = obj->mesh->radius;
	Vector3f world_center = obj->model * local_center; // world space
	Vector3f scale_vec = obj->model.getScale();
	//float scale = max(scale_vec.x, max(scale_vec.y, scale_vec.z)); 
	//float world_radius = local_rad * scale; // scaling factor: the object in world space might be enlarged or reduced (and therefore its radius too
	//return camera->testSphereInFrustum(world_center, world_radius);
	return camera->testBoxInFrustum(world_center, obj->mesh->box.halfsize * scale_vec);
};

void Renderer::fillLightArrays(Vector3f* light_pos, Vector3f* light_color, float* light_intensity, Vector3f* light_font, int* light_type, Vector2f* light_cone)
{
	for (int i = 0; i < light_list.size(); i++) { //cannot render more than the lights we already have stored
		if (i >= MAX_LIGHTS) break; //We only render up to MAX_LIGHTS
		light_pos[i] = light_list[i].pos;
		light_color[i] = light_list[i].color;
		light_intensity[i] = light_list[i].intensity;
		light_font[i] = light_list[i].front;
		light_type[i] = static_cast<int>(light_list[i].l_type); //Per canviar de eLightType a un int. 
		light_cone[i].x = DEG2RAD * light_list[i].cone_info.x; //store in radians
		light_cone[i].y = DEG2RAD * light_list[i].cone_info.y;
	}
}

void Renderer::createLightCameras(Camera* camera) 
{
	for (int i = 0; i < lights.size(); i++) {

		if (lights[i]->light_type != POINT) {
			mat4 light_model = lights[i]->root.getGlobalMatrix(); // Model matrix of the light
			vec3 light_position = light_model.getTranslation(); // Position
			Camera* light_camera = new Camera();
			light_camera->lookAt(light_position, light_model * vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));
			//light_pos: eye of the camera, light_model * vec3: direction of the camera, 	//up: which way is up
			float half_size = lights[i]->area / 2.0f; //half of the width of the world that the light covers
			if (lights[i]->light_type == DIRECTIONAL) {
				light_camera->setOrthographic(-half_size, half_size, -half_size, half_size, lights[i]->near_distance, lights[i]->max_distance);
			}
			else light_camera->setPerspective(camera->fov, camera->aspect, camera->near_plane, camera->far_plane);

			light_cameras.push_back(light_camera);

			shadow_vps[i] = light_camera->viewprojection_matrix;
		}
		
	}
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera); // fill the render list

	createLightCameras(camera); //Configure the light camera:

//SHADOW MAP:
	glColorMask(false, false, false, false); // Disable writing to color

	for (int i = 0; i < light_cameras.size(); i++) {
		if (i < MAX_SHADOWS) {
			shadow_fbos[i]->bind(); //Activem el nostre FBO de profunditat. 
			glClear(GL_DEPTH_BUFFER_BIT); //Clear depth buffer from prev frame

			//Render the scene from the lights point of view --> fill the depth buffer
			for (sRenderable call : opaque_list) {
				if (isInsideFrustum(&call, light_cameras[i]) != CLIP_OUTSIDE) {
					renderPlain(light_cameras[i], call.model, call.mesh, call.material);
				}
			}
			shadow_fbos[i]->unbind();
		}
	}
 	
	glColorMask(true, true, true, true); // Enable writing to color


	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);
	
	//Fill light arrays:
	fillLightArrays(light_pos, light_color, light_intensity, light_front, light_type, light_cone);

//RENDER ENTITIES:

	// we have to sort the objects lists
	Vector3 cam_pos = camera->eye;

	sort(opaque_list.begin(), opaque_list.end(), [&cam_pos](sRenderable& a, sRenderable& b) { //const --> error getTranslation
		float da = (a.model.getTranslation() - cam_pos).length();
		float db = (b.model.getTranslation() - cam_pos).length();
		return da < db;}); // First objects that are closer (minimize overwriting)

	sort(translucent_list.begin(), translucent_list.end(), [&cam_pos](sRenderable& a, sRenderable& b) { //const --> error getTranslation
		float da = (a.model.getTranslation() - cam_pos).length();
		float db = (b.model.getTranslation() - cam_pos).length();
		return da > db;}); // First objects that are closer (minimize overwriting)

	// render opaque list
	for (sRenderable call : opaque_list) {
		if (isInsideFrustum(&call, camera) != CLIP_OUTSIDE) renderMeshWithMaterial(light_cameras[1], call.model, call.mesh, call.material); // inside frustum -> render
	}
	// render translucent list
	for (sRenderable call : translucent_list) {
		if (isInsideFrustum(&call, camera) != CLIP_OUTSIDE) renderMeshWithMaterial(light_cameras[1], call.model, call.mesh, call.material);
	}

}

void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;

	// Apply skybox necesarry config:
	// No blending, no dpeth test, we are always rendering the skybox
	// Set the culling aproppiately, since we just want the back faces
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader* shader = GFX::Shader::Get("skybox");
	if (!shader)
		return;
	shader->enable();

	// Center the skybox at the camera, with a big sphere
	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(10, 10, 10);
	shader->setUniform("u_model", m);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	shader->setUniform("u_texture", cubemap, 0);

	sphere.render(GL_TRIANGLES);

	shader->disable();

	// Return opengl state to default
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(Camera* light_cam, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;
	Scene* scene = Scene::instance;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	//shader = GFX::Shader::Get("texture");
	shader = GFX::Shader::Get("phong");

    assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	
	shader->enable();

	material->bind(shader);

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_pos", camera->eye);

	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t );

	//Uniforms Light
	shader->setUniform("u_ambient_light", scene->ambient_light);
	shader->setUniform3Array("u_light_pos", (float*)&light_pos, MAX_LIGHTS); //enviem al shader la posició de memoria de la primera posició de les llums i quantes llums hi ha com a màxim.
	shader->setUniform1Array("u_intensity", (float*)light_intensity, MAX_LIGHTS);
	shader->setUniform3Array("u_light_color", (float*)&light_color, MAX_LIGHTS);
	shader->setUniform3Array("u_light_front", (float*)&light_front, MAX_LIGHTS);
	shader->setUniform1Array("u_light_type", (int*)&light_type, MAX_LIGHTS);
	shader->setUniform2Array("u_light_cone", (float*)&light_cone, MAX_LIGHTS);
	shader->setUniform("u_num_lights", (int)light_list.size());
	shader->setUniform("u_shininess", this->shininess);
	shader->setUniform("u_shadow_bias", 0.0001f);
	shader->setUniform("u_shadowmaps[0]", shadow_fbos[0]->depth_texture, 2);
	shader->setUniform("u_shadowmaps[1]", shadow_fbos[1]->depth_texture, 3);
	shader->setMatrix44Array("u_shadow_vps", shadow_vps, MAX_SHADOWS);


	// Render just the verticies as a wireframe
	if (render_wireframe)
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

void Renderer::renderPlain(Camera* light_cam, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material) {
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Scene* scene = Scene::instance;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	shader = GFX::Shader::Get("plain");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;

	shader->enable();

	material->bind(shader);

	shader->setUniform("u_camera_pos", light_cam->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", light_cam->viewprojection_matrix);

	// Render just the verticies as a wireframe
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//add here your stuff
	//...
	
	// Toggle between SiglePass and MultiPass:
	ImGui::Checkbox("Multipass", &isMultipass);
	ImGui::SliderFloat("Shininess", &shininess, 0.0, 100.0);

}

#else
void Renderer::showUI() {}
#endif