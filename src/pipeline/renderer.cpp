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

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();

	//fbo = new GFX::FBO();
	//fbo->setDepthOnly(SHADOW_RES, SHADOW_RES);
	for (int i = 0; i < MAX_SHADOWS; i++) {
		fbos[i] = new GFX::FBO();
		fbos[i]->setDepthOnly(SHADOW_RES, SHADOW_RES);
	}

	
	//Create the G-Buffer
	gbuffer = new GFX::FBO();
	//int width, int height, int num_textures, int format, int type, bool use_depth_texture
	//Vector2f size(1024, 768); Window size 
	gbuffer->create(WIDTH,HEIGHT,2,GL_RGBA,GL_UNSIGNED_BYTE,true);

}

Renderer::~Renderer() {
	for (int i = 0; i < MAX_SHADOWS; i++) {
		if (fbos[i]) {
			delete fbos[i];
		}
	}
	//if (fbo) delete fbo;
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

void Renderer::parseNode(Node* node){
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

	opaque_list.clear();
	translucent_list.clear();
	lights_list.clear();


	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		// Store Prefab Entitys
		// ...
		//		Store Children Prefab Entities

		if (entity->getType() == eEntityType::PREFAB) {
			PrefabEntity* e = (PrefabEntity*)entity;
			parseNode(&(entity->root));
		}

		// Store Lights
		// ...

		if (entity->getType() == eEntityType::LIGHT) {
			LightEntity* l = (LightEntity*)entity;
			lights_list.push_back(l);
		}

	}
	
}

char Renderer::isInsideFrustum(sRenderable* obj, Camera* camera) {
	if (!obj->mesh)
		return CLIP_OUTSIDE;

	Vector3f world_center = obj->model * obj->mesh->box.center; // world space bbox center
	Vector3f world_halfsize = obj->model * obj->mesh->box.halfsize; // world space halfsize
	return camera->testBoxInFrustum(world_center, world_halfsize);
}

void Renderer::fillLightArrays()
{
	for (int i = 0; i < lights_list.size(); i++) { //cannot render more than the lights we already have stored
		if (i >= MAX_LIGHTS) break; //We only render up to MAX_LIGHTS
		light_pos[i] = lights_list[i]->root.getGlobalMatrix().getTranslation(); //position
		light_color[i] = lights_list[i]->color; //color
		light_intensity[i] = lights_list[i]->intensity; //intensity
		light_type[i] = static_cast<int>(lights_list[i]->light_type); //type: cast eLightType to int. 
		light_front[i] = lights_list[i]->root.getGlobalMatrix().frontVector();
		light_cone[i].x = DEG2RAD * lights_list[i]->cone_info.x; //store in radians
		light_cone[i].y = DEG2RAD * lights_list[i]->cone_info.y;
	}
}

void Renderer::createLightCameras()
{
	int c_idx = 0;
	int l_idx = -1;
	for (LightEntity* l : lights_list) {
		l_idx += 1;
		if (c_idx >= MAX_SHADOWS) return;
		if (l->light_type == POINT) continue; //only for non point light sources
		Camera light_cam;
		mat4 light_model = l->root.getGlobalMatrix();
		vec3 light_pos = light_model.getTranslation();
		light_cam.lookAt(light_pos, light_model * vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));
		if (l->light_type == DIRECTIONAL) {
			float half_size = lights_list[l_idx]->area / 2.0f;
			light_cam.setOrthographic(-half_size, half_size, -half_size, half_size, lights_list[l_idx]->near_distance, lights_list[l_idx]->max_distance);
		}
		else if (l->light_type == SPOT) {
			light_cam.setPerspective(2.0f * l->cone_info.y, 1.0f, l->near_distance, l->max_distance);
		}
		shadow_vps[c_idx] = light_cam.viewprojection_matrix;
		light_cameras[c_idx] = light_cam;
		c_idx += 1;
	}
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);

//SHADOW MAPS:
	createLightCameras();

	int i = 0;
	for (GFX::FBO* fbo : fbos) {
		fbo->bind();
		glColorMask(false, false, false, false);
		glClear(GL_DEPTH_BUFFER_BIT);
		for (sRenderable& call : opaque_list) {
			if (isInsideFrustum(&call, &light_cameras[i]) != CLIP_OUTSIDE) renderPlain(&light_cameras[i], call.model, call.mesh, call.material);
		}
		glColorMask(true, true, true, true);
		fbo->unbind();
		i += 1;
	}
	
	//G-BUFFER: 
	gbuffer->bind();

	// Per saber que guarda més d'una textura 
	GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, buffers);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//Neteja les textures
	for (sRenderable& call : opaque_list) {
		if(isInsideFrustum(&call, camera) != CLIP_OUTSIDE) renderGBuffer(camera, call.model, call.mesh, call.material);
	}
	gbuffer->unbind();

//PREAPARE LIGHT UNIFORMS:
	fillLightArrays(); //to fill the arrays containing light info

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
		if (isInsideFrustum(&call, camera) != CLIP_OUTSIDE) renderMeshWithMaterial(call.model, call.mesh, call.material); // inside frustum -> render
	}
	// render translucent list
	for (sRenderable call : translucent_list) {
		if (isInsideFrustum(&call, camera) != CLIP_OUTSIDE) renderMeshWithMaterial(call.model, call.mesh, call.material);
	}

}

void Renderer::renderPlain(Camera* light_cam, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material) 
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	shader = GFX::Shader::Get("plain");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;

	shader->enable();

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);

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
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
}

void Renderer::renderGBuffer(Camera* light_cam, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	shader = GFX::Shader::Get("fill_gbuffer");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;

	shader->enable();

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);

	material->bind(shader);

	//For the basic.vs
	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", light_cam->viewprojection_matrix);
	shader->setUniform("u_camera_pos", light_cam->eye);
	shader->setUniform("u_model", model);


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
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
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
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;

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

	// Upload light uniforms
	shader->setUniform("u_ambient_light", scene->ambient_light);
	shader->setUniform("u_num_lights", (int)lights_list.size());
	shader->setUniform("u_shininess", shininess);
	shader->setUniform3Array("u_light_pos", (float*)light_pos, MAX_LIGHTS);
	shader->setUniform1Array("u_intensity", light_intensity, MAX_LIGHTS);
	shader->setUniform3Array("u_light_color", (float*)light_color, MAX_LIGHTS);
	shader->setUniform1Array("u_light_type", light_type, MAX_LIGHTS);
	shader->setUniform3Array("u_light_front", (float*)light_front, MAX_LIGHTS);
	shader->setUniform2Array("u_light_cone", (float*)light_cone, MAX_LIGHTS);

	// Upload shadowmap uniform
	//fbo->depth_texture->toViewport();
	//shader->setUniform("u_shadowmap", fbo->depth_texture, 2);
	//shader->setUniform("u_shadow_vp", dir_cam->viewprojection_matrix);
	shader->setUniform("u_shadow_bias", shadow_bias);
	shader->setUniform("u_shadowmaps[0]", fbos[0]->depth_texture, 2);
	shader->setUniform("u_shadowmaps[1]", fbos[1]->depth_texture, 3);
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

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//add here your stuff
	//...
	ImGui::SliderFloat("Shininess", &shininess, 0.0f, 100.0f);
	//ImGui::SliderFloat("ShadowBias", &shadow_bias, 0.0001f, 0.001f);

}

#else
void Renderer::showUI() {}
#endif