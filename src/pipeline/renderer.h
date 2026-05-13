#pragma once
#include "scene.h"
#include "prefab.h"

#include "light.h"

#define MAX_LIGHTS 4
#define SHADOW_RES 1024
#define MAX_SHADOWS 2

#define WIDTH 1024
#define HEIGHT 768


//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}

namespace SCN {

	class Prefab;
	class Material;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:

	//ENTITIES:
		struct sRenderable {
			GFX::Mesh* mesh = nullptr;
			Material* material = nullptr;
			Matrix44 model;
		};

		std::vector<sRenderable> opaque_list;
		std::vector<sRenderable> translucent_list;


	//LIGHTS:
		std::vector<LightEntity*> lights_list;
		Vector3f light_pos[MAX_LIGHTS];
		Vector3f light_color[MAX_LIGHTS];
		int light_type[MAX_LIGHTS];
		float light_intensity[MAX_LIGHTS];
		Vector3f light_front[MAX_LIGHTS];
		Vector2f light_cone[MAX_LIGHTS];

	//ImGui:
		float shininess = 20.0;
		float shadow_bias = 0.0001;
		bool ffc = true; // enable forward facing culling

		bool render_wireframe;
		bool render_boundaries;

	//FBOs & shadowmaps
		//GFX::FBO* fbo;
		//Camera* dir_cam;
		GFX::FBO* fbos[MAX_SHADOWS];
		Camera light_cameras[MAX_SHADOWS];
		Matrix44 shadow_vps[MAX_SHADOWS];

	//G_Buffer:
		GFX::FBO* gbuffer;

		GFX::Texture* skybox_cubemap;

		SCN::Scene* scene;

	//Light Volumes:
		GFX::FBO* lighting_FBO;


		//updated every frame
		Renderer(const char* shaders_atlas_filename );
		~Renderer();

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...

		void parseNode(Node* node);

		char isInsideFrustum(sRenderable* obj, Camera* camera);

		void fillLightArrays();

		void createLightCameras();

		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);
		void renderCook(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);

		//Phong with deferred rendering
		void renderQuadMesh(GFX::Mesh* mesh);
		void renderCookDeferred(GFX::Mesh* mesh);

		//to fill the framebuffer without drawing from the perspective of the light
		void renderPlain(Camera* light_cam, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);
		void renderGBuffer(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);

		//Light Volumes:
		void renderAmbient(GFX::Mesh* mesh);
		void renderLightVolume();
		void renderLightSpheres();

		void showUI();
	};

};