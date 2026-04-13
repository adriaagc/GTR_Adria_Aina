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
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}


void Renderer::parseNode(Node* node) { //obtain from each node of the sceen its mesh, material and position (model)
	if (!node) {
		return; //not analyze empty nodes
	}

	render_list.push_back({ // temporary sRenderable object. The dots allow us not to care about the order.
		.mesh = node->mesh,
		.material = node->material,
		.model = node->getGlobalMatrix()
		});

	for (Node* child : node->children) { //AQUÍ NO S'HAURIA DE COMPROBAR SI ESTÀ DINS EL FRUSTUM O NO???
		parseNode(child);
	}
}


void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam) {
	// HERE =====================
	// TODO: GENERATE RENDERABLES
	// ==========================
	render_list.clear(); // Clear the previous frame

	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		if (entity->getType() == eEntityType::PREFAB) {
			//
			PrefabEntity* e = (PrefabEntity*) entity;

			parseNode(&(entity->root));

		}

		if (entity->getType() == SCN::eEntityType::LIGHT) {
			LightEntity* light = (LightEntity*)entity;
			light_list.push_back({ // temporary sRenderable object. The dots allow us not to care about the order.
			.color = light->color,
			.position = light->root.model.getTranslation(),
			.intensity = light->intensity
			});
		}

		// Store Prefab Entitys
		// ...
		//		Store Children Prefab Entities

		// Store Lights
		// ...
	}
	
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera); // fill the render list

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);

	// HERE =====================
	// TODO: RENDER RENDERABLES
	// ==========================

	//POT SER QUE FALTI ORDENAR LA LLISTA??? APARTAT 3.4, diapo 31 (render front to back opaque objects; render back to front transparent objects; first transparent  objects and then opaque objects) 

	for (sRenderable call : render_list) { //for each element of the render_list
		renderMeshWithMaterial(call.model, call.mesh, call.material); //render the information of each node. 
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
	shader = GFX::Shader::Get("texture");

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
	shader->setUniform("u_camera_position", camera->eye);

	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t );

	//Només em falta passar al shader del phong la primera llum, i ara ho extenc a més d'una llum 
	SCN::Scene* scene = SCN::Scene::instance; //Creem una instancia de la scene. 
	
	//int num_lights = 0;
	
	//---------------
	/*if (scene->entities.size() > 0) {
		for (int i = 0; i < scene->entities.size(); ++i) {*/

				//num_lights++;
				//-----------
				//SCN::BaseEntity* first_light_ent = nullptr;
				//SCN::LightEntity* first_light = nullptr;

				//first_light_ent = scene->entities[i];
				//first_light = (SCN::LightEntity*)first_light_ent;
				//
				//shader->setUniform("u_light_position", first_light->root.model.getTranslation());
				//shader->setUniform("u_light_color", first_light->color);
				////break; // Un cop trobada la primera, surto del bucle. 
	/*		} 
		}
	}*/
	shader->setUniform("u_colors", light_colors);
	shader->setUniform("u_light_positions", light_positions);
	shader->setUniform("u_num_lights", num_lights);
	shader->setUniform("u_ambient_light", vec4(scene->ambient_light,1.0f)); //Ho he passat a vec4 per després al shader poder posar directament vec4 amb la hambient light. 


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
}

#else
void Renderer::showUI() {}
#endif