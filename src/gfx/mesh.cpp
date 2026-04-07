#include "mesh.h"
#include "../extra/textparser.h"
#include "../utils/utils.h"
#include "shader.h"
#include "../core/includes.h"
#define _USE_MATH_DEFINES
#include "math.h"
#include "gfx.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <sys/stat.h>

#include "../pipeline/camera.h" //??
#include "texture.h"
//#include "animation.h"
#include "../extra/coldet/coldet.h"

bool GFX::Mesh::use_binary = true;            //checks if there is .wbin, it there is one tries to read it instead of the other file
bool GFX::Mesh::auto_upload_to_vram = true;    //uploads the mesh to the GPU VRAM to speed up rendering
bool GFX::Mesh::interleave_meshes = true;    //places the geometry in an interleaved array

std::map<std::string, GFX::Mesh*> GFX::Mesh::sMeshesLoaded;
long GFX::Mesh::num_meshes_rendered = 0;
long GFX::Mesh::num_triangles_rendered = 0;

#define FORMAT_ASE 1
#define FORMAT_OBJ 2
#define FORMAT_MBIN 3
#define FORMAT_MESH 4



const vec3 corners[] = { {1,1,1},  {1,1,-1},  {1,-1,1},  {1,-1,-1},  {-1,1,1},  {-1,1,-1},  {-1,-1,1},  {-1,-1,-1} };

GFX::Mesh::Mesh()
{
    radius = 0;
    vertices_vbo_id = uvs_vbo_id = uvs1_vbo_id = normals_vbo_id = colors_vbo_id = interleaved_vbo_id = indices_vbo_id = bones_vbo_id = weights_vbo_id = 0;
    collision_model = NULL;
    clear();
}

GFX::Mesh::~Mesh()
{
    clear();
}

void GFX::Mesh::clear()
{
    //Free VBOs
    if (vertices_vbo_id)
        glDeleteBuffers(1, &vertices_vbo_id);
    if (uvs_vbo_id)
        glDeleteBuffers(1, &uvs_vbo_id);
    if (normals_vbo_id)
        glDeleteBuffers(1, &normals_vbo_id);
    if (colors_vbo_id)
        glDeleteBuffers(1, &colors_vbo_id);
    if (interleaved_vbo_id)
        glDeleteBuffers(1, &interleaved_vbo_id);
    if (indices_vbo_id)
        glDeleteBuffers(1, &indices_vbo_id);
    if (bones_vbo_id)
        glDeleteBuffers(1, &bones_vbo_id);
    if (weights_vbo_id)
        glDeleteBuffers(1, &weights_vbo_id);
    if (uvs1_vbo_id)
        glDeleteBuffers(1, &uvs1_vbo_id);

    //VBOs ids
    vertices_vbo_id = uvs_vbo_id = normals_vbo_id = colors_vbo_id = interleaved_vbo_id = indices_vbo_id = weights_vbo_id = bones_vbo_id = uvs1_vbo_id = 0;

    //buffers
    vertices.clear();
    normals.clear();
    uvs.clear();
    colors.clear();
    interleaved.clear();
    indices.clear();
    bones.clear();
    weights.clear();
    uvs1.clear();
}

int vertex_location = -1;
int normal_location = -1;
int uv_location = -1;
int color_location = -1;
int bones_location = -1;
int weights_location = -1;
int uv1_location = -1;

void GFX::Mesh::enableBuffers(GFX::Shader* sh)
{
    vertex_location = sh->getAttribLocation("a_vertex");
    assert(vertex_location != -1 && "No a_vertex found in shader");

    if (vertex_location == -1)
        return;

    int spacing = 0;
    int offset_normal = 0;
    int offset_uv = 0;

    if (interleaved.size())
    {
        spacing = sizeof(tInterleaved);
        offset_normal = sizeof(vec3);
        offset_uv = sizeof(vec3) + sizeof(vec3);
    }

    glBindVertexArray(interleaved_vao_id);

    if (vertices_vbo_id || interleaved_vbo_id)
    {
        glBindBuffer(GL_ARRAY_BUFFER, interleaved_vbo_id ? interleaved_vbo_id : vertices_vbo_id);
        checkGLErrors();
        glVertexAttribPointer(vertex_location, 3, GL_FLOAT, GL_FALSE, spacing, 0);
        checkGLErrors();
    }
    else
        glVertexAttribPointer(vertex_location, 3, GL_FLOAT, GL_FALSE, spacing, interleaved.size() ? &interleaved[0].vertex : &vertices[0]);

    glEnableVertexAttribArray(vertex_location);
    checkGLErrors();

    normal_location = -1;
    if (normals.size() || spacing)
    {
        normal_location = sh->getAttribLocation("a_normal");
        if (normal_location != -1)
        {
            glEnableVertexAttribArray(normal_location);
            if (normals_vbo_id || interleaved_vbo_id)
            {
                glBindBuffer(GL_ARRAY_BUFFER, interleaved_vbo_id ? interleaved_vbo_id : normals_vbo_id);
                glVertexAttribPointer(normal_location, 3, GL_FLOAT, GL_FALSE, spacing, (void*)offset_normal);
            }
            else
                glVertexAttribPointer(normal_location, 3, GL_FLOAT, GL_FALSE, spacing, interleaved.size() ? &interleaved[0].normal : &normals[0]);
        }
    }
    checkGLErrors();

    uv_location = -1;
    if (uvs.size() || spacing)
    {
        uv_location = sh->getAttribLocation("a_coord");
        if (uv_location != -1)
        {
            glEnableVertexAttribArray(uv_location);
            if (uvs_vbo_id || interleaved_vbo_id)
            {
                glBindBuffer(GL_ARRAY_BUFFER, interleaved_vbo_id ? interleaved_vbo_id : uvs_vbo_id);
                glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, spacing, (void*)offset_uv);
            }
            else
                glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, spacing, interleaved.size() ? &interleaved[0].uv : &uvs[0]);
        }
    }

    uv1_location = -1;
    if (uvs1.size())
    {
        uv1_location = sh->getAttribLocation("a_uv1");
        if (uv1_location != -1)
        {
            glEnableVertexAttribArray(uv1_location);
            if (uvs1_vbo_id)
            {
                glBindBuffer(GL_ARRAY_BUFFER, uvs1_vbo_id);
                glVertexAttribPointer(uv1_location, 2, GL_FLOAT, GL_FALSE, spacing, (void*)NULL);
            }
            else
                glVertexAttribPointer(uv1_location, 2, GL_FLOAT, GL_FALSE, spacing, &uvs1[0]);
        }
    }

    color_location = -1;
    if (colors.size())
    {
        color_location = sh->getAttribLocation("a_color");
        if (color_location != -1)
        {
            glEnableVertexAttribArray(color_location);
            if (colors_vbo_id)
            {
                glBindBuffer(GL_ARRAY_BUFFER, colors_vbo_id);
                glVertexAttribPointer(color_location, 4, GL_FLOAT, GL_FALSE, 0, NULL);
            }
            else
                glVertexAttribPointer(color_location, 4, GL_FLOAT, GL_FALSE, 0, &colors[0]);
        }
    }

    bones_location = -1;
    if (bones.size())
    {
        bones_location = sh->getAttribLocation("a_bones");
        if (bones_location != -1)
        {
            glEnableVertexAttribArray(bones_location);
            if (bones_vbo_id)
            {
                glBindBuffer(GL_ARRAY_BUFFER, bones_vbo_id);
                glVertexAttribPointer(bones_location, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, NULL);
            }
            else
                glVertexAttribPointer(bones_location, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, &bones[0]);
        }
    }
    weights_location = -1;
    if (weights.size())
    {
        weights_location = sh->getAttribLocation("a_weights");
        if (weights_location != -1)
        {
            glEnableVertexAttribArray(weights_location);
            if (weights_vbo_id)
            {
                glBindBuffer(GL_ARRAY_BUFFER, weights_vbo_id);
                glVertexAttribPointer(weights_location, 4, GL_FLOAT, GL_FALSE, 0, NULL);
            }
            else
                glVertexAttribPointer(weights_location, 4, GL_FLOAT, GL_FALSE, 0, &weights[0]);
        }
    }
}

