/*  by Javi Agenjo 2013 UPF  javi.agenjo@gmail.com
	This file has all the includes so the app works in different systems.
	It is a little bit low level so do not worry about the code.
*/

#ifndef INCLUDES_H
#define INCLUDES_H

//under windows we need this file to make opengl work
#ifdef WIN32 
	#include <windows.h>
#endif

#ifndef APIENTRY
    #define APIENTRY
#endif

#ifdef WIN32
	#define USE_GLEW
	#define GLEW_STATIC
	#include <GL/glew.h>
#endif

#define OPENGL_VERSION_MAJOR 3
#define OPENGL_VERSION_MINOR 0

//SDL
//#pragma comment(lib, "SDL2.lib")
//#pragma comment(lib, "SDL2main.lib")


#define GL_GLEXT_PROTOTYPES

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>


//GLUT
#ifdef WIN32
    //#include <GL/glext.h>
    #include "GL/GLU.h"
#endif

#ifdef __APPLE__
    #include <OpenGL/glext.h>
    #include "OpenGL/glu.h"
#endif

#include <iostream>

//remove warnings

//used to access opengl extensions
#define REGISTER_GLEXT(RET, FUNCNAME, ...) typedef RET ( * FUNCNAME ## _func)(__VA_ARGS__); FUNCNAME ## _func FUNCNAME = NULL; 
#define IMPORT_GLEXT(FUNCNAME) FUNCNAME = (FUNCNAME ## _func) SDL_GL_GetProcAddress(#FUNCNAME); if (FUNCNAME == NULL) { std::cout << "ERROR: This Graphics card doesnt support " << #FUNCNAME << std::endl; }


//OPENGL EXTENSIONS


//IMGUI
#ifndef SKIP_IMGUI
	//#define IMGUI_DEFINE_MATH_OPERATORS
	#include <imgui_internal.h>
	#include <imgui.h>
	#include <imgui_impl_sdl3.h>
	#include <imgui_impl_opengl3.h>
	
	#include <ImGuizmo.h> //imguizmo
	//#include "../extra/imgui/imfilebrowser.h"
#endif




#endif
