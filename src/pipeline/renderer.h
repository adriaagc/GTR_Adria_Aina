#pragma once // ensures the header file is included only once during compilation, preventing duplicate definitions.
#include "scene.h"
#include "prefab.h"

#include "light.h"

#define MAX_LIGHTS 5

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

		struct sRenderable {
			GFX::Mesh* mesh = nullptr; //the thing we want to render
			Material* material = nullptr;
			Matrix44 model; //where we want to render it
		};

		//std::vector<sRenderable> render_list; // collect everything to render and render it in a loop
		std::vector<sRenderable> opaque_list;
		std::vector<sRenderable> translucent_list;


		struct sLight {
			vec3 color;
			vec3 pos;
			float intensity;
			vec3 front;
			eLightType l_type;
			Vector2f cone_info;
		};

		std::vector<sLight> light_list;
		std::vector<LightEntity*> lights;

		Vector3f light_pos[MAX_LIGHTS];
		Vector3f light_color[MAX_LIGHTS];
		Vector3f light_front[MAX_LIGHTS];
		int light_type[MAX_LIGHTS];
		float light_intensity[MAX_LIGHTS];
		Vector2f light_cone[MAX_LIGHTS];

		bool render_wireframe;
		bool render_boundaries;
		bool isMultipass = false;
		float shininess = 20.0;

		GFX::Texture* skybox_cubemap;

		SCN::Scene* scene;

		SCN::Material* selected_material = nullptr; //creo una variable per guardar el material actual. 

		//shadow-maps
		GFX::Texture* shadow_map = nullptr;
		GFX::FBO* shadow_fbo = nullptr; //Buffer que es crea a la memòria de la GPU. Guarda el que "dibuixa" a una textura. 
		int shadow_map_resolution = 1024;
		Camera* light_camera;

		//updated every frame
		Renderer(const char* shaders_atlas_filename );
		//Destructor
		~Renderer();

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...

		char isInsideFrustum(sRenderable* obj, Camera* camera); // Tells if the object is inside the frustum or not

		void parseNode(Node* node);

		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);
		//void renderSkybox(GFX::Texture* cubemap);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);

		//to render lights
		void fillLightArrays(Vector3f* light_pos, Vector3f* light_color, float* light_intensity, Vector3f* light_font, int* light_type, Vector2f* ligh_cone);

		void renderPlain(Camera* light_cam, const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);

		void showUI();
	};

};