void GFX::Mesh::render(unsigned int primitive, int submesh_id, int num_instances)
{
    Shader* shader = Shader::current;
    if (!shader || !shader->compiled)
    {
        assert(0 && "no shader or shader not compiled or enabled");
        return;
    }
    assert((interleaved.size() || vertices.size()) && "No vertices in this mesh");

    //bind buffers to attribute locations
    enableBuffers(shader);
    checkGLErrors();

    //draw call
    drawCall(primitive, submesh_id, num_instances);
    checkGLErrors();

    //unbind them
    disableBuffers(shader);
    checkGLErrors();
}

void GFX::Mesh::drawCall(unsigned int primitive, int draw_call_id, int num_instances)
{
    size_t start = 0; //in primitives
    size_t size = vertices.size();
    if (indices.size())
        size = indices.size();
    else if (interleaved.size())
        size = interleaved.size();

    //DRAW
    if (indices.size())
    {
        if (num_instances > 0)
        {
            assert(indices_vbo_id && "indices must be uploaded to the GPU");
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_id);
            glDrawElementsInstanced(primitive, size * 3, GL_UNSIGNED_INT, (void*)(start * sizeof(vec3)), num_instances);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        else
        {
            glBindVertexArray(interleaved_vao_id);

            if (indices_vbo_id)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_id);
                checkGLErrors();
                glDrawElements(primitive, size * 3, GL_UNSIGNED_INT, (void*)(start * sizeof(vec3)));
                checkGLErrors();
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
            else
                glDrawElements(primitive, size * 3, GL_UNSIGNED_INT, (void*)(&indices[0] + start)); //no multiply, its a vector3u pointer)

            glBindVertexArray(0);
        }
    }
    else
    {
        glBindVertexArray(interleaved_vao_id);

        if (num_instances > 0)
            glDrawArraysInstanced(primitive, start, size, num_instances);
        else
            glDrawArrays(primitive, start, size);

        glBindVertexArray(0);
    }

    num_triangles_rendered += static_cast<long>((size / 3) * (num_instances ? num_instances : 1));
    num_meshes_rendered++;
}

void GFX::Mesh::disableBuffers(GFX::Shader* shader)
{
    glBindVertexArray(interleaved_vao_id);
    glDisableVertexAttribArray(vertex_location);
    if (normal_location != -1) glDisableVertexAttribArray(normal_location);
    if (uv_location != -1) glDisableVertexAttribArray(uv_location);
    if (uv1_location != -1) glDisableVertexAttribArray(uv1_location);
    if (color_location != -1) glDisableVertexAttribArray(color_location);
    if (bones_location != -1) glDisableVertexAttribArray(bones_location);
    if (weights_location != -1) glDisableVertexAttribArray(weights_location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);    //if crashes here, COMMENT THIS LINE ****************************
    glBindVertexArray(0);
}

GLuint instances_buffer_id = 0;

//should be faster but in some system it is slower
void GFX::Mesh::renderInstanced(unsigned int primitive, const mat4* instanced_models, int num_instances)
{
    if (!num_instances)
        return;

    GFX::Shader* shader = GFX::Shader::current;
    assert(shader && "shader must be enabled");

    if (instances_buffer_id == 0)
        glGenBuffers(1, &instances_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, instances_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, num_instances * sizeof(mat4), instanced_models, GL_STREAM_DRAW);

    int attribLocation = shader->getAttribLocation("u_model");
    assert(attribLocation != -1 && "shader must have attribute mat4 u_model (not a uniform)");
    if (attribLocation == -1)
        return; //this shader doesnt support instanced model

    //mat4 count as 4 different attributes of vec4... (thanks opengl...)
    for (int k = 0; k < 4; ++k)
    {
        glEnableVertexAttribArray(attribLocation + k);
        int offset = sizeof(float) * 4 * k;
        const uint8_t* addr = (uint8_t*)offset;
        glVertexAttribPointer(attribLocation + k, 4, GL_FLOAT, false, sizeof(mat4), addr);
        glVertexAttribDivisor(attribLocation + k, 1); // This makes it instanced!
    }

    //regular render
    render(primitive, -1, num_instances);

    //disable instanced attribs
    for (int k = 0; k < 4; ++k)
    {
        glDisableVertexAttribArray(attribLocation + k);
        glVertexAttribDivisor(attribLocation + k, 0);
    }
}

void GFX::Mesh::renderInstanced(unsigned int primitive, const std::vector<vec3> positions, const char* uniform_name)
{
    if (!positions.size())
        return;
    int num_instances = positions.size();

    GFX::Shader* shader = GFX::Shader::current;
    assert(shader && "shader must be enabled");

    if (instances_buffer_id == 0)
        glGenBuffers(1, &instances_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, instances_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, num_instances * sizeof(vec3), &positions[0], GL_STREAM_DRAW);

    int attribLocation = shader->getAttribLocation(uniform_name);
    assert(attribLocation != -1 && "shader uniform not found");
    if (attribLocation == -1)
        return; //this shader doesnt have instanced uniform

    glEnableVertexAttribArray(attribLocation);
    glVertexAttribPointer(attribLocation, 3, GL_FLOAT, false, sizeof(vec3), 0);
    glVertexAttribDivisor(attribLocation, 1); // This makes it instanced!

    //regular render
    render(primitive, -1, num_instances);

    //disable instanced attribs
    glDisableVertexAttribArray(attribLocation);
    glVertexAttribDivisor(attribLocation, 0);
}


//super obsolete rendering method, do not use
void GFX::Mesh::renderFixedPipeline(int primitive)
{
    assert((vertices.size() || interleaved.size()) && "No vertices in this mesh");

    int interleave_offset = interleaved.size() ? sizeof(tInterleaved) : 0;
    int offset_normal = sizeof(vec3);
    int offset_uv = sizeof(vec3) + sizeof(vec3);

    glEnableClientState(GL_VERTEX_ARRAY);

    if (vertices_vbo_id || interleaved_vbo_id)
    {
        glBindBuffer(GL_ARRAY_BUFFER, interleave_offset ? interleaved_vbo_id : vertices_vbo_id);
        glVertexPointer(3, GL_FLOAT, interleave_offset, 0);
    }
    else
        glVertexPointer(3, GL_FLOAT, interleave_offset, interleave_offset ? &interleaved[0].vertex : &vertices[0]);

    if (normals.size() || interleave_offset)
    {
        glEnableClientState(GL_NORMAL_ARRAY);
        if (normals_vbo_id || interleaved_vbo_id)
        {
            glBindBuffer(GL_ARRAY_BUFFER, interleaved_vbo_id ? interleaved_vbo_id : normals_vbo_id);
            glNormalPointer(GL_FLOAT, interleave_offset, (void*)offset_normal);
        }
        else
            glNormalPointer(GL_FLOAT, interleave_offset, interleave_offset ? &interleaved[0].normal : &normals[0]);
    }

    if (uvs.size() || interleave_offset)
    {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        if (uvs_vbo_id || interleaved_vbo_id)
        {
            glBindBuffer(GL_ARRAY_BUFFER, interleaved_vbo_id ? interleaved_vbo_id : uvs_vbo_id);
            glTexCoordPointer(2, GL_FLOAT, interleave_offset, (void*)offset_uv);
        }
        else
            glTexCoordPointer(2, GL_FLOAT, interleave_offset, interleave_offset ? &interleaved[0].uv : &uvs[0]);
    }

    if (colors.size())
    {
        glEnableClientState(GL_COLOR_ARRAY);
        if (colors_vbo_id)
        {
            glBindBuffer(GL_ARRAY_BUFFER, colors_vbo_id);
            glColorPointer(4, GL_FLOAT, 0, NULL);
        }
        else
            glColorPointer(4, GL_FLOAT, 0, &colors[0]);
    }

    int size = (int)vertices.size();
    if (!size)
        size = (int)interleaved.size();

    glDrawArrays(primitive, 0, (GLsizei)size);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (normals.size())
        glDisableClientState(GL_NORMAL_ARRAY);
    if (uvs.size())
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (colors.size())
        glDisableClientState(GL_COLOR_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0); //if it crashes, comment this line
}

