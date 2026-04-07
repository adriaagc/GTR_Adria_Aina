cmake_minimum_required(VERSION 3.12)

function(COPY_FROM_TO_DIR src_dir dst_dir)
    file(GLOB src_files ${src_dir}/*)
    foreach(src_file IN LISTS src_files)
        get_filename_component(file_and_ext ${src_file} NAME)
        set(dst_file ${dst_dir}/${file_and_ext})
#        MESSAGE(STATUS "[[[COPY_FROM_TO_DIR]]] :\n   * ${src_file}\n   * ${dst_file}")
#        add_custom_command(COMMAND ${CMAKE_COMMAND} -E make_directory ${dst_dir} OUTPUT ${dst_dir})
        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${src_file} ${dst_file}
        )
    endforeach()
endfunction()

# Macro to map filters to folder structure for MSVC projects
macro(GroupSources curdir)
	file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)

    foreach(child ${children})
        if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
            GroupSources(${curdir}/${child})
        else()
            string(REPLACE "/" "\\" groupname ${curdir})
            source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        endif()
    endforeach()
endmacro()

# Directories
set(DIR_ROOT       ${CMAKE_CURRENT_LIST_DIR})
set(DIR_SOURCES    "${DIR_ROOT}/src")

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

project (GTR_Framework CXX)

find_package(OpenGL REQUIRED)

if(NOT TARGET OpenGL::GLU)
    message(FATAL_ERROR "GLU could not be found")
endif(NOT TARGET OpenGL::GLU)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(libraries/SDL EXCLUDE_FROM_ALL)
add_subdirectory(libraries/glew-cmake EXCLUDE_FROM_ALL)

# Sources
macro(CG_FILES_APPEND)
    file(GLOB FILES_APPEND CONFIGURE_DEPENDS ${ARGV})
    list(APPEND CG_SOURCES ${FILES_APPEND})
endmacro()
macro(CG_SOURCES_APPEND)
    CG_FILES_APPEND(${ARGV0}/*.h)
    CG_FILES_APPEND(${ARGV0}/*.cpp)
endmacro()

CG_SOURCES_APPEND(${DIR_SOURCES})
CG_SOURCES_APPEND(${DIR_SOURCES}/core)
CG_SOURCES_APPEND(${DIR_SOURCES}/extra)
CG_SOURCES_APPEND(${DIR_SOURCES}/extra/coldet)
CG_SOURCES_APPEND(${DIR_SOURCES}/extra/imgui)
CG_SOURCES_APPEND(${DIR_SOURCES}/gfx)
CG_SOURCES_APPEND(${DIR_SOURCES}/pipeline)
CG_SOURCES_APPEND(${DIR_SOURCES}/utils)

# ImGUI
include_directories(libraries/imgui/)
include_directories(libraries/imgui/backends/)
set(IMGUI_IMPL_SRC libraries/imgui/backends/imgui_impl_sdl3.cpp 
                   libraries/imgui/backends/imgui_impl_opengl3.cpp)
file(GLOB IMGUI_SRC "libraries/imgui/*.cpp")

include_directories(libraries/ImGuizmo/)
file(GLOB IMGUIZMO_SRC "libraries/ImGuizmo/*.cpp")

add_executable(${PROJECT_NAME} ${CG_SOURCES} ${IMGUI_IMPL_SRC} ${IMGUI_SRC} ${IMGUIZMO_SRC})

target_include_directories(${PROJECT_NAME} PUBLIC ${DIR_SOURCES})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${DIR_ROOT}")
set_target_properties(${PROJECT_NAME} PROPERTIES 
                      XCODE_GENERATE_SCHEME TRUE
                      XCODE_SCHEME_WORKING_DIRECTORY "${DIR_ROOT}/")

GroupSources(src)

# Ensure that _AMD64_ or _X86_ are defined on Microsoft Windows, as otherwise
# um/winnt.h provided since Windows 10.0.22000 will error.
if (WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-D_AMD64_)
        message(STATUS "64 bits detected")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_definitions(-D_X86_)
        message(STATUS "32 bits detected")
    endif()
endif()

if (APPLE)
    find_library(cocoa_lib Cocoa REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${cocoa_lib})
    if (CMAKE_GENERATOR STREQUAL Xcode)
	    target_compile_definitions(${PROJECT_NAME} PUBLIC "XCODE_IDE")
    endif()
endif()

# sdl2
include_directories(${PROJECT_NAME} libraries/SDL/include)

# glew
include_directories(${PROJECT_NAME} libraries/glew-cmake/include)

#opengl
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3 libglew_static OpenGL::GL OpenGL::GLU)

# Properties
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)

message(STATUS "dir root: ${DIR_ROOT}")
message(STATUS "bin root: ${CMAKE_BINARY_DIR}")