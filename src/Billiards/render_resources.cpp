#include "render_resources.h"

#define GLEW_STATIC
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

#include "assets.h"
#include "image_loader.h"

#include <cstdio>
#include <vector>

namespace billiardgl {
namespace {

unsigned int createObjectVbo(const ObjLoader& obj, float scale)
{
    std::vector<GLfloat> vertices(obj.vertices.size() * 3);
    std::vector<GLfloat> normals(obj.vertices.size() * 3);
    std::vector<GLfloat> texCoords(obj.vertices.size() * 3);

    for (size_t i = 0; i < obj.vertices.size(); ++i) {
        vertices[i * 3] = obj.vertices[i].position.x * scale;
        vertices[i * 3 + 1] = obj.vertices[i].position.y * scale;
        vertices[i * 3 + 2] = obj.vertices[i].position.z * scale;
        normals[i * 3] = obj.vertices[i].normal.x;
        normals[i * 3 + 1] = obj.vertices[i].normal.y;
        normals[i * 3 + 2] = obj.vertices[i].normal.z;
        texCoords[i * 3] = obj.vertices[i].texture.x;
        texCoords[i * 3 + 1] = obj.vertices[i].texture.y;
        texCoords[i * 3 + 2] = obj.vertices[i].texture.z;
    }

    GLuint vbo = 0;
    const size_t dataSize = sizeof(GLfloat) * obj.vertices.size() * 3;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glGenBuffersARB(1, &vbo);
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo);
    glBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 3, 0, GL_STATIC_DRAW_ARB);
    glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, dataSize, vertices.data());
    glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize, dataSize, normals.data());
    glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 2, dataSize, texCoords.data());
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    return vbo;
}

bool textureLoaded(unsigned int texture)
{
    return texture != 0;
}

}  // namespace

unsigned int uploadTexture(const std::string& path)
{
    const ImageData image = loadImageFile(path);
    if (!image.error.empty()) {
        std::fprintf(stderr, "Failed to load texture %s: %s\n", path.c_str(), image.error.c_str());
        return 0;
    }

    GLuint lastTexture = 0;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return 0;
    }

    glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<int*>(&lastTexture));
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    glBindTexture(GL_TEXTURE_2D, lastTexture);
    return texture;
}

bool initializeRenderResources(RenderResources& resources, GameState& state)
{
    glEnable(GL_TEXTURE_2D);
    glewInit();

    resources.tableObj.reset(new ObjLoader(getObjectPath("table.obj")));
    resources.cueObj.reset(new ObjLoader(getObjectPath("cue.obj")));
    resources.benchObj.reset(new ObjLoader(getObjectPath("bench.obj")));
    resources.wardObj.reset(new ObjLoader(getObjectPath("wardrobe.obj")));

    resources.tableTextures[0] = uploadTexture(getTexturePath(resources.tableObj->materials[0]->texture));
    resources.tableTextures[1] = uploadTexture(getTexturePath(resources.tableObj->materials[1]->texture));
    resources.cueTextures[0] = uploadTexture(getTexturePath(resources.cueObj->materials[0]->texture));
    resources.cueTextures[1] = uploadTexture(getTexturePath(resources.cueObj->materials[1]->texture));

    const char* ballTextureNames[kBallCount] = {
        "B16.bmp", "B1.bmp", "B2.bmp", "B3.bmp", "B4.bmp", "B5.bmp", "B6.bmp", "B7.bmp",
        "B8.bmp", "B9.bmp", "B10.bmp", "B11.bmp", "B12.bmp", "B13.bmp", "B14.bmp", "B15.bmp"
    };
    for (int i = 0; i < kBallCount; ++i) {
        resources.ballTextures[i] = uploadTexture(getTexturePath(ballTextureNames[i]));
    }

    resources.ceilingTexture = uploadTexture(getTexturePath("ceiling.bmp"));
    resources.blackTexture = uploadTexture(getTexturePath("black.bmp"));
    resources.wardTexture = uploadTexture(getTexturePath("5.bmp"));
    resources.flameTexture = uploadTexture(getTexturePath("flame2.bmp"));

    resources.tableVertexVBO = createObjectVbo(*resources.tableObj, 1.0f);
    resources.cueVertexVBO = createObjectVbo(*resources.cueObj, 1.0f);
    resources.benchVertexVBO = createObjectVbo(*resources.benchObj, 1.0f / 3.0f);
    resources.wardVertexVBO = createObjectVbo(*resources.wardObj, 3.0f);

    applyBallTexturesToState(resources, state);

    return resources.tableObj && resources.cueObj && resources.benchObj && resources.wardObj
        && textureLoaded(resources.tableTextures[0])
        && textureLoaded(resources.tableTextures[1])
        && textureLoaded(resources.cueTextures[0])
        && textureLoaded(resources.cueTextures[1])
        && textureLoaded(resources.ceilingTexture)
        && textureLoaded(resources.blackTexture)
        && textureLoaded(resources.wardTexture)
        && textureLoaded(resources.flameTexture)
        && resources.tableVertexVBO != 0
        && resources.cueVertexVBO != 0
        && resources.benchVertexVBO != 0
        && resources.wardVertexVBO != 0;
}

void destroyRenderResources(RenderResources& resources)
{
    GLuint buffers[] = {
        resources.tableVertexVBO,
        resources.cueVertexVBO,
        resources.benchVertexVBO,
        resources.wardVertexVBO
    };
    glDeleteBuffersARB(4, buffers);

    resources.tableObj.reset();
    resources.cueObj.reset();
    resources.benchObj.reset();
    resources.wardObj.reset();
}

void applyBallTexturesToState(const RenderResources& resources, GameState& state)
{
    for (int i = 0; i < kBallCount; ++i) {
        state.balls[i].texture = resources.ballTextures[i];
    }
}

}  // namespace billiardgl