// TODO
//void GFX::Mesh::renderAnimated(unsigned int primitive, Skeleton* skeleton)
//{
//    if (skeleton)
//    {
//        GFX::Shader* shader = GFX::Shader::current;
//        std::vector<Matrix44> bone_matrices;
//        assert(bones.size());
//        int bones_loc = shader->getUniformLocation("u_bones");
//        if (bones_loc != -1)
//        {
//            skeleton->computeFinalBoneMatrices(bone_matrices, this);
//            shader->setUniform("u_bones", bone_matrices);
//        }
//    }
//
//    render(primitive);
//}

void GFX::Mesh::uploadToVRAM()
{
    assert(vertices.size() || interleaved.size());

    if (glGenBuffers == 0)
    {
        std::cout << "Error: your graphics cards dont support VBOs. Sorry." << std::endl;
        exit(0);
    }

    glGenVertexArrays(1, &interleaved_vao_id);
    //glBindVertexArray(interleaved_vao_id);
    if (interleaved.size())
    {
        // Vertex,Normal,UV
        if (interleaved_vbo_id == 0)
            glGenBuffers(1, &interleaved_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, interleaved_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(tInterleaved), &interleaved[0], GL_STATIC_DRAW);
    }
    else
    {
        // Vertices
        if (vertices_vbo_id == 0)
            glGenBuffers(1, &vertices_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), &vertices[0], GL_STATIC_DRAW);

        // UVs
        if (uvs.size())
        {
            if (uvs_vbo_id == 0)
                glGenBuffers(1, &uvs_vbo_id);
            glBindBuffer(GL_ARRAY_BUFFER, uvs_vbo_id);
            glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(vec2), &uvs[0], GL_STATIC_DRAW);
        }

        // Normals
        if (normals.size())
        {
            if (normals_vbo_id == 0)
                glGenBuffers(1, &normals_vbo_id);
            glBindBuffer(GL_ARRAY_BUFFER, normals_vbo_id);
            glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(vec3), &normals[0], GL_STATIC_DRAW);
        }
    }

    // UVs
    if (uvs1.size())
    {
        if (uvs1_vbo_id == 0)
            glGenBuffers(1, &uvs1_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, uvs1_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, uvs1.size() * sizeof(vec2), &uvs1[0], GL_STATIC_DRAW);
    }

    // Colors
    if (colors.size())
    {
        if (colors_vbo_id == 0)
            glGenBuffers(1, &colors_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, colors_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(vec4), &colors[0], GL_STATIC_DRAW);
    }

    if (bones.size())
    {
        if (bones_vbo_id == 0)
            glGenBuffers(1, &bones_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, bones_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, bones.size() * sizeof(Vector4ub), &bones[0], GL_STATIC_DRAW);
    }
    if (weights.size())
    {
        if (weights_vbo_id == 0)
            glGenBuffers(1, &weights_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, weights_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, weights.size() * sizeof(vec4), &weights[0], GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Indices
    if (indices.size())
    {
        if (indices_vbo_id == 0)
            glGenBuffers(1, &indices_vbo_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    //glBindVertexArray(0);

    checkGLErrors();

    //clear buffers to save memory
}

bool GFX::Mesh::interleaveBuffers()
{
    if (!vertices.size() || !normals.size() || !uvs.size())
        return false;

    assert(vertices.size() == normals.size() && normals.size() == uvs.size());

    interleaved.resize(vertices.size());

    for (unsigned int i = 0; i < vertices.size(); ++i)
    {
        interleaved[i].vertex = vertices[i];
        interleaved[i].normal = normals[i];
        interleaved[i].uv = uvs[i];
    }

    vertices.resize(0);
    normals.resize(0);
    uvs.resize(0);

    return true;
}

struct sMeshInfo
{
    int version = 0;
    int header_bytes = 0;
    size_t size = 0;
    size_t nuindices = 0;
    vec3 aabb_min;
    vec3 aabb_max;
    vec3 center;
    vec3 halfsize;
    float radius = 0.0;
    size_t num_bones = 0;
    size_t num_submeshes = 0;
    mat4 bind_matrix;
    char streams[8]; //Vertex/Interlaved|Normal|Uvs|Color|Indices|Bones|Weights|Extra|Uvs1
    char extra[32]; //unused
};

bool GFX::Mesh::readBin(const char* filename)
{
    FILE* f;
    assert(filename);

    struct stat stbuffer;

    stat(filename, &stbuffer);
    f = fopen(filename, "rb");
    if (f == NULL)
        return false;

    unsigned int size = (unsigned int)stbuffer.st_size;
    char* data = new char[size];
    fread(data, size, 1, f);
    fclose(f);

    //watermark
    if (memcmp(data, "MBIN", 4) != 0)
    {
        std::cout << "[ERROR] loading BIN: invalid content: " << filename << std::endl;
        return false;
    }

    char* pos = data + 4;
    sMeshInfo info;
    memcpy(&info, pos, sizeof(sMeshInfo));
    pos += sizeof(sMeshInfo);

    if (info.version != MESH_BIN_VERSION || info.header_bytes != sizeof(sMeshInfo))
    {
        std::cout << "[WARN] loading BIN: old version: " << filename << std::endl;
        return false;
    }

    if (info.streams[0] == 'I')
    {
        interleaved.resize(info.size);
        memcpy((void*)&interleaved[0], pos, sizeof(tInterleaved) * info.size);
        pos += sizeof(tInterleaved) * info.size;
    }
    else if (info.streams[0] == 'V')
    {
        vertices.resize(info.size);
        memcpy((void*)&vertices[0], pos, sizeof(Vector3f) * info.size);
        pos += sizeof(Vector3f) * info.size;
    }

    if (info.streams[1] == 'N')
    {
        normals.resize(info.size);
        memcpy((void*)&normals[0], pos, sizeof(Vector3f) * info.size);
        pos += sizeof(Vector3f) * info.size;
    }

    if (info.streams[2] == 'U')
    {
        uvs.resize(info.size);
        memcpy((void*)&uvs[0], pos, sizeof(Vector2f) * info.size);
        pos += sizeof(Vector2f) * info.size;
    }

    if (info.streams[3] == 'C')
    {
        colors.resize(info.size);
        memcpy((void*)&colors[0], pos, sizeof(Vector4f) * info.size);
        pos += sizeof(Vector4f) * info.size;
    }

    if (info.streams[4] == 'I')
    {
        indices.resize(info.nuindices);
        memcpy((void*)&indices[0], pos, sizeof(unsigned int) * info.nuindices);
        pos += sizeof(Vector3u) * info.nuindices;
    }

    if (info.streams[5] == 'B')
    {
        bones.resize(info.size);
        memcpy((void*)&bones[0], pos, sizeof(Vector4ub) * info.size);
        pos += sizeof(Vector4ub) * info.size;
    }

    if (info.streams[6] == 'W')
    {
        weights.resize(info.size);
        memcpy((void*)&weights[0], pos, sizeof(Vector4f) * info.size);
        pos += sizeof(Vector4f) * info.size;
    }

    if (info.streams[7] == 'u')
    {
        uvs1.resize(info.size);
        memcpy((void*)&uvs1[0], pos, sizeof(Vector2f) * info.size);
        pos += sizeof(Vector2f) * info.size;
    }

    if (info.num_bones)
    {
        bones_info.resize(info.num_bones);
        memcpy((void*)&bones_info[0], pos, sizeof(BoneInfo) * info.num_bones);
        pos += sizeof(BoneInfo) * info.num_bones;
    }

    aabb_max = info.aabb_max;
    aabb_min = info.aabb_min;
    box.center = info.center;
    box.halfsize = info.halfsize;
    radius = info.radius;
    bind_matrix = info.bind_matrix;

    submeshes.resize(info.num_submeshes);
    memcpy(&submeshes[0], pos, sizeof(sSubmeshInfo) * info.num_submeshes);
    pos += sizeof(sSubmeshInfo) * info.num_submeshes;

    createCollisionModel();
    return true;
}

bool GFX::Mesh::writeBin(const char* filename)
{
    assert(vertices.size() || interleaved.size());
    std::string s_filename = filename;
    s_filename += ".mbin";

    FILE* f = fopen(s_filename.c_str(), "wb");
    if (f == NULL)
    {
        std::cout << "[ERROR] cannot write mesh BIN: " << s_filename.c_str() << std::endl;
        return false;
    }

    //watermark
    fwrite("MBIN", sizeof(char), 4, f);

    sMeshInfo info;
    memset(&info, 0, sizeof(info));
    info.version = MESH_BIN_VERSION;
    info.header_bytes = sizeof(sMeshInfo);
    info.size = interleaved.size() ? interleaved.size() : vertices.size();
    info.nuindices = indices.size();
    info.aabb_max = aabb_max;
    info.aabb_min = aabb_min;
    info.center = box.center;
    info.halfsize = box.halfsize;
    info.radius = radius;
    info.num_bones = bones_info.size();
    info.bind_matrix = bind_matrix;
    info.num_submeshes = submeshes.size();

    info.streams[0] = interleaved.size() ? 'I' : 'V';
    info.streams[1] = normals.size() ? 'N' : ' ';
    info.streams[2] = uvs.size() ? 'U' : ' ';
    info.streams[3] = colors.size() ? 'C' : ' ';
    info.streams[4] = indices.size() ? 'I' : ' ';
    info.streams[5] = bones.size() ? 'B' : ' ';
    info.streams[6] = weights.size() ? 'W' : ' ';
    info.streams[7] = uvs1.size() ? 'u' : ' '; //uv second set

    //write info
    fwrite((void*)&info, sizeof(sMeshInfo), 1, f);

    //write streams
    if (interleaved.size())
        fwrite((void*)&interleaved[0], interleaved.size() * sizeof(tInterleaved), 1, f);
    else
    {
        fwrite((void*)&vertices[0], vertices.size() * sizeof(Vector3f), 1, f);
        if (normals.size())
            fwrite((void*)&normals[0], normals.size() * sizeof(Vector3f), 1, f);
        if (uvs.size())
            fwrite((void*)&uvs[0], uvs.size() * sizeof(Vector2f), 1, f);
    }

    if (colors.size())
        fwrite((void*)&colors[0], colors.size() * sizeof(Vector4f), 1, f);

    if (indices.size())
        fwrite((void*)&indices[0], indices.size() * sizeof(unsigned int), 1, f);

    if (bones.size())
        fwrite((void*)&bones[0], bones.size() * sizeof(Vector4ub), 1, f);
    if (weights.size())
        fwrite((void*)&weights[0], weights.size() * sizeof(Vector4f), 1, f);
    if (bones_info.size())
        fwrite((void*)&bones_info[0], bones_info.size() * sizeof(BoneInfo), 1, f);
    if (uvs1.size())
        fwrite((void*)&uvs1[0], uvs1.size() * sizeof(Vector2f), 1, f);

    fwrite((void*)&submeshes[0], submeshes.size() * sizeof(sSubmeshInfo), 1, f);

    fclose(f);
    return true;
}

void parseFromText(vec3& v, const char* text, const char separator)
{
    int pos = 0;
    char num[255];
    const char* start = text;
    const char* current = text;

    while (1)
    {
        if (*current == separator || (*current == '\0' && current != text))
        {
            strncpy(num, start, current - start);
            num[current - start] = '\0';
            start = current + 1;
            if (num[0] != 'x')
                switch (pos)
                {
                case 0: v.x = (float)atof(num); break;
                case 1: v.y = (float)atof(num); break;
                case 2: v.z = (float)atof(num); break;
                default: return; break;
                }

            ++pos;
            if (*current == '\0')
                break;
        }

        ++current;
    }
};

bool GFX::Mesh::loadOBJ(const char* filename)
{
    std::string data;
    if (!readFile(filename, data))
        return false;
    char* pos = &data[0];
    char line[255];
    int i = 0;

    std::vector<Vector3f> indexed_positions;
    std::vector<Vector3f> indexed_normals;
    std::vector<Vector2f> indexed_uvs;

    const float max_float = 10000000;
    const float min_float = -10000000;
    aabb_min.set(max_float, max_float, max_float);
    aabb_max.set(min_float, min_float, min_float);

    unsigned int vertex_i = 0;

    sSubmeshInfo submesh_info;
    int last_submesh_vertex = 0;
    memset(&submesh_info, 0, sizeof(submesh_info));

    //parse file
    while (*pos != 0)
    {
        if (*pos == '\n') pos++;
        if (*pos == '\r') pos++;

        //read one line
        i = 0;
        while (i < 255 && pos[i] != '\n' && pos[i] != '\r' && pos[i] != 0) i++;
        memcpy(line, pos, i);
        line[i] = 0;
        pos = pos + i;

        //std::cout << "Line: \"" << line << "\"" << std::endl;
        if (*line == '#' || *line == 0) continue; //comment

        //tokenize line
        std::vector<std::string> tokens = tokenize(line, " ");

        if (tokens.empty()) continue;

        if (tokens[0] == "v" && tokens.size() == 4)
        {
            Vector3f v((float)atof(tokens[1].c_str()), (float)atof(tokens[2].c_str()), (float)atof(tokens[3].c_str()));
            indexed_positions.push_back(v);

            aabb_min.setMin(v);
            aabb_max.setMax(v);
        }
        else if (tokens[0] == "vt" && tokens.size() >= 3)
        {
            Vector2f v((float)atof(tokens[1].c_str()), 1.0 - (float)atof(tokens[2].c_str()));
            indexed_uvs.push_back(v);
        }
        else if (tokens[0] == "vn" && tokens.size() == 4)
        {
            Vector3f v((float)atof(tokens[1].c_str()), (float)atof(tokens[2].c_str()), (float)atof(tokens[3].c_str()));
            indexed_normals.push_back(v);
        }
        else if (tokens[0] == "s") //surface? it appears one time before the faces
        {
            //process mesh: ????
            //if (uvs.size() == 0 && indexed_uvs.size() )
            //    uvs.resize(1);
        }
        else if (tokens[0] == "usemtl") //surface? it appears one time before the faces
        {
            if (last_submesh_vertex != vertices.size())
            {
                submesh_info.length = vertices.size() - submesh_info.start;
                last_submesh_vertex = vertices.size();
                submeshes.push_back(submesh_info);
                memset(&submesh_info, 0, sizeof(submesh_info));
                strcpy(submesh_info.name, tokens[1].c_str());
                submesh_info.start = last_submesh_vertex;
            }
            else
                strcpy(submesh_info.material, tokens[1].c_str());
        }
        else if (tokens[0] == "g") //surface? it appears one time before the faces
        {
            if (last_submesh_vertex != vertices.size())
            {
                submesh_info.length = vertices.size() - submesh_info.start;
                last_submesh_vertex = vertices.size();
                submeshes.push_back(submesh_info);
                memset(&submesh_info, 0, sizeof(submesh_info));
                strcpy(submesh_info.name, tokens[1].c_str());
                submesh_info.start = last_submesh_vertex;
            }
        }
        else if (tokens[0] == "f" && tokens.size() >= 4)
        {
            Vector3f v1, v2, v3;
            v1.parseFromText(tokens[1].c_str(), '/');

            for (unsigned int iPoly = 2; iPoly < tokens.size() - 1; iPoly++)
            {
                v2.parseFromText(tokens[iPoly].c_str(), '/');
                v3.parseFromText(tokens[iPoly + 1].c_str(), '/');

                vertices.push_back(indexed_positions[(unsigned int)(v1.x) - 1]);
                vertices.push_back(indexed_positions[(unsigned int)(v2.x) - 1]);
                vertices.push_back(indexed_positions[(unsigned int)(v3.x) - 1]);
                //triangles.push_back( VECTOR_INDICES_TYPE(vertex_i, vertex_i+1, vertex_i+2) ); //not needed
                vertex_i += 3;

                if (indexed_uvs.size() > 0)
                {
                    uvs.push_back(indexed_uvs[(unsigned int)(v1.y) - 1]);
                    uvs.push_back(indexed_uvs[(unsigned int)(v2.y) - 1]);
                    uvs.push_back(indexed_uvs[(unsigned int)(v3.y) - 1]);
                }

                if (indexed_normals.size() > 0)
                {
                    normals.push_back(indexed_normals[(unsigned int)(v1.z) - 1]);
                    normals.push_back(indexed_normals[(unsigned int)(v2.z) - 1]);
                    normals.push_back(indexed_normals[(unsigned int)(v3.z) - 1]);
                }
            }
        }
    }

    box.center = (aabb_max + aabb_min) * 0.5f;
    box.halfsize = (aabb_max - box.center);
    radius = (float)fmax(aabb_max.length(), aabb_min.length());

    submesh_info.length = vertices.size() - last_submesh_vertex;
    submeshes.push_back(submesh_info);
    return true;
}

bool GFX::Mesh::loadMESH(const char* filename)
{
    struct stat stbuffer;

    FILE* f = fopen(filename, "rb");
    if (f == NULL)
    {
        std::cerr << "File not found: " << filename << std::endl;
        return false;
    }
    stat(filename, &stbuffer);

    unsigned int size = stbuffer.st_size;
    char* data = new char[size + 1];
    fread(data, size, 1, f);
    fclose(f);
    data[size] = 0;
    char* pos = data;
    char word[255];

    while (*pos)
    {
        char type = *pos;
        pos++;
        if (type == '-') //buffer
        {
            pos = fetchWord(pos, word);
            std::string str(word);
            if (str == "vertices")
                pos = fetchBufferVec3(pos, vertices);
            else if (str == "normals")
                pos = fetchBufferVec3(pos, normals);
            else if (str == "coords")
                pos = fetchBufferVec2(pos, uvs);
            else if (str == "colors")
                pos = fetchBufferVec4(pos, colors);
            else if (str == "bone_indices")
                pos = fetchBufferVec4ub(pos, bones);
            else if (str == "weights")
                pos = fetchBufferVec4(pos, weights);
            else
                pos = fetchEndLine(pos);
        }
        else if (type == '*') //buffer
        {
            pos = fetchWord(pos, word);
            pos = fetchBufferVec3u(pos, indices);
        }
        else if (type == '@') //info
        {
            pos = fetchWord(pos, word);
            std::string str(word);
            if (str == "bones")
            {
                pos = fetchWord(pos, word);
                bones_info.resize(static_cast<size_t>(atof(word)));
                for (int j = 0; j < bones_info.size(); ++j)
                {
                    pos = fetchWord(pos, word);
                    strcpy(bones_info[j].name, word);
                    pos = fetchMatrix44(pos, bones_info[j].bind_pose);
                }
            }
            else if (str == "bind_matrix")
                pos = fetchMatrix44(pos, bind_matrix);
            else
                pos = fetchEndLine(pos);
        }
        else
            pos = fetchEndLine(pos);
    }

    delete[] data;

    return true;
}

void GFX::Mesh::createCube()
{
    const float _verts[] = { -1, 1, -1, -1, -1, +1, -1, 1, 1,    -1, 1, -1, -1, -1, -1, -1, -1, +1,     1, 1, -1,  1, 1, 1,  1, -1, +1,     1, 1, -1,   1, -1, +1,   1, -1, -1,    -1, 1, 1,  1, -1, 1,  1, 1, 1,    -1, 1, 1, -1,-1,1,  1, -1, 1,    -1,1,-1, 1,1,-1,  1,-1,-1,   -1,1,-1, 1,-1,-1, -1,-1,-1,   -1,1,-1, 1,1,1, 1,1,-1,    -1,1,-1, -1,1,1, 1,1,1,    -1,-1,-1, 1,-1,-1, 1,-1,1,   -1,-1,-1, 1,-1,1, -1,-1,1 };
    const float _uvs[] = { 0,  1, 1, 0, 1, 1,                      0, 1,       0,  0,      1,  0,        0, 1,      1, 1,      1, 0,         0, 1,        1, 0,        0, 0,          0, 1, 1, 0, 1, 1,               0, 1,  0, 0,  1,  0,              0,1,  1,1, 1,0,              0,1,    1,0,    0,0,           0,0, 1,1, 1,0,           0,0,    0,1,   1,1,        0,0, 1,0, 1,1,              0,0, 1,1, 0,1 };

    vertices.resize(6 * 2 * 3);
    uvs.resize(6 * 2 * 3);
    memcpy(&vertices[0], _verts, sizeof(vec3) * vertices.size());
    memcpy(&uvs[0], _uvs, sizeof(vec2) * uvs.size());

    box.center = vec3(0, 0, 0);
    box.halfsize = vec3(1, 1, 1);
    radius = (float)box.halfsize.length();

    updateBoundingBox();
    uploadToVRAM();
}

void GFX::Mesh::createWireBox()
{
    const float _verts[] = { -1,-1,-1,  1,-1,-1,  -1,1,-1,  1,1,-1, -1,-1,1,  1,-1,1, -1,1,1,  1,1,1,    -1,-1,-1, -1,1,-1, 1,-1,-1, 1,1,-1, -1,-1,1, -1,1,1, 1,-1,1, 1,1,1,   -1,-1,-1, -1,-1,1, 1,-1,-1, 1,-1,1, -1,1,-1, -1,1,1, 1,1,-1, 1,1,1 };
    vertices.resize(24);
    memcpy(&vertices[0], _verts, sizeof(vec3) * vertices.size());

    box.center = vec3(0, 0, 0);
    box.halfsize = vec3(1, 1, 1);
    radius = (float)box.halfsize.length();
}

void GFX::Mesh::createQuad(float center_x, float center_y, float w, float h, bool flip_uvs)
{
    vertices.clear();
    normals.clear();
    uvs.clear();
    colors.clear();

    //create six vertices (3 for upperleft triangle and 3 for lowerright)

    vertices.push_back(vec3(center_x + w * 0.5f, center_y + h * 0.5f, 0.0f));
    vertices.push_back(vec3(center_x - w * 0.5f, center_y - h * 0.5f, 0.0f));
    vertices.push_back(vec3(center_x + w * 0.5f, center_y - h * 0.5f, 0.0f));
    vertices.push_back(vec3(center_x - w * 0.5f, center_y + h * 0.5f, 0.0f));
    vertices.push_back(vec3(center_x - w * 0.5f, center_y - h * 0.5f, 0.0f));
    vertices.push_back(vec3(center_x + w * 0.5f, center_y + h * 0.5f, 0.0f));

    //texture coordinates
    uvs.push_back(vec2(1.0f, flip_uvs ? 0.0f : 1.0f));
    uvs.push_back(vec2(0.0f, flip_uvs ? 1.0f : 0.0f));
    uvs.push_back(vec2(1.0f, flip_uvs ? 1.0f : 0.0f));
    uvs.push_back(vec2(0.0f, flip_uvs ? 0.0f : 1.0f));
    uvs.push_back(vec2(0.0f, flip_uvs ? 1.0f : 0.0f));
    uvs.push_back(vec2(1.0f, flip_uvs ? 0.0f : 1.0f));

    //all of them have the same normal
    normals.push_back(vec3(0.0f, 0.0f, 1.0f));
    normals.push_back(vec3(0.0f, 0.0f, 1.0f));
    normals.push_back(vec3(0.0f, 0.0f, 1.0f));
    normals.push_back(vec3(0.0f, 0.0f, 1.0f));
    normals.push_back(vec3(0.0f, 0.0f, 1.0f));
    normals.push_back(vec3(0.0f, 0.0f, 1.0f));
}


void GFX::Mesh::createPlane(float size)
{
    vertices.clear();
    normals.clear();
    uvs.clear();
    colors.clear();

    //create six vertices (3 for upperleft triangle and 3 for lowerright)

    vertices.push_back(vec3(size, 0, size));
    vertices.push_back(vec3(size, 0, -size));
    vertices.push_back(vec3(-size, 0, -size));
    vertices.push_back(vec3(-size, 0, size));
    vertices.push_back(vec3(size, 0, size));
    vertices.push_back(vec3(-size, 0, -size));

    //all of them have the same normal
    normals.push_back(vec3(0, 1, 0));
    normals.push_back(vec3(0, 1, 0));
    normals.push_back(vec3(0, 1, 0));
    normals.push_back(vec3(0, 1, 0));
    normals.push_back(vec3(0, 1, 0));
    normals.push_back(vec3(0, 1, 0));

    //texture coordinates
    uvs.push_back(vec2(1, 1));
    uvs.push_back(vec2(1, 0));
    uvs.push_back(vec2(0, 0));
    uvs.push_back(vec2(0, 1));
    uvs.push_back(vec2(1, 1));
    uvs.push_back(vec2(0, 0));

    box.center = vec3(0, 0, 0);
    box.halfsize = vec3(size, 0, size);
    radius = (float)box.halfsize.length();
}

void GFX::Mesh::createSubdividedPlane(float size, int subdivisions, bool centered)
{
    float isize = static_cast<float>(size / (double)(subdivisions));
    //float hsize = centered ? size * -0.5f : 0.0f;
    float iuv = static_cast<float>(1 / (double)(subdivisions * size));
    float sub_size = 1.0f / subdivisions;
    vertices.clear();

    for (int x = 0; x < subdivisions; ++x)
    {
        for (int z = 0; z < subdivisions; ++z)
        {

            vec2 offset(sub_size * z, sub_size * x);
            vec3 offset2(isize * x, 0.0f, isize * z);

            vertices.push_back(vec3(isize, 0.0f, isize) + offset2);
            vertices.push_back(vec3(isize, 0.0f, 0.0f) + offset2);
            vertices.push_back(vec3(0.0f, 0.0f, 0.0f) + offset2);

            uvs.push_back(vec2(sub_size, sub_size) + offset);
            uvs.push_back(vec2(0.0f, sub_size) + offset);
            uvs.push_back(vec2(0.0f, 0.0f) + offset);

            vertices.push_back(vec3(isize, 0.0f, isize) + offset2);
            vertices.push_back(vec3(0.0f, 0.0f, 0.0f) + offset2);
            vertices.push_back(vec3(0.0f, 0.0f, isize) + offset2);

            uvs.push_back(vec2(sub_size, sub_size) + offset);
            uvs.push_back(vec2(0.0f, 0.0f) + offset);
            uvs.push_back(vec2(sub_size, 0.0f) + offset);
        }
    }
    if (centered)
        box.center = vec3(0.0f, 0.0f, 0.0f);
    else
        box.center = vec3(size * 0.5f, 0.0f, size * 0.5f);

    box.halfsize = vec3(size * 0.5f, 0.0f, size * 0.5f);
    radius = static_cast<float>(box.halfsize.length());
}

void GFX::Mesh::createGrid(float dist)
{
    int num_lines = 2000;
    vec4 color(0.5f, 0.5f, 0.5f, 1.0f);

    for (float i = num_lines * -0.5f; i <= num_lines * 0.5f; ++i)
    {
        vertices.push_back(vec3(i * dist, 0.0f, dist * num_lines * -0.5f));
        vertices.push_back(vec3(i * dist, 0.0f, dist * num_lines * +0.5f));
        vertices.push_back(vec3(dist * num_lines * 0.5f, 0.0f, i * dist));
        vertices.push_back(vec3(dist * num_lines * -0.5f, 0.0f, i * dist));

        vec4 color = int(i) % 10 == 0 ? vec4(1.0f, 1.0f, 1.0f, 1.0f) : vec4(0.75f, 0.75f, 0.75f, 0.5f);
        colors.push_back(color);
        colors.push_back(color);
        colors.push_back(color);
        colors.push_back(color);
    }

    uploadToVRAM();
}

void GFX::Mesh::updateBoundingBox()
{
    if (vertices.size())
    {
        aabb_max = aabb_min = vertices[0];
        for (int i = 1; i < vertices.size(); ++i)
        {
            //aabb_min.setMin(vertices[i]);
            if (vertices[i].x < aabb_min.x) aabb_min.x = vertices[i].x;
            if (vertices[i].y < aabb_min.y) aabb_min.y = vertices[i].y;
            if (vertices[i].z < aabb_min.z) aabb_min.z = vertices[i].z;

            //aabb_max.setMax(vertices[i]);
            if (vertices[i].x > aabb_max.x) aabb_max.x = vertices[i].x;
            if (vertices[i].y > aabb_max.y) aabb_max.y = vertices[i].y;
            if (vertices[i].z > aabb_max.z) aabb_max.z = vertices[i].z;
        }
    }
    else if (interleaved.size())
    {
        aabb_max = aabb_min = interleaved[0].vertex;
        for (int i = 1; i < interleaved.size(); ++i)
        {
            //aabb_min.setMin(interleaved[i].vertex);
            if (interleaved[i].vertex.x < aabb_min.x) aabb_min.x = interleaved[i].vertex.x;
            if (interleaved[i].vertex.y < aabb_min.y) aabb_min.y = interleaved[i].vertex.y;
            if (interleaved[i].vertex.z < aabb_min.z) aabb_min.z = interleaved[i].vertex.z;

            //aabb_max.setMax(interleaved[i].vertex);
            if (interleaved[i].vertex.x > aabb_max.x) aabb_max.x = interleaved[i].vertex.x;
            if (interleaved[i].vertex.y > aabb_max.y) aabb_max.y = interleaved[i].vertex.y;
            if (interleaved[i].vertex.z > aabb_max.z) aabb_max.z = interleaved[i].vertex.z;
        }
    }
    box.center = (aabb_max + aabb_min) * 0.5f;
    box.halfsize = aabb_max - box.center;
}

GFX::Mesh* wire_box = NULL;

void GFX::Mesh::renderBounding(const Matrix44& model, bool world_bounding)
{
    if (!wire_box)
    {
        wire_box = new Mesh();
        wire_box->createWireBox();
        wire_box->uploadToVRAM();
    }

    Shader* sh = Shader::getDefaultShader("flat");
    sh->enable();
    sh->setUniform("u_viewprojection", Camera::current->viewprojection_matrix);

    Matrix44 matrix;
    matrix.translate(box.center.x, box.center.y, box.center.z);
    matrix.scale(box.halfsize.x, box.halfsize.y, box.halfsize.z);

    sh->setUniform("u_color", Vector4f(1, 1, 0, 1));
    sh->setUniform("u_model", matrix * model);
    wire_box->render(GL_LINES);

    if (world_bounding)
    {
        BoundingBox AABB = transformBoundingBox(model, box);
        matrix.setIdentity();
        matrix.translate(AABB.center.x, AABB.center.y, AABB.center.z);
        matrix.scale(AABB.halfsize.x, AABB.halfsize.y, AABB.halfsize.z);
        sh->setUniform("u_model", matrix);
        sh->setUniform("u_color", Vector4f(0, 1, 1, 1));
        wire_box->render(GL_LINES);
    }

    sh->disable();
}



GFX::Mesh* GFX::Mesh::getQuad()
{
    static GFX::Mesh* quad = NULL;
    if (!quad)
    {
        quad = new GFX::Mesh();
        quad->createQuad(0, 0, 2, 2, false);
        quad->uploadToVRAM();
    }
    return quad;
}

GFX::Mesh* GFX::Mesh::Get(const char* filename)
{
    assert(filename);
    std::map<std::string, GFX::Mesh*>::iterator it = sMeshesLoaded.find(filename);
    if (it != sMeshesLoaded.end())
        return it->second;

    GFX::Mesh* m = new GFX::Mesh();
    std::string name = filename;

    //detect format
    char file_format = 0;
    std::string ext = name.substr(name.find_last_of(".") + 1);
    if (ext == "ase" || ext == "ASE")
        file_format = FORMAT_ASE;
    else if (ext == "obj" || ext == "OBJ")
        file_format = FORMAT_OBJ;
    else if (ext == "mbin" || ext == "MBIN")
        file_format = FORMAT_MBIN;
    else if (ext == "mesh" || ext == "MESH")
        file_format = FORMAT_MESH;
    else
    {
        std::cerr << "Unknown mesh format: " << filename << std::endl;
        return NULL;
    }

    //stats
    long time = getTime();
    std::cout << " + GFX::Mesh loading: " << filename << " ... ";
    std::string binfilename = filename;

    if (file_format != FORMAT_MBIN)
        binfilename = binfilename + ".mbin";

    //try loading the binary version
    if (use_binary && m->readBin(binfilename.c_str()))
    {
        if (interleave_meshes && m->interleaved.size() == 0)
        {
            std::cout << "[INTERL] ";
            m->interleaveBuffers();
        }

        if (auto_upload_to_vram)
        {
            std::cout << "[VRAM] ";
            m->uploadToVRAM();
        }

        std::cout << "[OK BIN]  Faces: " << (m->interleaved.size() ? m->interleaved.size() : m->vertices.size()) / 3 << " Time: " << (getTime() - time) * 0.001 << "sec" << std::endl;
        m->registerMesh(filename);
        return m;
    }

    //load the ascii version
    bool loaded = false;
    if (file_format == FORMAT_OBJ)
        loaded = m->loadOBJ(filename);
    /*else if (file_format == FORMAT_ASE)
        loaded = m->loadASE(filename);*/
    else if (file_format == FORMAT_MESH)
        loaded = m->loadMESH(filename);

    if (!loaded)
    {
        delete m;
        std::cout << "[ERROR]: GFX::Mesh not found" << std::endl;
        return NULL;
    }

    //to optimize, interleave the meshes
    if (interleave_meshes)
    {
        std::cout << "[INTERL] ";
        m->interleaveBuffers();
    }

    //and upload them to VRAM
    if (auto_upload_to_vram)
    {
        std::cout << "[VRAM] ";
        m->uploadToVRAM();
    }

    std::cout << "[OK]  Faces: " << m->vertices.size() / 3 << " Time: " << (getTime() - time) * 0.001 << "sec" << std::endl;
    if (use_binary)
    {
        std::cout << "\t\t Writing .BIN ... ";
        m->writeBin(filename);
        std::cout << "[OK]" << std::endl;
    }

    m->registerMesh(name);
    return m;
}

void GFX::Mesh::registerMesh(std::string name)
{
    this->name = name;
    sMeshesLoaded[name] = this;
}

bool GFX::Mesh::createCollisionModel(bool is_static)
{
    if (collision_model)
        return true;

    double time = getTime();
    std::cout << "Creating collision model for: " << this->name << " (" << (interleaved.size() ? interleaved.size() : vertices.size()) / 3 << ") ...";

    CollisionModel3D* collision_model = newCollisionModel3D(is_static);

    if (indices.size()) //indexed
    {
        collision_model->setTriangleNumber((int)indices.size() / 3);

        if (interleaved.size())
            for (unsigned int i = 0; i < indices.size(); i += 3)
            {
                auto v1 = interleaved[indices[i + 0]];
                auto v2 = interleaved[indices[i + 1]];
                auto v3 = interleaved[indices[i + 2]];
                collision_model->addTriangle(v1.vertex.v, v2.vertex.v, v3.vertex.v);
            }
        else
            for (unsigned int i = 0; i < indices.size(); i += 3)
            {
                auto v1 = vertices[indices[i + 0]];
                auto v2 = vertices[indices[i + 1]];
                auto v3 = vertices[indices[i + 2]];
                collision_model->addTriangle(v1.v, v2.v, v3.v);
            }
    }
    else if (interleaved.size()) //is interleaved
    {
        collision_model->setTriangleNumber(interleaved.size() / 3);
        for (unsigned int i = 0; i < interleaved.size(); i += 3)
        {
            auto v1 = interleaved[i];
            auto v2 = interleaved[i + 1];
            auto v3 = interleaved[i + 2];
            collision_model->addTriangle(v1.vertex.v, v2.vertex.v, v3.vertex.v);
        }
    }
    else if (vertices.size()) //non interleaved
    {
        collision_model->setTriangleNumber((int)vertices.size() / 3);
        for (unsigned int i = 0; i < (int)vertices.size(); i += 3)
        {
            auto v1 = vertices[i];
            auto v2 = vertices[i + 1];
            auto v3 = vertices[i + 2];
            collision_model->addTriangle(v1.v, v2.v, v3.v);
        }
    }
    else
    {
        assert(0 && "mesh without vertices, cannot create collision model");
        std::cout << "[ERROR]" << std::endl;
        return false;
    }
    collision_model->finalize();
    this->collision_model = collision_model;

    std::cout << "[OK] Time: " << (getTime() - time) * 0.001 << "sec" << std::endl;

    return true;
}

//help: model is the transform of the mesh, ray origin and direction, a Vector3 where to store the collision if found, a Vector3 where to store the normal if there was a collision, max ray distance in case the ray should go to infintiy, and in_object_space to get the collision point in object space or world space
bool GFX::Mesh::testRayCollision(Matrix44 model, Vector3f start, Vector3f front, Vector3f& collision, Vector3f& normal, float max_ray_dist, bool in_object_space)
{
    if (!this->collision_model)
    {
        //test first against bounding before creating collision model
        BoundingBox aabb = transformBoundingBox(model, box);
        if (!RayBoundingBoxCollision(aabb, start, front, collision))
            return false;

        if (!createCollisionModel())
            return false;
    }

    CollisionModel3D* collision_model = (CollisionModel3D*)this->collision_model;
    assert(collision_model && "CollisionModel3D must be created before using it, call createCollisionModel");

    collision_model->setTransform(model.m);
    if (collision_model->rayCollision(start.v, front.v, true, 0.0, max_ray_dist) == false)
        return false;

    collision_model->getCollisionPoint(collision.v, in_object_space);

    float t1[9], t2[9];
    collision_model->getCollidingTriangles(t1, t2, in_object_space);

    Vector3f v1;
    Vector3f v2;
    v1 = Vector3f(t1[3] - t1[0], t1[4] - t1[1], t1[5] - t1[2]);
    v2 = Vector3f(t1[6] - t1[0], t1[7] - t1[1], t1[8] - t1[2]);
    v1.normalize();
    v2.normalize();
    normal = v1.cross(v2);

    return true;
}

bool GFX::Mesh::testSphereCollision(Matrix44 model, Vector3f center, float radius, Vector3f& collision, Vector3f& normal)
{
    if (!this->collision_model)
        if (!createCollisionModel())
            return false;

    CollisionModel3D* collision_model = (CollisionModel3D*)this->collision_model;
    assert(collision_model && "CollisionModel3D must be created before using it, call createCollisionModel");

    collision_model->setTransform(model.m);
    if (collision_model->sphereCollision(center.v, radius) == false)
        return false;

    collision_model->getCollisionPoint(collision.v, false);

    float t1[9], t2[9];
    collision_model->getCollidingTriangles(t1, t2, false);

    Vector3f v1;
    Vector3f v2;
    v1 = Vector3f(t1[3] - t1[0], t1[4] - t1[1], t1[5] - t1[2]);
    v2 = Vector3f(t1[6] - t1[0], t1[7] - t1[1], t1[8] - t1[2]);
    v1.normalize();
    v2.normalize();
    normal = v1.cross(v2);

    return true;
}

void GFX::Mesh::createSphere(float radius, float slices, float arcs)
{
    for (int i = 0; i < slices; ++i)
    {
        float u1 = (i / slices);
        float r_angle1 = u1 * M_PI * 2;
        float u2 = ((i + 1) / slices);
        float r_angle2 = u2 * M_PI * 2;
        quat r1;
        r1.setAxisAngle(vec3(0, 1, 0), r_angle1);
        quat r2;
        r2.setAxisAngle(vec3(0, 1, 0), r_angle2);

        for (int j = -arcs / 2; j < arcs / 2; ++j)
        {
            float v1 = (j / arcs);
            float ang1 = v1 * M_PI;
            float v2 = ((j + 1) / arcs);
            float ang2 = v2 * M_PI;
            vec3 A(cos(ang1) * radius, sin(ang1) * radius, 0.0);
            vec3 B(cos(ang2) * radius, sin(ang2) * radius, 0.0);
            vec3 P1 = r1.rotate(A);
            vec3 P2 = r2.rotate(A);
            vec3 P3 = r1.rotate(B);
            vec3 P4 = r2.rotate(B);
            vec3 N1 = normalize(P1);
            vec3 N2 = normalize(P2);
            vec3 N3 = normalize(P3);
            vec3 N4 = normalize(P4);
            vec2 UV1 = vec2(u1, v1 * 0.5 + 0.5);
            vec2 UV2 = vec2(u2, v1 * 0.5 + 0.5);
            vec2 UV3 = vec2(u1, v2 * 0.5 + 0.5);
            vec2 UV4 = vec2(u2, v2 * 0.5 + 0.5);

            vertices.push_back(P1);
            vertices.push_back(P3);
            vertices.push_back(P2);
            vertices.push_back(P2);
            vertices.push_back(P3);
            vertices.push_back(P4);

            normals.push_back(N1);
            normals.push_back(N3);
            normals.push_back(N2);
            normals.push_back(N2);
            normals.push_back(N3);
            normals.push_back(N4);

            uvs.push_back(UV1);
            uvs.push_back(UV3);
            uvs.push_back(UV2);
            uvs.push_back(UV2);
            uvs.push_back(UV3);
            uvs.push_back(UV4);
        }
    }

    box.center.set(0, 0, 0);
    box.halfsize = vec3(radius * sqrt(3.0)); //not sure
    this->radius = radius;
}

GFX::Mesh* GFX::Mesh::Get(const char* filename, bool skip_load)
{
    assert(filename);
    std::map<std::string, Mesh*>::iterator it = sMeshesLoaded.find(filename);
    if (it != sMeshesLoaded.end())
        return it->second;

    if (skip_load)
        return NULL;

    Mesh* m = new Mesh();
    std::string name = filename;

    //detect format
    char file_format = 0;
    std::string ext = name.substr(name.find_last_of(".") + 1);
    if (ext == "ase" || ext == "ASE")
        file_format = FORMAT_ASE;
    else if (ext == "obj" || ext == "OBJ")
        file_format = FORMAT_OBJ;
    else if (ext == "mbin" || ext == "MBIN")
        file_format = FORMAT_MBIN;
    else if (ext == "mesh" || ext == "MESH")
        file_format = FORMAT_MESH;
    else
    {
        //if (ext.size()) std::cerr << "Unknown mesh format: " << filename << std::endl;
        return NULL;
    }

    //stats
    double time = getTime();
    std::cout << " + Mesh loading: " << TermColor::YELLOW << filename << TermColor::DEFAULT << " ... ";
    std::string binfilename = filename;

    if (file_format != FORMAT_MBIN)
        binfilename = binfilename + ".mbin";

    //try loading the binary version
    if (use_binary && m->readBin(binfilename.c_str()))
    {
        if (interleave_meshes && m->interleaved.size() == 0)
        {
            std::cout << "[INTERL] ";
            m->interleaveBuffers();
        }

        if (auto_upload_to_vram)
        {
            std::cout << "[VRAM] ";
            m->uploadToVRAM();
        }

        std::cout << "[OK BIN]  Faces: " << (m->interleaved.size() ? m->interleaved.size() : m->vertices.size()) / 3 << " Time: " << (getTime() - time) * 0.001 << "sec" << std::endl;
        sMeshesLoaded[filename] = m;
        return m;
    }

    //load the ascii version
    bool loaded = false;
    if (file_format == FORMAT_OBJ)
        loaded = m->loadOBJ(filename);
    else if (file_format == FORMAT_ASE)
        loaded = m->loadASE(filename);
    else if (file_format == FORMAT_MESH)
        loaded = m->loadMESH(filename);

    if (!loaded)
    {
        delete m;
        std::cout << "[ERROR]: Mesh not found" << std::endl;
        return NULL;
    }

    //to optimize, interleave the meshes
    if (interleave_meshes)
    {
        std::cout << "[INTERL] ";
        m->interleaveBuffers();
    }

    //and upload them to VRAM
    if (auto_upload_to_vram)
    {
        std::cout << "[VRAM] ";
        m->uploadToVRAM();
    }

    std::cout << "[OK]  Faces: " << m->vertices.size() / 3 << " Time: " << (getTime() - time) * 0.001 << "sec" << std::endl;
    if (use_binary)
    {
        std::cout << "\t\t Writing .BIN ... ";
        m->writeBin(filename);
        std::cout << "[OK]" << std::endl;
    }

    m->registerMesh(name);
    return m;
}

bool GFX::Mesh::loadASE(const char* filename)
{
    int nVtx, nFcs;
    int count;
    int vId, aId, bId, cId;
    float vtxX, vtxY, vtxZ;
    float nX, nY, nZ;
    TextParser t;
    if (t.create(filename) == false)
        return false;

    t.seek("*MESH_NUMVERTEX");
    nVtx = t.getint();
    t.seek("*MESH_NUMFACES");
    nFcs = t.getint();

    normals.resize(nFcs * 3);
    vertices.resize(nFcs * 3);
    uvs.resize(nFcs * 3);

    std::vector<Vector3f> unique_vertices;
    unique_vertices.resize(nVtx);

    const float max_float = 10000000;
    const float min_float = -10000000;
    aabb_min.set(max_float, max_float, max_float);
    aabb_max.set(min_float, min_float, min_float);

    //load unique vertices
    for (count = 0;count < nVtx;count++)
    {
        t.seek("*MESH_VERTEX");
        vId = t.getint();
        vtxX = (float)t.getfloat();
        vtxY = (float)t.getfloat();
        vtxZ = (float)t.getfloat();
        Vector3f v(-vtxX, vtxZ, vtxY);
        unique_vertices[count] = v;
        aabb_min.setMin(v);
        aabb_max.setMax(v);
    }
    box.center = (aabb_max + aabb_min) * 0.5f;
    box.halfsize = (aabb_max - box.center);
    radius = (float)fmax(aabb_max.length(), aabb_min.length());

    int prev_mat = 0;

    sSubmeshInfo submesh;
    memset(&submesh, 0, sizeof(submesh));

    //load faces
    for (count = 0;count < nFcs;count++)
    {
        t.seek("*MESH_FACE");
        t.seek("A:");
        aId = t.getint();
        t.seek("B:");
        bId = t.getint();
        t.seek("C:");
        cId = t.getint();
        vertices[count * 3 + 0] = unique_vertices[aId];
        vertices[count * 3 + 1] = unique_vertices[bId];
        vertices[count * 3 + 2] = unique_vertices[cId];

        t.seek("*MESH_MTLID");
        int current_mat = t.getint();
        if (current_mat != prev_mat)
        {
            submesh.length = count * 3 - submesh.start;
            submeshes.push_back(submesh);
            memset(&submesh, 0, sizeof(submesh));
            submesh.start = count * 3;
            prev_mat = current_mat;
        }
    }

    submesh.length = count * 3 - submesh.start;
    submeshes.push_back(submesh);

    t.seek("*MESH_NUMTVERTEX");
    nVtx = t.getint();
    std::vector<Vector2f> unique_uvs;
    unique_uvs.resize(nVtx);

    for (count = 0;count < nVtx;count++)
    {
        t.seek("*MESH_TVERT");
        vId = t.getint();
        vtxX = (float)t.getfloat();
        vtxY = (float)t.getfloat();
        unique_uvs[count] = Vector2f(vtxX, vtxY);
    }

    t.seek("*MESH_NUMTVFACES");
    nFcs = t.getint();
    for (count = 0;count < nFcs;count++)
    {
        t.seek("*MESH_TFACE");
        t.getint(); //num face
        uvs[count * 3] = unique_uvs[t.getint()];
        uvs[count * 3 + 1] = unique_uvs[t.getint()];
        uvs[count * 3 + 2] = unique_uvs[t.getint()];
    }

    //normals
    for (count = 0;count < nFcs;count++)
    {
        t.seek("*MESH_VERTEXNORMAL");
        aId = t.getint();
        nX = (float)t.getfloat();
        nY = (float)t.getfloat();
        nZ = (float)t.getfloat();
        normals[count * 3] = Vector3f(-nX, nZ, nY);
        t.seek("*MESH_VERTEXNORMAL");
        aId = t.getint();
        nX = (float)t.getfloat();
        nY = (float)t.getfloat();
        nZ = (float)t.getfloat();
        normals[count * 3 + 1] = Vector3f(-nX, nZ, nY);
        t.seek("*MESH_VERTEXNORMAL");
        aId = t.getint();
        nX = (float)t.getfloat();
        nY = (float)t.getfloat();
        nZ = (float)t.getfloat();
        normals[count * 3 + 2] = Vector3f(-nX, nZ, nY);
    }

    return true;
}
