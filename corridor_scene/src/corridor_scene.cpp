// corridor_scene.cpp - SCENE_CORRIDOR
// WASD movement, picture frames, ceiling fixtures, 3-point lighting + shadow
#include <crtdbg.h>
#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/shader.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>

// stb_image: compiled exactly once here across the whole project.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION
#include <learnopengl/model.h>

namespace Corridor {

// ---- Window / Shadow resolution ----
const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;
const unsigned int SHADOW_W   = 2048;
const unsigned int SHADOW_H   = 2048;

// ---- Scene state ----
enum SceneState { PLAYING, FADING_OUT, DONE };
SceneState gScene = PLAYING;

// ---- Timing ----
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---- Camera ----
float camX       =  0.0f;
float camZ       = -12.0f;
float walkTimer  =  0.0f;
bool  isMoving   =  false;

// Mouse look
float yaw        =  90.0f;
float pitch      =   0.0f;
float lastMouseX =  640.0f;
float lastMouseY =  360.0f;
bool  firstMouse =  true;

// Window opening in right wall
const float WIN_Z_MIN = -5.5f;
const float WIN_Z_MAX = -3.5f;
const float WIN_Y_MIN =  0.10f;
const float WIN_Y_MAX =  1.40f;

const float WALK_SPEED  = 2.5f;
const float BASE_Y      = 0.0f;
const float BOB_FREQ    = 1.8f;
const float BOB_AMP     = 0.045f;

// Corridor bounds for collision
const float BOUND_X = 1.75f;
const float BOUND_Z_MIN = -12.5f;
const float BOUND_Z_MAX =  0.5f;
const float DOOR_TRIGGER_Z = -0.5f;

// ---- Fade ----
float fadeAlpha         = 0.0f;
const float FADE_SPEED  = 1.0f;

// ---- Particle system ----
const int MAX_PARTICLES = 280;
struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    float age;
    float maxAge;
    float size;
    float baseAlpha;
};
static Particle gParticles[MAX_PARTICLES];

// ---- Dog companion state (persistent across frames) ----
static glm::vec3 gDogPos(0.f, -1.f, -5.5f);  // world position (Y = floor)
static glm::vec3 gDogDir(0.f, 0.f,  1.f);      // normalised facing direction
static float     gDogWalk   = 0.f;             // walk-cycle phase (radians)
static bool      gDogMoving = false;
static float     gDogYawRad = 0.f;

// ---- Wine post-processing effect ----
static bool  wineActive  = false;
static bool  wineKeyHeld = false;

// ---- Glass vase explosion ----
static bool  vaseExploding   = false;
static float vaseExplodeTime = 0.0f;
static bool  vaseGone        = false;
static bool  vaseKeyHeld     = false;

// =============================================================
//  Procedural textures
// =============================================================
static unsigned int makeTexture(const std::vector<unsigned char>& data, int w, int h,
                                bool repeat = true)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    int wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

// Load a JPG/PNG texture from disk using stb_image.
// Flips vertically so OpenGL UV origin (bottom-left) matches the image.
// Returns 0 and prints an error if the file cannot be opened.
static unsigned int loadTextureFile(const char* path, bool repeat = true)
{
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 3);  // force RGB
    if (!data) {
        std::cerr << "[texture] Failed to load: " << path << "\n";
        return 0;
    }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

// White floor tiles with grey grout
static unsigned int genFloorTex()
{
    const int W = 512, H = 512, TILE = 64, GROUT = 4;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        bool g = (x % TILE < GROUT || y % TILE < GROUT);
        float n = g ? 1.f : (0.97f + 0.03f * float((x*7 + y*13) % 11) / 10.f);
        int i = (y * W + x) * 3;
        d[i]   = (unsigned char)(g ? 108 : 232 * n);
        d[i+1] = (unsigned char)(g ? 106 : 230 * n);
        d[i+2] = (unsigned char)(g ? 103 : 227 * n);
    }
    return makeTexture(d, W, H);
}

// Light painted wall with subtle panel lines
static unsigned int genWallTex()
{
    const int W = 512, H = 512;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float n = 1.f - 0.03f * float((x*3 + y*7) % 17) / 16.f;
        float b = ((y % 256) < 3) ? 0.87f : 1.f;
        int i = (y * W + x) * 3;
        d[i]   = (unsigned char)(240 * n * b);
        d[i+1] = (unsigned char)(238 * n * b);
        d[i+2] = (unsigned char)(235 * n * b);
    }
    return makeTexture(d, W, H);
}

// Off-white ceiling
static unsigned int genCeilingTex()
{
    const int W = 128, H = 128;
    std::vector<unsigned char> d(W * H * 3);
    for (int i = 0; i < W * H; i++) {
        float n = 1.f - 0.015f * float((i*5) % 13) / 12.f;
        d[i*3]   = (unsigned char)(253 * n);
        d[i*3+1] = (unsigned char)(251 * n);
        d[i*3+2] = (unsigned char)(249 * n);
    }
    return makeTexture(d, W, H);
}

// Dark charcoal door with panel insets
static unsigned int genDoorTex()
{
    const int W = 256, H = 512;
    std::vector<unsigned char> d(W * H * 3);
    for (int i = 0; i < W * H; i++) { d[i*3]=44; d[i*3+1]=44; d[i*3+2]=50; }
    auto rect = [&](int x0,int y0,int x1,int y1, unsigned char v) {
        for (int y=y0; y<=y1; y++) for (int x=x0; x<=x1; x++) {
            if (x<0||x>=W||y<0||y>=H) continue;
            bool border = (x==x0||x==x1||y==y0||y==y1||x==x0+1||x==x1-1||y==y0+1||y==y1-1);
            if (border) { d[(y*W+x)*3]=v; d[(y*W+x)*3+1]=v; d[(y*W+x)*3+2]=v+5; }
        }
    };
    rect(18, 18, 237, 238, 78);
    rect(18,258, 237, 492, 78);
    for (int y=282; y<308; y++) for (int x=208; x<238; x++) { d[(y*W+x)*3]=155; d[(y*W+x)*3+1]=145; d[(y*W+x)*3+2]=115; }
    return makeTexture(d, W, H, false);
}

// Painting 0 - "City Night" : dark blue, cyan geometry
static unsigned int genPainting0()
{
    const int W = 256, H = 256;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        // Gradient background
        d[i]   = (unsigned char)(5 + 15 * y / H);
        d[i+1] = (unsigned char)(10 + 30 * y / H);
        d[i+2] = (unsigned char)(60 + 80 * y / H);
        // Building silhouettes
        int bh = 60 + (x / 32) * 20 + (x / 16) % 2 * 15;
        if (y > H - bh) {
            d[i] = 18; d[i+1] = 18; d[i+2] = 28;
        }
        // Glowing windows
        if (y > H - bh + 5 && (x % 12 < 4) && (y % 10 < 4)) {
            d[i] = 220; d[i+1] = 200; d[i+2] = 120;
        }
        // Cyan horizon line
        if (y == H - 62 || y == H - 63) {
            d[i] = 0; d[i+1] = 200; d[i+2] = 220;
        }
    }
    return makeTexture(d, W, H, false);
}

// Painting 1 - "Warm Abstract" : orange/red color field
static unsigned int genPainting1()
{
    const int W = 256, H = 256;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float fx = (float)x / W, fy = (float)y / H;
        // Color zones
        if (fx < 0.35f) {
            d[i]=200; d[i+1]=80; d[i+2]=30;
        } else if (fx < 0.65f) {
            d[i]=(unsigned char)(240*fy+30*(1-fy));
            d[i+1]=(unsigned char)(160*fy+60*(1-fy));
            d[i+2]=20;
        } else {
            d[i]=255; d[i+1]=(unsigned char)(200*fy+90*(1-fy)); d[i+2]=40;
        }
        // Horizontal dividing lines
        if (y == H/3 || y == H/3+1 || y == 2*H/3 || y == 2*H/3+1) {
            d[i]=255; d[i+1]=255; d[i+2]=240;
        }
        // Vertical dividing lines
        if (x == W*35/100 || x == W*65/100) {
            d[i]=255; d[i+1]=255; d[i+2]=240;
        }
    }
    return makeTexture(d, W, H, false);
}

// Painting 2 - "Green Nature" : white bg with green organic forms
static unsigned int genPainting2()
{
    const int W = 256, H = 256;
    std::vector<unsigned char> d(W * H * 3, 245);  // off-white bg
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float fx = (float)x / W - 0.5f, fy = (float)y / H - 0.5f;
        // Elliptical green blobs
        float e1 = (fx+0.15f)*(fx+0.15f)*4.f + (fy+0.1f)*(fy+0.1f)*9.f;
        float e2 = (fx-0.2f)*(fx-0.2f)*6.f  + (fy-0.2f)*(fy-0.2f)*6.f;
        float e3 = fx*fx*3.f + (fy+0.3f)*(fy+0.3f)*12.f;
        if (e1 < 0.08f) { d[i]=60; d[i+1]=130; d[i+2]=60; }
        else if (e2 < 0.06f) { d[i]=40; d[i+1]=110; d[i+2]=50; }
        else if (e3 < 0.04f) { d[i]=80; d[i+1]=160; d[i+2]=70; }
        // Ground stripe
        if (y > H * 3 / 4) {
            float t = (float)(y - H*3/4) / (H/4);
            d[i]  = (unsigned char)(180*t + 245*(1-t));
            d[i+1]= (unsigned char)(160*t + 245*(1-t));
            d[i+2]= (unsigned char)(120*t + 245*(1-t));
        }
    }
    return makeTexture(d, W, H, false);
}

// =============================================================
//  Vertex helper: pos(3) + normal(3) + uv(2) = 8 floats
// =============================================================
static void pushV(std::vector<float>& v, glm::vec3 p, glm::vec3 n, glm::vec2 uv)
{
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    v.push_back(uv.x); v.push_back(uv.y);
}
static void addQuad(std::vector<float>& v,
                    glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 n,
                    glm::vec2 u0, glm::vec2 u1, glm::vec2 u2, glm::vec2 u3)
{
    pushV(v,p0,n,u0); pushV(v,p1,n,u1); pushV(v,p2,n,u2);
    pushV(v,p0,n,u0); pushV(v,p2,n,u2); pushV(v,p3,n,u3);
}
static unsigned int uploadVAO(const std::vector<float>& verts, int& cnt)
{
    cnt = (int)(verts.size() / 8);
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glBindVertexArray(0);
    return VAO;
}

// =============================================================
//  Normal-Map vertex helpers: pos(3)+normal(3)+uv(2)+tangent(3)+bitangent(3) = 14 floats
//  Reference: learnopengl.com/Advanced-Lighting/Normal-Mapping  (tangent calc section)
// =============================================================
static void pushV_NM(std::vector<float>& v,
                     glm::vec3 p, glm::vec3 n, glm::vec2 uv,
                     glm::vec3 t, glm::vec3 b)
{
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    v.push_back(uv.x); v.push_back(uv.y);
    v.push_back(t.x); v.push_back(t.y); v.push_back(t.z);
    v.push_back(b.x); v.push_back(b.y); v.push_back(b.z);
}

// Compute tangent/bitangent from quad edges+UVs (LearnOpenGL formula),
// then emit two triangles with those T/B values for all 4 vertices.
static void addQuad_NM(std::vector<float>& v,
                       glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                       glm::vec3 n,
                       glm::vec2 u0, glm::vec2 u1, glm::vec2 u2, glm::vec2 u3)
{
    // Use first triangle (p0,p1,p2) to derive tangent/bitangent
    glm::vec3 e1 = p1 - p0,  e2 = p2 - p0;
    glm::vec2 d1 = u1 - u0,  d2 = u2 - u0;

    float det = d1.x * d2.y - d2.x * d1.y;
    float f   = (fabsf(det) > 1e-8f) ? (1.0f / det) : 0.0f;

    glm::vec3 T = glm::normalize(glm::vec3(
        f * (d2.y * e1.x - d1.y * e2.x),
        f * (d2.y * e1.y - d1.y * e2.y),
        f * (d2.y * e1.z - d1.y * e2.z)));
    glm::vec3 B = glm::normalize(glm::vec3(
        f * (-d2.x * e1.x + d1.x * e2.x),
        f * (-d2.x * e1.y + d1.x * e2.y),
        f * (-d2.x * e1.z + d1.x * e2.z)));

    // Flat quad: same T/B for all 4 vertices
    pushV_NM(v, p0, n, u0, T, B);  pushV_NM(v, p1, n, u1, T, B);
    pushV_NM(v, p2, n, u2, T, B);
    pushV_NM(v, p0, n, u0, T, B);  pushV_NM(v, p2, n, u2, T, B);
    pushV_NM(v, p3, n, u3, T, B);
}

// Upload NM vertex buffer (14 floats/vertex, 5 attrib locations)
static unsigned int uploadVAO_NM(const std::vector<float>& verts, int& cnt)
{
    cnt = (int)(verts.size() / 14);
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float),
                 verts.data(), GL_STATIC_DRAW);
    const int S = 14 * (int)sizeof(float);
    // loc 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S, (void*)0);
    // loc 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, S, (void*)(3*sizeof(float)));
    // loc 2: uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, S, (void*)(6*sizeof(float)));
    // loc 3: tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, S, (void*)(8*sizeof(float)));
    // loc 4: bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, S, (void*)(11*sizeof(float)));
    glBindVertexArray(0);
    return VAO;
}

// =============================================================
//  Corridor mesh (floor / ceiling / walls)
// =============================================================
struct CorridorMesh {
    unsigned int floorVAO, ceilVAO, wallVAO;
    int floorVerts, ceilVerts, wallVerts;
};
static CorridorMesh buildCorridorMesh()
{
    const float xMin=-2.f, xMax=2.f, yMin=-1.f, yMax=2.5f, zMin=-13.f, zMax=1.f;
    const float dxMin=-0.85f, dxMax=0.85f, dyMax=1.9f;

    // -- Floor --
    std::vector<float> fv;
    addQuad(fv, glm::vec3(xMin,yMin,zMin), glm::vec3(xMax,yMin,zMin),
                glm::vec3(xMax,yMin,zMax), glm::vec3(xMin,yMin,zMax),
                glm::vec3(0,1,0),
                glm::vec2(0,0), glm::vec2(4,0), glm::vec2(4,14), glm::vec2(0,14));

    // -- Ceiling --
    std::vector<float> cv;
    addQuad(cv, glm::vec3(xMin,yMax,zMin), glm::vec3(xMin,yMax,zMax),
                glm::vec3(xMax,yMax,zMax), glm::vec3(xMax,yMax,zMin),
                glm::vec3(0,-1,0),
                glm::vec2(0,0), glm::vec2(0,14), glm::vec2(4,14), glm::vec2(4,0));

    // -- Walls --
    std::vector<float> wv;
    float wallH = yMax - yMin, wallL = zMax - zMin;

    addQuad(wv, glm::vec3(xMin,yMin,zMin), glm::vec3(xMin,yMin,zMax),
                glm::vec3(xMin,yMax,zMax), glm::vec3(xMin,yMax,zMin),
                glm::vec3(1,0,0),
                glm::vec2(0,0), glm::vec2(wallL,0), glm::vec2(wallL,wallH), glm::vec2(0,wallH));

    // Right wall split around window opening
    {
        float uz0=WIN_Z_MIN-zMin, uz1=WIN_Z_MAX-zMin, vy0=WIN_Y_MIN-yMin, vy1=WIN_Y_MAX-yMin;
        glm::vec3 nr(-1,0,0);
        // Bottom strip (full Z width, below window)
        addQuad(wv,glm::vec3(xMax,yMin,zMin),   glm::vec3(xMax,WIN_Y_MIN,zMin),
                   glm::vec3(xMax,WIN_Y_MIN,zMax),glm::vec3(xMax,yMin,zMax),nr,
                   glm::vec2(0,0),glm::vec2(0,vy0),glm::vec2(wallL,vy0),glm::vec2(wallL,0));
        // Top strip (full Z width, above window)
        addQuad(wv,glm::vec3(xMax,WIN_Y_MAX,zMin),glm::vec3(xMax,yMax,zMin),
                   glm::vec3(xMax,yMax,zMax),    glm::vec3(xMax,WIN_Y_MAX,zMax),nr,
                   glm::vec2(0,vy1),glm::vec2(0,wallH),glm::vec2(wallL,wallH),glm::vec2(wallL,vy1));
        // Far side of window gap
        addQuad(wv,glm::vec3(xMax,WIN_Y_MIN,zMin),   glm::vec3(xMax,WIN_Y_MAX,zMin),
                   glm::vec3(xMax,WIN_Y_MAX,WIN_Z_MIN),glm::vec3(xMax,WIN_Y_MIN,WIN_Z_MIN),nr,
                   glm::vec2(0,vy0),glm::vec2(0,vy1),glm::vec2(uz0,vy1),glm::vec2(uz0,vy0));
        // Near side of window gap
        addQuad(wv,glm::vec3(xMax,WIN_Y_MIN,WIN_Z_MAX),glm::vec3(xMax,WIN_Y_MAX,WIN_Z_MAX),
                   glm::vec3(xMax,WIN_Y_MAX,zMax),    glm::vec3(xMax,WIN_Y_MIN,zMax),nr,
                   glm::vec2(uz1,vy0),glm::vec2(uz1,vy1),glm::vec2(wallL,vy1),glm::vec2(wallL,vy0));
    }

    addQuad(wv, glm::vec3(xMin,yMin,zMin), glm::vec3(xMax,yMin,zMin),
                glm::vec3(xMax,yMax,zMin), glm::vec3(xMin,yMax,zMin),
                glm::vec3(0,0,1),
                glm::vec2(0,0), glm::vec2(4,0), glm::vec2(4,wallH), glm::vec2(0,wallH));

    float lw = dxMin - xMin, rw = xMax - dxMax, topH = yMax - dyMax, dw = dxMax - dxMin;
    addQuad(wv, glm::vec3(xMin,yMin,zMax), glm::vec3(xMin,yMax,zMax),
                glm::vec3(dxMin,yMax,zMax), glm::vec3(dxMin,yMin,zMax),
                glm::vec3(0,0,-1),
                glm::vec2(0,0), glm::vec2(0,wallH), glm::vec2(lw,wallH), glm::vec2(lw,0));
    addQuad(wv, glm::vec3(dxMax,yMin,zMax), glm::vec3(dxMax,yMax,zMax),
                glm::vec3(xMax,yMax,zMax), glm::vec3(xMax,yMin,zMax),
                glm::vec3(0,0,-1),
                glm::vec2(0,0), glm::vec2(0,wallH), glm::vec2(rw,wallH), glm::vec2(rw,0));
    addQuad(wv, glm::vec3(dxMin,dyMax,zMax), glm::vec3(dxMin,yMax,zMax),
                glm::vec3(dxMax,yMax,zMax), glm::vec3(dxMax,dyMax,zMax),
                glm::vec3(0,0,-1),
                glm::vec2(0,0), glm::vec2(0,topH), glm::vec2(dw,topH), glm::vec2(dw,0));

    CorridorMesh m;
    m.floorVAO = uploadVAO(fv, m.floorVerts);
    m.ceilVAO  = uploadVAO(cv, m.ceilVerts);
    m.wallVAO  = uploadVAO(wv, m.wallVerts);
    return m;
}

// =============================================================
//  Wall mesh with tangent/bitangent  (NM layout, 14 floats/vertex)
//  Exactly mirrors the wall quads in buildCorridorMesh(), just with
//  per-quad T/B computed analytically from positions+UVs.
// =============================================================
struct WallNMMesh { unsigned int VAO; int cnt; };

static WallNMMesh buildWallNMMesh()
{
    const float xMin=-2.f, xMax=2.f, yMin=-1.f, yMax=2.5f, zMin=-13.f, zMax=1.f;
    const float dxMin=-0.85f, dxMax=0.85f, dyMax=1.9f;
    float wallH = yMax - yMin,  wallL = zMax - zMin;
    float lw    = dxMin - xMin, rw    = xMax - dxMax;
    float topH  = yMax - dyMax, dw    = dxMax - dxMin;

    std::vector<float> wv;

    // Left wall
    addQuad_NM(wv,
        glm::vec3(xMin,yMin,zMin), glm::vec3(xMin,yMin,zMax),
        glm::vec3(xMin,yMax,zMax), glm::vec3(xMin,yMax,zMin),
        glm::vec3(1,0,0),
        glm::vec2(0,0), glm::vec2(wallL,0), glm::vec2(wallL,wallH), glm::vec2(0,wallH));

    // Right wall — 4 quads around window opening
    {
        float uz0=WIN_Z_MIN-zMin, uz1=WIN_Z_MAX-zMin;
        float vy0=WIN_Y_MIN-yMin, vy1=WIN_Y_MAX-yMin;
        glm::vec3 nr(-1,0,0);

        addQuad_NM(wv,  // bottom strip
            glm::vec3(xMax,yMin,zMin),     glm::vec3(xMax,WIN_Y_MIN,zMin),
            glm::vec3(xMax,WIN_Y_MIN,zMax),glm::vec3(xMax,yMin,zMax), nr,
            glm::vec2(0,0),     glm::vec2(0,vy0),
            glm::vec2(wallL,vy0),glm::vec2(wallL,0));

        addQuad_NM(wv,  // top strip
            glm::vec3(xMax,WIN_Y_MAX,zMin),glm::vec3(xMax,yMax,zMin),
            glm::vec3(xMax,yMax,zMax),     glm::vec3(xMax,WIN_Y_MAX,zMax), nr,
            glm::vec2(0,vy1),    glm::vec2(0,wallH),
            glm::vec2(wallL,wallH),glm::vec2(wallL,vy1));

        addQuad_NM(wv,  // far side of window gap
            glm::vec3(xMax,WIN_Y_MIN,zMin),    glm::vec3(xMax,WIN_Y_MAX,zMin),
            glm::vec3(xMax,WIN_Y_MAX,WIN_Z_MIN),glm::vec3(xMax,WIN_Y_MIN,WIN_Z_MIN), nr,
            glm::vec2(0,vy0), glm::vec2(0,vy1),
            glm::vec2(uz0,vy1),glm::vec2(uz0,vy0));

        addQuad_NM(wv,  // near side of window gap
            glm::vec3(xMax,WIN_Y_MIN,WIN_Z_MAX),glm::vec3(xMax,WIN_Y_MAX,WIN_Z_MAX),
            glm::vec3(xMax,WIN_Y_MAX,zMax),     glm::vec3(xMax,WIN_Y_MIN,zMax), nr,
            glm::vec2(uz1,vy0),glm::vec2(uz1,vy1),
            glm::vec2(wallL,vy1),glm::vec2(wallL,vy0));
    }

    // Back wall (far end)
    addQuad_NM(wv,
        glm::vec3(xMin,yMin,zMin), glm::vec3(xMax,yMin,zMin),
        glm::vec3(xMax,yMax,zMin), glm::vec3(xMin,yMax,zMin),
        glm::vec3(0,0,1),
        glm::vec2(0,0), glm::vec2(4,0), glm::vec2(4,wallH), glm::vec2(0,wallH));

    // Front wall — 3 sections around door opening
    addQuad_NM(wv,
        glm::vec3(xMin,yMin,zMax),  glm::vec3(xMin,yMax,zMax),
        glm::vec3(dxMin,yMax,zMax), glm::vec3(dxMin,yMin,zMax),
        glm::vec3(0,0,-1),
        glm::vec2(0,0), glm::vec2(0,wallH), glm::vec2(lw,wallH), glm::vec2(lw,0));

    addQuad_NM(wv,
        glm::vec3(dxMax,yMin,zMax), glm::vec3(dxMax,yMax,zMax),
        glm::vec3(xMax,yMax,zMax),  glm::vec3(xMax,yMin,zMax),
        glm::vec3(0,0,-1),
        glm::vec2(0,0), glm::vec2(0,wallH), glm::vec2(rw,wallH), glm::vec2(rw,0));

    addQuad_NM(wv,
        glm::vec3(dxMin,dyMax,zMax), glm::vec3(dxMin,yMax,zMax),
        glm::vec3(dxMax,yMax,zMax),  glm::vec3(dxMax,dyMax,zMax),
        glm::vec3(0,0,-1),
        glm::vec2(0,0), glm::vec2(0,topH), glm::vec2(dw,topH), glm::vec2(dw,0));

    WallNMMesh m;
    m.VAO = uploadVAO_NM(wv, m.cnt);
    return m;
}

// =============================================================
//  Floor mesh with tangent/bitangent  (NM layout, 14 floats/vertex)
//  UV tiling matches buildCorridorMesh() floor: 4 × 14 repeats.
// =============================================================
struct FloorNMMesh { unsigned int VAO; int cnt; };
static FloorNMMesh buildFloorNMMesh()
{
    const float xMin=-2.f, xMax=2.f, yMin=-1.f, zMin=-13.f, zMax=1.f;
    std::vector<float> fv;
    addQuad_NM(fv,
        glm::vec3(xMin,yMin,zMin), glm::vec3(xMax,yMin,zMin),
        glm::vec3(xMax,yMin,zMax), glm::vec3(xMin,yMin,zMax),
        glm::vec3(0,1,0),
        glm::vec2(0,0), glm::vec2(4,0), glm::vec2(4,14), glm::vec2(0,14));
    FloorNMMesh m;
    m.VAO = uploadVAO_NM(fv, m.cnt);
    return m;
}

// =============================================================
//  Door VAO
// =============================================================
static unsigned int buildDoorVAO(int& cnt)
{
    std::vector<float> v;
    addQuad(v, glm::vec3(-0.85f,-1.0f,0.95f), glm::vec3(-0.85f,1.9f,0.95f),
               glm::vec3( 0.85f,1.9f,0.95f),  glm::vec3( 0.85f,-1.0f,0.95f),
               glm::vec3(0,0,-1),
               glm::vec2(0,1), glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1));
    return uploadVAO(v, cnt);

}

// =============================================================
//  Baseboard VAO  (dark accent strip at wall base)
// =============================================================
static unsigned int buildBaseVAO(int& cnt)
{
    const float xMin=-2.f, xMax=2.f, yMin=-1.f, yTop=-0.75f, zMin=-13.f, zMax=1.f;
    std::vector<float> v;
    addQuad(v, glm::vec3(xMin+.01f,yMin,zMin), glm::vec3(xMin+.01f,yMin,zMax),
               glm::vec3(xMin+.01f,yTop,zMax), glm::vec3(xMin+.01f,yTop,zMin),
               glm::vec3(1,0,0),
               glm::vec2(0,0), glm::vec2(14,0), glm::vec2(14,1), glm::vec2(0,1));
    addQuad(v, glm::vec3(xMax-.01f,yMin,zMin), glm::vec3(xMax-.01f,yTop,zMin),
               glm::vec3(xMax-.01f,yTop,zMax), glm::vec3(xMax-.01f,yMin,zMax),
               glm::vec3(-1,0,0),
               glm::vec2(0,0), glm::vec2(0,1), glm::vec2(14,1), glm::vec2(14,0));
    return uploadVAO(v, cnt);
}

// =============================================================
//  Dog house: body + gable roof  (14-float NM, wallNMShader)
//  cx,cz: world-space foot-print centre.  Placed in back-left corner.
//  Uses addQuad_NM throughout so brick normal map is sampled correctly.
//  Gable triangles: degenerate addQuad_NM (p2==p3==apex) — second
//  triangle has zero area and is invisible; addQuad_NM still derives
//  a correct T/B from the valid first triangle's edges+UVs.
// =============================================================
static unsigned int buildDogHouseVAO(float cx, float cz, int& cnt)
{
    const float yF  = -1.0f;       // floor level
    const float hx  =  0.28f;      // half-width (X)
    const float hz  =  0.22f;      // half-depth (Z)
    const float yT  = yF + 0.44f;  // top of walls  (-0.56)
    const float yA  = yF + 0.62f;  // roof apex     (-0.38)
    const float dw  =  0.11f;      // door half-width
    const float dh  =  0.30f;      // door height above floor
    const float ox  =  hx + 0.04f; // roof overhang in X (0.32)
    const float TILE = 4.0f;       // texture tiles per world metre

    const float x0 = cx - hx, x1 = cx + hx;
    const float z0 = cz - hz, z1 = cz + hz;
    const float dx0 = cx - dw,  dx1 = cx + dw;

    const float uX  = 2.f * hx * TILE;
    const float uZ  = 2.f * hz * TILE;
    const float uY  = (yT - yF) * TILE;
    const float dY   = yA - yT;                       // roof rise = 0.18 m
    const float sLen = sqrtf(hz*hz + dY*dY);          // slope length ≈ 0.284 m
    const glm::vec3 nBack  = glm::normalize(glm::vec3(0.f, hz, -dY));
    const glm::vec3 nFront = glm::normalize(glm::vec3(0.f, hz,  dY));
    const float uRX = 2.f * ox * TILE;
    const float uRS = sLen * TILE;

    std::vector<float> v;

    // ── Walls ────────────────────────────────────────────────

    // Back wall (z = z0, normal -Z)
    addQuad_NM(v, glm::vec3(x0,yF,z0), glm::vec3(x0,yT,z0),
                  glm::vec3(x1,yT,z0), glm::vec3(x1,yF,z0),
                  glm::vec3(0,0,-1),
                  glm::vec2(0,0), glm::vec2(0,uY), glm::vec2(uX,uY), glm::vec2(uX,0));

    // Left wall (x = x0, normal -X)
    addQuad_NM(v, glm::vec3(x0,yF,z0), glm::vec3(x0,yT,z0),
                  glm::vec3(x0,yT,z1), glm::vec3(x0,yF,z1),
                  glm::vec3(-1,0,0),
                  glm::vec2(0,0), glm::vec2(0,uY), glm::vec2(uZ,uY), glm::vec2(uZ,0));

    // Right wall (x = x1, normal +X)
    addQuad_NM(v, glm::vec3(x1,yF,z1), glm::vec3(x1,yT,z1),
                  glm::vec3(x1,yT,z0), glm::vec3(x1,yF,z0),
                  glm::vec3(1,0,0),
                  glm::vec2(0,0), glm::vec2(0,uY), glm::vec2(uZ,uY), glm::vec2(uZ,0));

    // Front wall (z = z1, normal +Z) — 3 panels around door opening
    {
        const float pL = (dx0 - x0) * TILE;        // left panel UV width
        const float pR = (x1 - dx1) * TILE;        // right panel UV width
        const float pD = 2.f * dw * TILE;           // door span UV width
        const float pT = (yT - (yF + dh)) * TILE;  // beam UV height

        // Left panel
        addQuad_NM(v, glm::vec3(x0, yF,    z1), glm::vec3(x0, yT,    z1),
                      glm::vec3(dx0,yT,    z1), glm::vec3(dx0,yF,    z1),
                      glm::vec3(0,0,1),
                      glm::vec2(0,0), glm::vec2(0,uY), glm::vec2(pL,uY), glm::vec2(pL,0));
        // Right panel
        addQuad_NM(v, glm::vec3(dx1,yF,    z1), glm::vec3(dx1,yT,    z1),
                      glm::vec3(x1, yT,    z1), glm::vec3(x1, yF,    z1),
                      glm::vec3(0,0,1),
                      glm::vec2(0,0), glm::vec2(0,uY), glm::vec2(pR,uY), glm::vec2(pR,0));
        // Top beam above door
        addQuad_NM(v, glm::vec3(dx0,yF+dh,z1), glm::vec3(dx0,yT,   z1),
                      glm::vec3(dx1,yT,   z1),  glm::vec3(dx1,yF+dh,z1),
                      glm::vec3(0,0,1),
                      glm::vec2(0,0), glm::vec2(0,pT), glm::vec2(pD,pT), glm::vec2(pD,0));
    }

    // ── Gable roof ───────────────────────────────────────────

    // Back slope (eave at z0,yT → ridge at cz,yA)
    addQuad_NM(v, glm::vec3(cx-ox,yT,z0), glm::vec3(cx+ox,yT,z0),
                  glm::vec3(cx+ox,yA,cz), glm::vec3(cx-ox,yA,cz),
                  nBack,
                  glm::vec2(0,0), glm::vec2(uRX,0), glm::vec2(uRX,uRS), glm::vec2(0,uRS));

    // Front slope (ridge at cz,yA → eave at z1,yT)
    addQuad_NM(v, glm::vec3(cx-ox,yA,cz), glm::vec3(cx+ox,yA,cz),
                  glm::vec3(cx+ox,yT,z1), glm::vec3(cx-ox,yT,z1),
                  nFront,
                  glm::vec2(0,0), glm::vec2(uRX,0), glm::vec2(uRX,uRS), glm::vec2(0,uRS));

    // Left gable triangle (x = cx-ox, normal -X)
    // p2==p3==apex → triangle 2 is degenerate (zero area); addQuad_NM derives
    // T=(0,0,1), B=(0,1,0) from the first triangle's edges, which is correct.
    {
        const float uZg = 2.f * hz * TILE, uYg = dY * TILE;
        const glm::vec2 apex_uv(uZg * 0.5f, uYg);
        addQuad_NM(v,
            glm::vec3(cx-ox,yT,z0), glm::vec3(cx-ox,yT,z1),
            glm::vec3(cx-ox,yA,cz), glm::vec3(cx-ox,yA,cz),   // p2 == p3
            glm::vec3(-1,0,0),
            glm::vec2(0,0), glm::vec2(uZg,0), apex_uv, apex_uv);
    }

    // Right gable triangle (x = cx+ox, normal +X)
    // T=(0,0,-1), B=(0,1,0) — symmetric to left gable.
    {
        const float uZg = 2.f * hz * TILE, uYg = dY * TILE;
        const glm::vec2 apex_uv(uZg * 0.5f, uYg);
        addQuad_NM(v,
            glm::vec3(cx+ox,yT,z1), glm::vec3(cx+ox,yT,z0),
            glm::vec3(cx+ox,yA,cz), glm::vec3(cx+ox,yA,cz),   // p2 == p3
            glm::vec3(1,0,0),
            glm::vec2(0,0), glm::vec2(uZg,0), apex_uv, apex_uv);
    }

    return uploadVAO_NM(v, cnt);
}

// Dark interior panel visible through the door opening
static unsigned int buildDogHouseDoorVAO(float cx, float cz, int& cnt)
{
    const float yF    = -1.0f;
    const float hz    =  0.22f;
    const float dw    =  0.11f;
    const float dh    =  0.30f;
    const float inset =  0.05f;   // how far behind the front face the panel sits
    const float z1    = cz + hz;

    std::vector<float> v;
    addQuad(v,
        glm::vec3(cx-dw, yF,    z1-inset),
        glm::vec3(cx-dw, yF+dh, z1-inset),
        glm::vec3(cx+dw, yF+dh, z1-inset),
        glm::vec3(cx+dw, yF,    z1-inset),
        glm::vec3(0,0,1),
        glm::vec2(0,0), glm::vec2(0,1), glm::vec2(1,1), glm::vec2(1,0));
    return uploadVAO(v, cnt);
}

// =============================================================
//  Glass vase  (8-float, 16-sided procedural cylinder)
//  cx,cy,cz: centre of vase base at floor level
// =============================================================
static unsigned int buildVaseVAO(float cx, float cy, float cz, int& cnt)
{
    const int   SEGS = 16;
    const float PI   = 3.14159265f;

    // Profile rings: (y offset from base, outer radius)
    const int NR = 9;
    const float pY[NR] = { 0.00f, 0.00f, 0.08f, 0.20f, 0.35f,
                            0.44f, 0.50f, 0.54f, 0.56f };
    const float pR[NR] = { 0.000f, 0.055f, 0.065f, 0.072f, 0.068f,
                            0.042f, 0.028f, 0.038f, 0.032f };
    // Ring 0 = bottom centre (cap), rings 1..NR-1 = side profile

    std::vector<float> v;

    auto push = [&](glm::vec3 p, glm::vec3 n, float u, float vv){
        v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
        v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
        v.push_back(u);   v.push_back(vv);
    };

    // Side quads between adjacent profile rings (1 .. NR-1)
    for (int ri = 1; ri < NR-1; ri++) {
        float r0=pR[ri],  y0v=cy+pY[ri],  vc0=pY[ri]/pY[NR-1];
        float r1=pR[ri+1],y1v=cy+pY[ri+1],vc1=pY[ri+1]/pY[NR-1];
        for (int si = 0; si < SEGS; si++) {
            float a0=(float)si    *2.f*PI/SEGS;
            float a1=(float)(si+1)*2.f*PI/SEGS;
            float u0=(float)si    /SEGS;
            float u1=(float)(si+1)/SEGS;
            glm::vec3 n0(cosf(a0),0,sinf(a0));
            glm::vec3 n1(cosf(a1),0,sinf(a1));
            glm::vec3 p00(cx+r0*cosf(a0),y0v,cz+r0*sinf(a0));
            glm::vec3 p01(cx+r0*cosf(a1),y0v,cz+r0*sinf(a1));
            glm::vec3 p10(cx+r1*cosf(a0),y1v,cz+r1*sinf(a0));
            glm::vec3 p11(cx+r1*cosf(a1),y1v,cz+r1*sinf(a1));
            push(p00,n0,u0,vc0); push(p10,n0,u0,vc1); push(p11,n1,u1,vc1);
            push(p00,n0,u0,vc0); push(p11,n1,u1,vc1); push(p01,n1,u1,vc0);
        }
    }
    // Bottom cap (triangle fan, facing -Y)
    {
        float yBot=cy+pY[1], r=pR[1];
        glm::vec3 nb(0,-1,0);
        for (int si = 0; si < SEGS; si++) {
            float a0=(float)si    *2.f*PI/SEGS;
            float a1=(float)(si+1)*2.f*PI/SEGS;
            push(glm::vec3(cx,yBot,cz),                    nb,0.5f,0.5f);
            push(glm::vec3(cx+r*cosf(a1),yBot,cz+r*sinf(a1)),nb,
                 (cosf(a1)+1.f)*0.5f,(sinf(a1)+1.f)*0.5f);
            push(glm::vec3(cx+r*cosf(a0),yBot,cz+r*sinf(a0)),nb,
                 (cosf(a0)+1.f)*0.5f,(sinf(a0)+1.f)*0.5f);
        }
    }
    return uploadVAO(v, cnt);
}

// =============================================================
//  City skyline texture (visible through window)
// =============================================================
static unsigned int genCityTex()
{
    const int W = 512, H = 256;
    std::vector<unsigned char> d(W * H * 3);
    // Dusk sky gradient
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float fy = (float)y / H;
        d[i]   = (unsigned char)(15 + 55 * fy);
        d[i+1] = (unsigned char)(10 + 35 * fy);
        d[i+2] = (unsigned char)(55 + 90 * fy);
    }
    // Buildings
    for (int bx = 0; bx < W; bx++) {
        int grp = bx / 32;
        int bh = 90 + (grp * 31 + (bx % 32) * 7) % 110;
        for (int y = H - bh; y < H; y++) {
            int i = (y * W + bx) * 3;
            unsigned char bc = 30 + (bx / 32 + y / 20) % 22;
            d[i] = bc; d[i+1] = bc; d[i+2] = (unsigned char)(bc + 10);
            if (y > H - bh + 6 && (bx % 9 < 5) && (y % 16 < 8)) {
                d[i] = 215; d[i+1] = 205; d[i+2] = 135;
            }
        }
    }
    // Neon signs: red, green, yellow
    for (int y = H-100; y < H-82; y++) for (int x = 45; x < 80; x++) {
        int i=(y*W+x)*3; d[i]=230; d[i+1]=30; d[i+2]=30;
    }
    for (int y = H-115; y < H-97; y++) for (int x = 190; x < 225; x++) {
        int i=(y*W+x)*3; d[i]=30; d[i+1]=210; d[i+2]=70;
    }
    for (int y = H-88; y < H-72; y++) for (int x = 330; x < 365; x++) {
        int i=(y*W+x)*3; d[i]=230; d[i+1]=200; d[i+2]=30;
    }
    return makeTexture(d, W, H, false);
}

// =============================================================
//  Procedural textures for 3D props
// =============================================================

// Terracotta: orange-brown with subtle surface variation
static unsigned int genTerraCottaTex()
{
    const int W = 128, H = 128;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float n  = 1.f - 0.10f * float((x*11 + y*7) % 17) / 16.f;
        float ry = 1.f - 0.05f * float((y*13) % 19) / 18.f;
        float f  = n * ry;
        d[i]   = (unsigned char)(198 * f);
        d[i+1] = (unsigned char)(102 * f);
        d[i+2] = (unsigned char)( 60 * f);
    }
    return makeTexture(d, W, H);
}

// Leaf: deep green with a central vein + lateral veins
static unsigned int genLeafTex()
{
    const int W = 128, H = 256;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float n = 1.f - 0.08f * float((x*3 + y*11) % 13) / 12.f;
        // Central mid-vein + lateral veins every 18 rows
        bool vein = (x == W/2 || x == W/2+1) ||
                    (y % 18 < 2 && x > W/4 && x < 3*W/4);
        d[i]   = vein ? 80  : (unsigned char)(38  * n);
        d[i+1] = vein ? 160 : (unsigned char)(138 * n);
        d[i+2] = vein ? 60  : (unsigned char)(44  * n);
    }
    return makeTexture(d, W, H);
}

// Wood: warm honey-brown with horizontal grain lines
static unsigned int genWoodTex()
{
    const int W = 256, H = 256;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float grain = 1.f - 0.18f * float((y*5 + x/8) % 11) / 10.f;
        float knot  = 1.f - 0.07f * float((x*7 + y*3) % 17) / 16.f;
        float f = grain * knot;
        d[i]   = (unsigned char)(178 * f);
        d[i+1] = (unsigned char)(118 * f);
        d[i+2] = (unsigned char)( 62 * f);
    }
    return makeTexture(d, W, H);
}

// Shiba Inu golden coat — warm orange-gold with multi-layer fur noise
static unsigned int genDogFurTex()
{
    const int W = 128, H = 128;
    std::vector<unsigned char> d(W * H * 3);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int i = (y * W + x) * 3;
        float base   = 1.f - 0.12f * float((x * 7  + y * 11) % 17) / 16.f;
        float fine   = 1.f - 0.06f * float((x * 3  + y * 29) % 13) / 12.f;
        float streak = 1.f - 0.08f * float(((x + y / 3) * 5) % 11) / 10.f;
        float f = base * fine * streak;
        d[i]   = (unsigned char)(218 * f);   // warm orange-gold
        d[i+1] = (unsigned char)(142 * f);
        d[i+2] = (unsigned char)( 55 * f);
    }
    return makeTexture(d, W, H);
}

// =============================================================
//  Procedural tangent-space normal map for the corridor walls
//  Features:
//    1. Horizontal recessed panel grooves (matches diffuse panel lines)
//    2. Multi-octave stucco surface noise (4 harmonics, weighted)
//    3. Encoded as (nx*0.5+0.5, ny*0.5+0.5, nz*0.5+0.5) in RGB
// =============================================================
static unsigned int genWallNormalMap()
{
    const int W = 512, H = 512;
    const int PANEL_H = 256;   // matches genWallTex() 256-pixel panel period
    const int GW      = 6;     // groove influence width (pixels)
    std::vector<unsigned char> d(W * H * 3);

    // Cheap but spatially-uncorrelated integer hash (Wang hash variant)
    auto hashF = [](unsigned int v) -> float {
        v ^= v >> 16; v *= 0x45d9f3bu;
        v ^= v >> 16; v *= 0x45d9f3bu;
        v ^= v >> 16;
        return float(v & 0xFFFF) / 65535.f - 0.5f;  // [-0.5, 0.5)
    };

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = (y * W + x) * 3;

            float nx = 0.f, ny = 0.f;

            // ---- Panel groove edges ----
            // Groove band at py < 3 (matching diffuse darker line).
            // Normals tilt toward the groove center on each side.
            int py = y % PANEL_H;

            if (py < GW) {
                // Lower boundary: normal tilts in -V direction (into groove)
                float t = 1.f - (float)py / GW;
                ny -= 0.85f * (t * t);   // quadratic fade for smooth bevel
            }
            else if (py >= PANEL_H - GW) {
                // Upper boundary: normal tilts in +V direction (into groove)
                float t = (float)(py - (PANEL_H - GW)) / GW;
                ny += 0.85f * (t * t);
            }

            // ---- Multi-octave stucco surface noise ----
            // 4 harmonics with decreasing amplitude (lacunarity ≈ 2)
            // Using prime multipliers to decorrelate axes and octaves
            unsigned int ux = (unsigned int)x, uy = (unsigned int)y;

            float f0 = hashF(ux * 7u   + uy * 13u);
            float f1 = hashF(ux * 17u  + uy * 3u);
            float f2 = hashF(ux * 5u   + uy * 23u);
            float f3 = hashF(ux * 31u  + uy * 7u);
            float g0 = hashF(ux * 11u  + uy * 19u);
            float g1 = hashF(ux * 3u   + uy * 29u);
            float g2 = hashF(ux * 41u  + uy * 11u);
            float g3 = hashF(ux * 13u  + uy * 37u);

            // Weighted octave sum — amplitude halves each octave
            float snx = (f0*0.500f + f1*0.250f + f2*0.125f + f3*0.062f) * 0.14f;
            float sny = (g0*0.500f + g1*0.250f + g2*0.125f + g3*0.062f) * 0.11f;

            nx += snx;
            ny += sny;

            // ---- Reconstruct Z component (unit vector in tangent space) ----
            float r2 = nx * nx + ny * ny;
            float nz = (r2 < 1.f) ? sqrtf(1.f - r2) : 0.001f;

            // ---- Encode to [0, 255] ----
            // Flat tangent-space normal = (0,0,1) → (128,128,255)
            d[i]   = (unsigned char)((nx * 0.5f + 0.5f) * 255.f + 0.5f);
            d[i+1] = (unsigned char)((ny * 0.5f + 0.5f) * 255.f + 0.5f);
            d[i+2] = (unsigned char)((nz * 0.5f + 0.5f) * 255.f + 0.5f);
        }
    }
    return makeTexture(d, W, H);   // GL_REPEAT, no gamma correction
}

// City background plane placed just outside the window gap
static unsigned int buildCityBackVAO(int& cnt)
{
    float x = 2.35f;
    std::vector<float> v;
    addQuad(v, glm::vec3(x,WIN_Y_MIN,WIN_Z_MIN), glm::vec3(x,WIN_Y_MAX,WIN_Z_MIN),
               glm::vec3(x,WIN_Y_MAX,WIN_Z_MAX), glm::vec3(x,WIN_Y_MIN,WIN_Z_MAX),
               glm::vec3(-1,0,0),
               glm::vec2(0,0), glm::vec2(0,1), glm::vec2(1,1), glm::vec2(1,0));
    return uploadVAO(v, cnt);
}

// Glass pane sitting just in front of the wall at the window gap
static unsigned int buildWindowGlassVAO(int& cnt)
{
    float x = 2.01f;
    std::vector<float> v;
    addQuad(v, glm::vec3(x,WIN_Y_MIN,WIN_Z_MIN), glm::vec3(x,WIN_Y_MAX,WIN_Z_MIN),
               glm::vec3(x,WIN_Y_MAX,WIN_Z_MAX), glm::vec3(x,WIN_Y_MIN,WIN_Z_MAX),
               glm::vec3(-1,0,0),
               glm::vec2(0,0), glm::vec2(0,1), glm::vec2(1,1), glm::vec2(1,0));
    return uploadVAO(v, cnt);
}

// =============================================================
//  Prop geometry helpers
// =============================================================

// Axis-aligned box: 6 faces, pos+normal+uv per vertex
static void pushBox(std::vector<float>& v,
                    glm::vec3 c, float hx, float hy, float hz)
{
    // +X
    addQuad(v, glm::vec3(c.x+hx,c.y-hy,c.z-hz), glm::vec3(c.x+hx,c.y+hy,c.z-hz),
               glm::vec3(c.x+hx,c.y+hy,c.z+hz), glm::vec3(c.x+hx,c.y-hy,c.z+hz),
               glm::vec3(1,0,0),
               glm::vec2(0,0),glm::vec2(0,1),glm::vec2(1,1),glm::vec2(1,0));
    // -X
    addQuad(v, glm::vec3(c.x-hx,c.y-hy,c.z+hz), glm::vec3(c.x-hx,c.y+hy,c.z+hz),
               glm::vec3(c.x-hx,c.y+hy,c.z-hz), glm::vec3(c.x-hx,c.y-hy,c.z-hz),
               glm::vec3(-1,0,0),
               glm::vec2(0,0),glm::vec2(0,1),glm::vec2(1,1),glm::vec2(1,0));
    // +Y
    addQuad(v, glm::vec3(c.x-hx,c.y+hy,c.z-hz), glm::vec3(c.x-hx,c.y+hy,c.z+hz),
               glm::vec3(c.x+hx,c.y+hy,c.z+hz), glm::vec3(c.x+hx,c.y+hy,c.z-hz),
               glm::vec3(0,1,0),
               glm::vec2(0,0),glm::vec2(0,1),glm::vec2(1,1),glm::vec2(1,0));
    // -Y
    addQuad(v, glm::vec3(c.x-hx,c.y-hy,c.z+hz), glm::vec3(c.x-hx,c.y-hy,c.z-hz),
               glm::vec3(c.x+hx,c.y-hy,c.z-hz), glm::vec3(c.x+hx,c.y-hy,c.z+hz),
               glm::vec3(0,-1,0),
               glm::vec2(0,0),glm::vec2(0,1),glm::vec2(1,1),glm::vec2(1,0));
    // +Z
    addQuad(v, glm::vec3(c.x+hx,c.y-hy,c.z+hz), glm::vec3(c.x+hx,c.y+hy,c.z+hz),
               glm::vec3(c.x-hx,c.y+hy,c.z+hz), glm::vec3(c.x-hx,c.y-hy,c.z+hz),
               glm::vec3(0,0,1),
               glm::vec2(0,0),glm::vec2(0,1),glm::vec2(1,1),glm::vec2(1,0));
    // -Z
    addQuad(v, glm::vec3(c.x-hx,c.y-hy,c.z-hz), glm::vec3(c.x-hx,c.y+hy,c.z-hz),
               glm::vec3(c.x+hx,c.y+hy,c.z-hz), glm::vec3(c.x+hx,c.y-hy,c.z-hz),
               glm::vec3(0,0,-1),
               glm::vec2(0,0),glm::vec2(0,1),glm::vec2(1,1),glm::vec2(1,0));
}

// Normal-mapped box  (14-float NM vertex format, same as wallNMShader)
// texScale: how many texture tiles per world-unit of surface area (default = 8)
// UVs are physically scaled so larger faces don't show stretched textures.
static void pushBoxNM(std::vector<float>& v, glm::vec3 c, float hx, float hy, float hz,
                      float texScale = 8.0f)
{
    float sX = 2.f * hx * texScale;
    float sY = 2.f * hy * texScale;
    float sZ = 2.f * hz * texScale;
    // +X face: U along Z, V along Y
    addQuad_NM(v,
        glm::vec3(c.x+hx,c.y-hy,c.z-hz), glm::vec3(c.x+hx,c.y+hy,c.z-hz),
        glm::vec3(c.x+hx,c.y+hy,c.z+hz), glm::vec3(c.x+hx,c.y-hy,c.z+hz),
        glm::vec3(1,0,0),
        glm::vec2(0,0), glm::vec2(0,sY), glm::vec2(sZ,sY), glm::vec2(sZ,0));
    // -X face: U along Z, V along Y
    addQuad_NM(v,
        glm::vec3(c.x-hx,c.y-hy,c.z+hz), glm::vec3(c.x-hx,c.y+hy,c.z+hz),
        glm::vec3(c.x-hx,c.y+hy,c.z-hz), glm::vec3(c.x-hx,c.y-hy,c.z-hz),
        glm::vec3(-1,0,0),
        glm::vec2(0,0), glm::vec2(0,sY), glm::vec2(sZ,sY), glm::vec2(sZ,0));
    // +Y face: U along X, V along Z
    addQuad_NM(v,
        glm::vec3(c.x-hx,c.y+hy,c.z-hz), glm::vec3(c.x-hx,c.y+hy,c.z+hz),
        glm::vec3(c.x+hx,c.y+hy,c.z+hz), glm::vec3(c.x+hx,c.y+hy,c.z-hz),
        glm::vec3(0,1,0),
        glm::vec2(0,0), glm::vec2(0,sZ), glm::vec2(sX,sZ), glm::vec2(sX,0));
    // -Y face: U along X, V along Z
    addQuad_NM(v,
        glm::vec3(c.x-hx,c.y-hy,c.z+hz), glm::vec3(c.x-hx,c.y-hy,c.z-hz),
        glm::vec3(c.x+hx,c.y-hy,c.z-hz), glm::vec3(c.x+hx,c.y-hy,c.z+hz),
        glm::vec3(0,-1,0),
        glm::vec2(0,0), glm::vec2(0,sZ), glm::vec2(sX,sZ), glm::vec2(sX,0));
    // +Z face: U along X, V along Y
    addQuad_NM(v,
        glm::vec3(c.x+hx,c.y-hy,c.z+hz), glm::vec3(c.x+hx,c.y+hy,c.z+hz),
        glm::vec3(c.x-hx,c.y+hy,c.z+hz), glm::vec3(c.x-hx,c.y-hy,c.z+hz),
        glm::vec3(0,0,1),
        glm::vec2(0,0), glm::vec2(0,sY), glm::vec2(sX,sY), glm::vec2(sX,0));
    // -Z face: U along X, V along Y
    addQuad_NM(v,
        glm::vec3(c.x-hx,c.y-hy,c.z-hz), glm::vec3(c.x-hx,c.y+hy,c.z-hz),
        glm::vec3(c.x+hx,c.y+hy,c.z-hz), glm::vec3(c.x+hx,c.y-hy,c.z-hz),
        glm::vec3(0,0,-1),
        glm::vec2(0,0), glm::vec2(0,sY), glm::vec2(sX,sY), glm::vec2(sX,0));
}

// Truncated cone: base center at 'base', rises +Y by 'ht'
// botR/topR are bottom/top radii; set topR==botR for a cylinder
static void pushCone(std::vector<float>& v, glm::vec3 base,
                     float botR, float topR, float ht, int slices)
{
    const float PI2 = 6.28318530f;
    glm::vec3 top = base + glm::vec3(0.f, ht, 0.f);
    float slope = atan2f(botR - topR, ht);   // taper angle from vertical
    float cosS  = cosf(slope), sinS = sinf(slope);

    for (int i = 0; i < slices; i++) {
        float a0 = i       * PI2 / slices;
        float a1 = (i + 1) * PI2 / slices;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        glm::vec3 b0 = base + glm::vec3(c0*botR, 0.f, s0*botR);
        glm::vec3 b1 = base + glm::vec3(c1*botR, 0.f, s1*botR);
        glm::vec3 t0 = top  + glm::vec3(c0*topR, 0.f, s0*topR);
        glm::vec3 t1 = top  + glm::vec3(c1*topR, 0.f, s1*topR);
        glm::vec3 n0(c0*cosS, sinS, s0*cosS);
        glm::vec3 n1(c1*cosS, sinS, s1*cosS);
        float u0 = (float)i / slices, u1 = (float)(i+1) / slices;

        // Side quad
        pushV(v, b0, n0, glm::vec2(u0, 0.f)); pushV(v, b1, n1, glm::vec2(u1, 0.f));
        pushV(v, t1, n1, glm::vec2(u1, 1.f));
        pushV(v, b0, n0, glm::vec2(u0, 0.f)); pushV(v, t1, n1, glm::vec2(u1, 1.f));
        pushV(v, t0, n0, glm::vec2(u0, 1.f));

        // Bottom cap
        glm::vec3 nB(0.f, -1.f, 0.f);
        pushV(v, base, nB, glm::vec2(.5f, .5f));
        pushV(v, b1,   nB, glm::vec2(.5f + c1*.5f, .5f + s1*.5f));
        pushV(v, b0,   nB, glm::vec2(.5f + c0*.5f, .5f + s0*.5f));

        // Top cap
        glm::vec3 nT(0.f, 1.f, 0.f);
        pushV(v, top, nT, glm::vec2(.5f, .5f));
        pushV(v, t0,  nT, glm::vec2(.5f + c0*.5f, .5f + s0*.5f));
        pushV(v, t1,  nT, glm::vec2(.5f + c1*.5f, .5f + s1*.5f));
    }
}

// Normal-mapped truncated cone (14-float NM vertex format)
// Side quads use addQuad_NM so T/B are derived from geometry+UVs.
// Caps use fixed horizontal T/B (cap normal = ±Y, so NM has no meaningful effect there).
static void pushConeNM(std::vector<float>& v, glm::vec3 base,
                       float botR, float topR, float ht, int slices)
{
    const float PI2 = 6.28318530f;
    glm::vec3 top = base + glm::vec3(0.f, ht, 0.f);
    float slope = atan2f(botR - topR, ht);
    float cosS  = cosf(slope), sinS = sinf(slope);
    const glm::vec3 T_cap(1.f,0.f,0.f), B_cap(0.f,0.f,1.f);

    for (int i = 0; i < slices; i++) {
        float a0 = i       * PI2 / slices;
        float a1 = (i + 1) * PI2 / slices;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        glm::vec3 b0 = base + glm::vec3(c0*botR, 0.f, s0*botR);
        glm::vec3 b1 = base + glm::vec3(c1*botR, 0.f, s1*botR);
        glm::vec3 t0 = top  + glm::vec3(c0*topR, 0.f, s0*topR);
        glm::vec3 t1 = top  + glm::vec3(c1*topR, 0.f, s1*topR);
        // Per-face average normal (smooth shading approximation)
        glm::vec3 nMid = glm::normalize(glm::vec3(
            (c0+c1)*0.5f*cosS, sinS, (s0+s1)*0.5f*cosS));

        float u0 = (float)i       / slices;
        float u1 = (float)(i + 1) / slices;
        // Side quad: b0(u0,0), b1(u1,0), t1(u1,1), t0(u0,1)
        // addQuad_NM derives T/B from the actual positions+UVs
        addQuad_NM(v, b0, b1, t1, t0, nMid,
                   glm::vec2(u0, 0.f), glm::vec2(u1, 0.f),
                   glm::vec2(u1, 1.f), glm::vec2(u0, 1.f));

        // Bottom cap (N = -Y, fixed horizontal T/B)
        glm::vec3 nB(0.f,-1.f,0.f);
        pushV_NM(v, base, nB, glm::vec2(.5f,.5f),                         T_cap, B_cap);
        pushV_NM(v, b1,   nB, glm::vec2(.5f+c1*.5f, .5f+s1*.5f),         T_cap, B_cap);
        pushV_NM(v, b0,   nB, glm::vec2(.5f+c0*.5f, .5f+s0*.5f),         T_cap, B_cap);
        // Top cap (N = +Y)
        glm::vec3 nT(0.f,1.f,0.f);
        pushV_NM(v, top,  nT, glm::vec2(.5f,.5f),                         T_cap, B_cap);
        pushV_NM(v, t0,   nT, glm::vec2(.5f+c0*.5f, .5f+s0*.5f),         T_cap, B_cap);
        pushV_NM(v, t1,   nT, glm::vec2(.5f+c1*.5f, .5f+s1*.5f),         T_cap, B_cap);
    }
}

// Single two-sided triangular leaf blade fanning outward+upward from origin
static void pushLeafBlade(std::vector<float>& v, glm::vec3 origin,
                          float angle, float outR, float upH, float halfW)
{
    float ca = cosf(angle), sa = sinf(angle);
    glm::vec3 perp(-sa, 0.f, ca);
    glm::vec3 bl  = origin + perp *  halfW;
    glm::vec3 br  = origin - perp *  halfW;
    glm::vec3 tip = origin + glm::vec3(ca * outR, upH, sa * outR);

    glm::vec3 n  = glm::normalize(glm::cross(br - bl, tip - bl));
    glm::vec3 nb = -n;

    // Front face
    pushV(v, bl,  n,  glm::vec2(0.f,  0.f));
    pushV(v, br,  n,  glm::vec2(1.f,  0.f));
    pushV(v, tip, n,  glm::vec2(0.5f, 1.f));
    // Back face (reverse winding, flipped normal)
    pushV(v, bl,  nb, glm::vec2(0.f,  0.f));
    pushV(v, tip, nb, glm::vec2(0.5f, 1.f));
    pushV(v, br,  nb, glm::vec2(1.f,  0.f));
}

// =============================================================
//  Potted plant  (pot + soil  /  leaf blades — two separate VAOs)
// =============================================================
struct PlantMesh {
    unsigned int potVAO;
    unsigned int leafVAO;
    int potCnt;
    int leafCnt;
};

static PlantMesh buildPlantMesh(float px, float pz)
{
    const float FLOOR_Y = -1.0f;
    const int   SLICES  = 24;
    const float PI2     = 6.28318530f;

    // ---- Pot + rim + soil (terracotta-coloured) ----
    std::vector<float> pv;

    // Main pot body: wider at top (0.14 -> 0.19 radius, 0.28 tall)
    pushCone(pv, glm::vec3(px, FLOOR_Y, pz),          0.14f, 0.19f, 0.28f,  SLICES);
    // Decorative rim band on top of pot
    pushCone(pv, glm::vec3(px, FLOOR_Y + 0.28f, pz),  0.20f, 0.19f, 0.025f, SLICES);
    // Soil disk: flat cylinder flush with rim interior
    pushCone(pv, glm::vec3(px, FLOOR_Y + 0.305f, pz), 0.185f,0.185f,0.015f, SLICES);

    PlantMesh m;
    m.potVAO = uploadVAO(pv, m.potCnt);

    // ---- Leaf blades (two-sided, green-textured) ----
    std::vector<float> lv;
    glm::vec3 leafOrigin(px, FLOOR_Y + 0.32f, pz);

    // Outer ring: 10 long arching blades
    for (int i = 0; i < 10; i++) {
        float angle = i * PI2 / 10.f;
        pushLeafBlade(lv, leafOrigin, angle, 0.36f, 0.55f, 0.058f);
    }
    // Inner ring: 6 shorter more-upright blades (staggered)
    for (int i = 0; i < 6; i++) {
        float angle = i * PI2 / 6.f + PI2 / 12.f;
        pushLeafBlade(lv, leafOrigin, angle, 0.16f, 0.72f, 0.036f);
    }
    // Central upright spikes
    for (int i = 0; i < 3; i++) {
        float angle = i * PI2 / 3.f;
        pushLeafBlade(lv, leafOrigin, angle, 0.05f, 0.85f, 0.020f);
    }

    m.leafVAO = uploadVAO(lv, m.leafCnt);
    return m;
}

// corridorShader 규약 드로우: 명시적 diffuse 텍스처 → GL_TEXTURE0.
// units 1/2/3 (shadow map) 은 건드리지 않는다.
static void drawPlantFlat(Model& mdl, unsigned int texDiffuse)
{
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texDiffuse);
    for (unsigned int i = 0; i < mdl.meshes.size(); i++) {
        glBindVertexArray(mdl.meshes[i].VAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<unsigned int>(mdl.meshes[i].indices.size()),
                       GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

// wallNMShader 규약 드로우: unit 0 = diffuse, unit 1 = normal map.
// units 2/3/4 (shadow map) 는 호출자가 미리 바인딩.
// wall_nm.vs location 0~4 (pos/normal/uv/tangent/bitangent) 가 assimp VAO 와 일치.
static void drawPlantNM(Model& mdl, unsigned int texColor, unsigned int texNM)
{
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texColor);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texNM);
    for (unsigned int i = 0; i < mdl.meshes.size(); i++) {
        glBindVertexArray(mdl.meshes[i].VAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<unsigned int>(mdl.meshes[i].indices.size()),
                       GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

// OBJ 메쉬 순서: [0..N-2]=잎(Blatt/Blatt_NONE), [N-1]=화분(Material)
// 잎 부분만 드로우 (마지막 메쉬 제외)
static void drawPlantLeavesNM(Model& mdl, unsigned int texColor, unsigned int texNM)
{
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texColor);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texNM);
    for (unsigned int i = 0; i + 1 < mdl.meshes.size(); i++) {
        glBindVertexArray(mdl.meshes[i].VAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<unsigned int>(mdl.meshes[i].indices.size()),
                       GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

// 화분 부분만 드로우 (마지막 메쉬만)
static void drawPlantPotNM(Model& mdl, unsigned int texColor, unsigned int texNM)
{
    if (mdl.meshes.empty()) return;
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texColor);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texNM);
    unsigned int last = static_cast<unsigned int>(mdl.meshes.size()) - 1;
    glBindVertexArray(mdl.meshes[last].VAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<unsigned int>(mdl.meshes[last].indices.size()),
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// =============================================================
//  Wooden chair facing +X (back against left wall at x=-2)
//  cx: seat center X,  cz: seat center Z
// =============================================================
static unsigned int buildChairVAO(float cx, float cz, int& cnt)
{
    const float FLOOR_Y = -1.0f;
    std::vector<float> v;

    // Seat board (symmetric — same in both orientations)
    pushBox(v, glm::vec3(cx, FLOOR_Y + 0.455f, cz), 0.210f, 0.022f, 0.210f);

    // Backrest panel: thin in X, tall in Y, wide in Z
    // Center is 0.188m toward -X from seat center (back against left wall)
    float backX = cx - 0.188f;
    float backY  = FLOOR_Y + 0.455f + 0.022f + 0.270f;
    pushBox(v, glm::vec3(backX, backY, cz),               0.022f, 0.270f, 0.210f);

    // Decorative top rail
    float railY = FLOOR_Y + 0.455f + 0.022f + 0.270f + 0.270f - 0.025f;
    pushBox(v, glm::vec3(backX, railY, cz),                0.032f, 0.025f, 0.210f);

    // 4 Legs: depth along X (lx), width along Z (lz)
    float legHY = (0.455f - 0.022f) * 0.5f;
    float legCY = FLOOR_Y + legHY;
    float lx = 0.175f, lz = 0.175f;
    // Front legs (toward corridor, +X side)
    pushBox(v, glm::vec3(cx+lx, legCY, cz-lz), 0.022f, legHY, 0.022f);
    pushBox(v, glm::vec3(cx+lx, legCY, cz+lz), 0.022f, legHY, 0.022f);
    // Back legs (near wall, -X side)
    pushBox(v, glm::vec3(cx-lx, legCY, cz-lz), 0.022f, legHY, 0.022f);
    pushBox(v, glm::vec3(cx-lx, legCY, cz+lz), 0.022f, legHY, 0.022f);

    // Horizontal stretchers
    float strY = FLOOR_Y + 0.19f;
    // Front / rear rails (X-axis pair, extend in Z)
    pushBox(v, glm::vec3(cx+lx, strY,        cz), 0.014f, 0.014f, lz-0.022f);
    pushBox(v, glm::vec3(cx-lx, strY,        cz), 0.014f, 0.014f, lz-0.022f);
    // Left / right rails (Z-axis pair, extend in X)
    pushBox(v, glm::vec3(cx, strY+0.04f, cz-lz),  lx-0.022f, 0.014f, 0.014f);
    pushBox(v, glm::vec3(cx, strY+0.04f, cz+lz),  lx-0.022f, 0.014f, 0.014f);

    return uploadVAO(v, cnt);
}

// =============================================================
//  Dog companion geometry  (dynamic mesh — rebuilt every frame)
//
//  Local space convention
//    Y = 0  →  paw ground contact
//    +Z     →  dog faces this direction (head end)
//    model matrix translates to gDogPos and rotates around Y
//
//  Proportions target a medium dog (Shiba Inu-ish):
//    Shoulder height ≈ 0.28 m,  total height ≈ 0.73 m
// =============================================================
static void buildDogBody(std::vector<float>& v, float walkPhase, bool moving)
{
    const float HALF_PI = 1.5707963f;
    const float PI      = 3.14159265f;
    const float legH    = 0.28f;          // hip height above ground
    v.clear();

    // ---- Walk-cycle derived values ----
    float bodyBob = moving ? sinf(walkPhase * 2.f) * 0.008f : 0.f;
    float bb      = legH + bodyBob;       // body parts offset from foot-level

    // Foot stride: Z-axis shift of the paw while stepping
    float strFL = moving ?  sinf(walkPhase)               * 0.038f : 0.f;
    float strFR = moving ?  sinf(walkPhase + PI)          * 0.038f : 0.f;
    float strBL = moving ?  sinf(walkPhase + PI + 0.35f)  * 0.030f : 0.f;
    float strBR = moving ?  sinf(walkPhase       + 0.35f) * 0.030f : 0.f;

    // Foot lift: paw rises off the ground during the stride forward
    float liftFL = moving ? fmaxf(0.f, sinf(walkPhase))               * 0.042f : 0.f;
    float liftFR = moving ? fmaxf(0.f, sinf(walkPhase + PI))          * 0.042f : 0.f;
    float liftBL = moving ? fmaxf(0.f, sinf(walkPhase + PI + 0.35f))  * 0.030f : 0.f;
    float liftBR = moving ? fmaxf(0.f, sinf(walkPhase       + 0.35f)) * 0.030f : 0.f;

    const float lx  = 0.076f;            // ±X position of legs
    const float fz  =  0.148f;           // front leg Z
    const float bz  = -0.148f;           // back  leg Z
    const float lR0 = 0.032f, lR1 = 0.028f;  // leg cone bottom/top radius
    const int   LSL = 8;                 // leg cone slices

    // ---- Legs (pushCone: base = paw, grows upward to hip) ----
    pushCone(v, glm::vec3(-lx, liftFL, fz + strFL), lR0, lR1, legH - liftFL, LSL);
    pushCone(v, glm::vec3(+lx, liftFR, fz + strFR), lR0, lR1, legH - liftFR, LSL);
    pushCone(v, glm::vec3(-lx, liftBL, bz + strBL), lR0, lR1, legH - liftBL, LSL);
    pushCone(v, glm::vec3(+lx, liftBR, bz + strBR), lR0, lR1, legH - liftBR, LSL);

    // ---- Paws (flattened boxes, slightly ahead of the leg centre) ----
    const float pw = 0.044f, ph = 0.026f, pd = 0.054f;
    pushBox(v, glm::vec3(-lx, liftFL + ph, fz + strFL - 0.016f), pw, ph, pd);
    pushBox(v, glm::vec3(+lx, liftFR + ph, fz + strFR - 0.016f), pw, ph, pd);
    pushBox(v, glm::vec3(-lx, liftBL + ph, bz + strBL - 0.012f), pw, ph, pd);
    pushBox(v, glm::vec3(+lx, liftBR + ph, bz + strBR - 0.012f), pw, ph, pd);

    // ---- Torso ----
    pushBox(v, glm::vec3(0.f,  bb + 0.115f,  0.f),     0.108f, 0.115f, 0.220f);
    pushBox(v, glm::vec3(0.f,  bb + 0.078f,  0.202f),  0.090f, 0.078f, 0.058f); // chest
    pushBox(v, glm::vec3(0.f,  bb + 0.130f, -0.200f),  0.090f, 0.090f, 0.060f); // rump

    // ---- Neck ----
    pushBox(v, glm::vec3(0.f,  bb + 0.258f,  0.194f),  0.064f, 0.072f, 0.064f);

    // ---- Head + cheek bulges ----
    pushBox(v, glm::vec3(0.f,  bb + 0.360f,  0.298f),  0.096f, 0.088f, 0.094f);
    pushBox(v, glm::vec3(-0.082f, bb + 0.347f, 0.286f), 0.024f, 0.055f, 0.068f);
    pushBox(v, glm::vec3(+0.082f, bb + 0.347f, 0.286f), 0.024f, 0.055f, 0.068f);

    // ---- Snout / lower jaw ----
    pushBox(v, glm::vec3(0.f,  bb + 0.302f,  0.395f),  0.054f, 0.044f, 0.068f);

    // ---- Floppy ears (hang to each side, slightly below crown) ----
    pushBox(v, glm::vec3(-0.105f, bb + 0.403f, 0.264f), 0.030f, 0.065f, 0.044f);
    pushBox(v, glm::vec3(+0.105f, bb + 0.403f, 0.264f), 0.030f, 0.065f, 0.044f);

    // ---- Tail  (3-segment Shiba curl with side-to-side wag) ----
    //  Segment X offsets increase up the tail to simulate the curl
    float wagS = moving ? sinf(walkPhase * 1.8f)
                        : sinf(walkPhase * 0.40f) * 0.6f;
    float wag  = wagS * 0.040f;
    pushCone(v, glm::vec3(0.f,         bb + 0.110f, -0.232f), 0.028f, 0.020f, 0.132f, 8);
    pushCone(v, glm::vec3(wag * 0.45f, bb + 0.242f, -0.215f), 0.020f, 0.013f, 0.112f, 8);
    pushCone(v, glm::vec3(wag * 0.90f, bb + 0.354f, -0.190f), 0.013f, 0.006f, 0.078f, 8);
}

// =============================================================
//  Dog body split into three NM-format regions (14-float vertex)
//  so each region gets its own texture:
//    Orange  – Carpet016 (fibrous/fur-like), warm Shiba tint
//    Cream   – Fabric025 (soft fabric),      cream tint (chest/cheeks/snout)
//    Paws    – Fabric035 (different weave),  cream tint (paws)
// =============================================================

// Shared walk-cycle helper (avoids duplicating computation across the 3 builders)
static void dogWalkCycle(float walkPhase, bool moving,
                          float& bb,
                          float& strFL, float& strFR, float& strBL, float& strBR,
                          float& liftFL,float& liftFR,float& liftBL,float& liftBR,
                          float& wagS)
{
    const float PI = 3.14159265f;
    const float legH = 0.28f;
    float bodyBob = moving ? sinf(walkPhase * 2.f) * 0.008f : 0.f;
    bb    = legH + bodyBob;
    strFL = moving ?  sinf(walkPhase)              * 0.038f : 0.f;
    strFR = moving ?  sinf(walkPhase + PI)         * 0.038f : 0.f;
    strBL = moving ?  sinf(walkPhase + PI + 0.35f) * 0.030f : 0.f;
    strBR = moving ?  sinf(walkPhase       + 0.35f)* 0.030f : 0.f;
    liftFL= moving ? fmaxf(0.f, sinf(walkPhase))              * 0.042f : 0.f;
    liftFR= moving ? fmaxf(0.f, sinf(walkPhase + PI))         * 0.042f : 0.f;
    liftBL= moving ? fmaxf(0.f, sinf(walkPhase + PI + 0.35f)) * 0.030f : 0.f;
    liftBR= moving ? fmaxf(0.f, sinf(walkPhase       + 0.35f))* 0.030f : 0.f;
    wagS  = moving ? sinf(walkPhase * 1.8f)
                   : sinf(walkPhase * 0.40f) * 0.6f;
}

// Orange/gold parts: legs, torso, rump, neck, head, ears, tail  (Carpet016)
static void buildDogBodyOrange(std::vector<float>& v, float walkPhase, bool moving)
{
    v.clear();
    float bb, strFL, strFR, strBL, strBR, liftFL, liftFR, liftBL, liftBR, wagS;
    dogWalkCycle(walkPhase, moving, bb,
                 strFL, strFR, strBL, strBR,
                 liftFL, liftFR, liftBL, liftBR, wagS);

    const float lx  = 0.076f, fz =  0.148f, bz = -0.148f;
    const float lR0 = 0.032f, lR1 = 0.028f;
    const int   LSL = 8;

    // Legs (conical, orange)
    pushConeNM(v, glm::vec3(-lx, liftFL, fz + strFL), lR0, lR1, 0.28f - liftFL, LSL);
    pushConeNM(v, glm::vec3(+lx, liftFR, fz + strFR), lR0, lR1, 0.28f - liftFR, LSL);
    pushConeNM(v, glm::vec3(-lx, liftBL, bz + strBL), lR0, lR1, 0.28f - liftBL, LSL);
    pushConeNM(v, glm::vec3(+lx, liftBR, bz + strBR), lR0, lR1, 0.28f - liftBR, LSL);

    // Body boxes (orange)
    pushBoxNM(v, glm::vec3(0.f,  bb+0.115f,  0.f),    0.108f, 0.115f, 0.220f); // torso
    pushBoxNM(v, glm::vec3(0.f,  bb+0.130f, -0.200f), 0.090f, 0.090f, 0.060f); // rump
    pushBoxNM(v, glm::vec3(0.f,  bb+0.258f,  0.194f), 0.064f, 0.072f, 0.064f); // neck
    pushBoxNM(v, glm::vec3(0.f,  bb+0.360f,  0.298f), 0.096f, 0.088f, 0.094f); // head
    pushBoxNM(v, glm::vec3(-0.105f, bb+0.403f, 0.264f), 0.030f, 0.065f, 0.044f); // L ear
    pushBoxNM(v, glm::vec3(+0.105f, bb+0.403f, 0.264f), 0.030f, 0.065f, 0.044f); // R ear

    // Tail (3-segment curl, orange)
    float wag = wagS * 0.040f;
    pushConeNM(v, glm::vec3(0.f,         bb+0.110f, -0.232f), 0.028f, 0.020f, 0.132f, 8);
    pushConeNM(v, glm::vec3(wag*0.45f,   bb+0.242f, -0.215f), 0.020f, 0.013f, 0.112f, 8);
    pushConeNM(v, glm::vec3(wag*0.90f,   bb+0.354f, -0.190f), 0.013f, 0.006f, 0.078f, 8);
}

// Cream/white parts: chest, cheeks, snout  (Fabric025)
static void buildDogBodyCream(std::vector<float>& v, float walkPhase, bool moving)
{
    v.clear();
    float bb, strFL, strFR, strBL, strBR, liftFL, liftFR, liftBL, liftBR, wagS;
    dogWalkCycle(walkPhase, moving, bb,
                 strFL, strFR, strBL, strBR,
                 liftFL, liftFR, liftBL, liftBR, wagS);

    pushBoxNM(v, glm::vec3(0.f,  bb+0.078f,  0.202f),  0.090f, 0.078f, 0.058f); // chest
    pushBoxNM(v, glm::vec3(-0.082f, bb+0.347f, 0.286f), 0.024f, 0.055f, 0.068f); // L cheek
    pushBoxNM(v, glm::vec3(+0.082f, bb+0.347f, 0.286f), 0.024f, 0.055f, 0.068f); // R cheek
    pushBoxNM(v, glm::vec3(0.f,  bb+0.302f,  0.395f),  0.054f, 0.044f, 0.068f); // snout
}

// Paw parts: four paws  (Fabric035 — slightly rougher weave for paw pads)
static void buildDogBodyPaws(std::vector<float>& v, float walkPhase, bool moving)
{
    v.clear();
    float bb, strFL, strFR, strBL, strBR, liftFL, liftFR, liftBL, liftBR, wagS;
    dogWalkCycle(walkPhase, moving, bb,
                 strFL, strFR, strBL, strBR,
                 liftFL, liftFR, liftBL, liftBR, wagS);

    const float lx = 0.076f, fz = 0.148f, bz = -0.148f;
    const float pw = 0.044f, ph = 0.026f, pd = 0.054f;
    pushBoxNM(v, glm::vec3(-lx, liftFL+ph, fz+strFL-0.016f), pw, ph, pd);
    pushBoxNM(v, glm::vec3(+lx, liftFR+ph, fz+strFR-0.016f), pw, ph, pd);
    pushBoxNM(v, glm::vec3(-lx, liftBL+ph, bz+strBL-0.012f), pw, ph, pd);
    pushBoxNM(v, glm::vec3(+lx, liftBR+ph, bz+strBR-0.012f), pw, ph, pd);
}

// Eyes and nose — drawn separately with a very dark colour tint
static void buildDogDetails(std::vector<float>& v, float walkPhase, bool moving)
{
    v.clear();
    float bodyBob = moving ? sinf(walkPhase * 2.f) * 0.008f : 0.f;
    float bb      = 0.28f + bodyBob;

    // Eyes (upper-front face of head)
    pushBox(v, glm::vec3(-0.061f, bb + 0.382f, 0.387f), 0.019f, 0.018f, 0.010f);
    pushBox(v, glm::vec3(+0.061f, bb + 0.382f, 0.387f), 0.019f, 0.018f, 0.010f);

    // Nose (front tip of snout)
    pushBox(v, glm::vec3(0.f, bb + 0.323f, 0.463f), 0.032f, 0.022f, 0.013f);
}

// =============================================================
//  Door handle  – L-shaped lever (two cylinders, PBR rendered)
//  Horizontal arm: juts out from door toward camera
//  Vertical arm  : curves down at the end (classic lever shape)
// Helper: push one cylinder arm into a vertex buffer
static void pushCylArm(std::vector<float>& v,
                       glm::vec3 pA, glm::vec3 pB,
                       glm::vec3 axDir, glm::vec3 ayDir,
                       glm::vec3 capNA, glm::vec3 capNB,
                       float r, int SLICES)
{
    const float PI2 = 6.28318530f;
    for (int i = 0; i < SLICES; i++) {
        float a0 = i       * PI2 / SLICES;
        float a1 = (i + 1) * PI2 / SLICES;
        glm::vec3 n0 = cosf(a0)*axDir + sinf(a0)*ayDir;
        glm::vec3 n1 = cosf(a1)*axDir + sinf(a1)*ayDir;
        glm::vec3 p0A = pA+n0*r, p1A = pA+n1*r;
        glm::vec3 p0B = pB+n0*r, p1B = pB+n1*r;
        float u0 = (float)i/SLICES, u1 = (float)(i+1)/SLICES;

        // Side quads
        pushV(v, p0A, n0, glm::vec2(u0,0.f));
        pushV(v, p1A, n1, glm::vec2(u1,0.f));
        pushV(v, p1B, n1, glm::vec2(u1,1.f));
        pushV(v, p0A, n0, glm::vec2(u0,0.f));
        pushV(v, p1B, n1, glm::vec2(u1,1.f));
        pushV(v, p0B, n0, glm::vec2(u0,1.f));

        // End caps
        pushV(v, pA,  capNA, glm::vec2(.5f,.5f));
        pushV(v, p0A, capNA, glm::vec2(.5f+n0.x*.5f,.5f+n0.y*.5f));
        pushV(v, p1A, capNA, glm::vec2(.5f+n1.x*.5f,.5f+n1.y*.5f));

        pushV(v, pB,  capNB, glm::vec2(.5f,.5f));
        pushV(v, p1B, capNB, glm::vec2(.5f+n1.x*.5f,.5f+n1.y*.5f));
        pushV(v, p0B, capNB, glm::vec2(.5f+n0.x*.5f,.5f+n0.y*.5f));
    }
}

// =============================================================
//  Door handle  – L-shaped lever (two cylinder arms)
// =============================================================
static unsigned int buildHandleVAO(int& cnt)
{
    const int   SLICES = 20;
    const float r      = 0.013f;
    std::vector<float> v;

    // Horizontal arm: from door surface (Z=0.945) outward toward camera (Z=0.830)
    glm::vec3 hA(0.60f, 0.22f, 0.945f);
    glm::vec3 hB(0.60f, 0.22f, 0.830f);
    pushCylArm(v, hA, hB,
               glm::vec3(1,0,0), glm::vec3(0,1,0),
               glm::vec3(0,0,1), glm::vec3(0,0,-1), r, SLICES);

    // Vertical arm: goes downward from end of horizontal arm
    glm::vec3 vA = hB;
    glm::vec3 vB(0.60f, 0.085f, 0.830f);
    pushCylArm(v, vA, vB,
               glm::vec3(1,0,0), glm::vec3(0,0,1),
               glm::vec3(0,1,0), glm::vec3(0,-1,0), r, SLICES);

    return uploadVAO(v, cnt);
}

// Three colored LED accent strips on the left wall
static void buildColorAccents(unsigned int vao[3], int cnt[3])
{
    const float wx = -1.985f, sy = 0.80f, sh = 0.05f;
    float z0[3] = { -11.5f, -7.5f, -3.5f };
    float z1[3] = {  -9.0f, -5.0f, -1.0f };
    for (int i = 0; i < 3; i++) {
        std::vector<float> v;
        addQuad(v, glm::vec3(wx,sy,z0[i]),    glm::vec3(wx,sy+sh,z0[i]),
                   glm::vec3(wx,sy+sh,z1[i]), glm::vec3(wx,sy,z1[i]),
                   glm::vec3(1,0,0),
                   glm::vec2(0,0), glm::vec2(0,1), glm::vec2(1,1), glm::vec2(1,0));
        vao[i] = uploadVAO(v, cnt[i]);
    }
}

// =============================================================
//  Picture frames  (all borders in one VAO, paintings split by type)
// =============================================================
struct FrameDef { float wallX, normalDir, centerZ, centerY, halfW, halfH; int paintType; };

static const FrameDef FRAMES[] = {
    { -2.f, +1.f, -11.0f, 0.55f, 0.50f, 0.38f, 0 },
    { -2.f, +1.f,  -8.0f, 0.55f, 0.45f, 0.35f, 1 },
    { -2.f, +1.f,  -5.0f, 0.55f, 0.50f, 0.40f, 2 },
    { -2.f, +1.f,  -2.0f, 0.55f, 0.42f, 0.35f, 0 },
    {  2.f, -1.f, -11.5f, 0.55f, 0.45f, 0.38f, 2 },
    {  2.f, -1.f,  -8.5f, 0.55f, 0.50f, 0.35f, 0 },
    {  2.f, -1.f,  -5.5f, 0.55f, 0.42f, 0.40f, 1 },
    {  2.f, -1.f,  -2.5f, 0.55f, 0.50f, 0.38f, 2 },
};
const int NUM_FRAMES = 8;

// Builds border + one painting-type VAO
static void buildFrameMeshes(
    unsigned int& borderVAO, int& borderCnt,
    unsigned int paintVAO[3], int paintCnt[3])
{
    const float BORDER = 0.055f;  // frame border width

    std::vector<float> bv;           // all borders
    std::vector<float> pv[3];        // paintings per type

    for (int fi = 0; fi < NUM_FRAMES; fi++) {
        const FrameDef& f = FRAMES[fi];
        float nd  = f.normalDir;
        float x   = f.wallX + nd * 0.012f;   // border face
        float xp  = f.wallX + nd * 0.018f;   // painting face (slightly offset)
        glm::vec3 n(nd, 0, 0);

        float hw = f.halfW, hh = f.halfH;
        float cz = f.centerZ, cy = f.centerY;

        // -- Border quad (full rectangle) --
        // We render the full rectangle then the painting on top to get border effect
        addQuad(bv,
            glm::vec3(x, cy-hh, cz-hw), glm::vec3(x, cy+hh, cz-hw),
            glm::vec3(x, cy+hh, cz+hw), glm::vec3(x, cy-hh, cz+hw),
            n, glm::vec2(0,0), glm::vec2(0,1), glm::vec2(1,1), glm::vec2(1,0));

        // -- Painting quad (inset by BORDER) --
        addQuad(pv[f.paintType],
            glm::vec3(xp, cy-hh+BORDER, cz-hw+BORDER),
            glm::vec3(xp, cy+hh-BORDER, cz-hw+BORDER),
            glm::vec3(xp, cy+hh-BORDER, cz+hw-BORDER),
            glm::vec3(xp, cy-hh+BORDER, cz+hw-BORDER),
            n, glm::vec2(0,0), glm::vec2(0,1), glm::vec2(1,1), glm::vec2(1,0));
    }

    borderVAO = uploadVAO(bv, borderCnt);
    for (int i = 0; i < 3; i++)
        paintVAO[i] = uploadVAO(pv[i], paintCnt[i]);
}



// =============================================================
//  Shadow FBO
// =============================================================
static unsigned int createShadowFBO(unsigned int& shadowTex)
{
    unsigned int FBO;
    glGenFramebuffers(1, &FBO);
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,SHADOW_W,SHADOW_H,0,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float border[]={1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    glBindFramebuffer(GL_FRAMEBUFFER,FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTex,0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return FBO;
}

// =============================================================
//  Full-screen fade quad
// =============================================================
static unsigned int buildFadeVAO()
{
    float v[]={-1,1,-1,-1,1,-1,-1,1,1,-1,1,1};
    unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glBindVertexArray(0);
    return VAO;
}

// =============================================================
//  GLFW callbacks
// =============================================================
void framebuffer_size_callback(GLFWwindow*, int w, int h) { glViewport(0,0,w,h); }

// =============================================================
//  Particle helpers
// =============================================================
static void resetParticle(Particle& p)
{
    // Random position spread through the corridor
    p.pos = glm::vec3(
        ((float)rand() / RAND_MAX) * 3.4f - 1.7f,
        ((float)rand() / RAND_MAX) * 3.0f - 0.8f,
        ((float)rand() / RAND_MAX) * 12.5f - 12.5f
    );
    // Very slow upward drift + slight sideways wobble
    p.vel = glm::vec3(
        ((float)rand() / RAND_MAX - 0.5f) * 0.02f,
        ((float)rand() / RAND_MAX) * 0.025f + 0.004f,
        ((float)rand() / RAND_MAX - 0.5f) * 0.008f
    );
    p.maxAge    = 7.0f + ((float)rand() / RAND_MAX) * 10.0f;
    p.age       = 0.0f;
    p.size      = 0.006f + ((float)rand() / RAND_MAX) * 0.013f;
    p.baseAlpha = 0.25f  + ((float)rand() / RAND_MAX) * 0.50f;
}

static void initParticles()
{
    srand(42);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        resetParticle(gParticles[i]);
        // Stagger initial ages so they don't all spawn at once
        gParticles[i].age = ((float)rand() / RAND_MAX) * gParticles[i].maxAge;
    }
}

static void updateParticles(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        gParticles[i].age += dt;
        if (gParticles[i].age >= gParticles[i].maxAge) {
            resetParticle(gParticles[i]);
        } else {
            gParticles[i].pos += gParticles[i].vel * dt;
            // Tiny random horizontal drift each frame
            gParticles[i].vel.x += ((float)rand() / RAND_MAX - 0.5f) * 0.0004f;
            if (gParticles[i].vel.x >  0.025f) gParticles[i].vel.x =  0.025f;
            if (gParticles[i].vel.x < -0.025f) gParticles[i].vel.x = -0.025f;
        }
    }
}

void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
    float fx = (float)xpos, fy = (float)ypos;
    if (firstMouse) { lastMouseX = fx; lastMouseY = fy; firstMouse = false; }
    float dx = (fx - lastMouseX) * 0.10f;
    float dy = (lastMouseY - fy) * 0.10f;
    lastMouseX = fx; lastMouseY = fy;
    yaw   += dx;
    pitch  = glm::clamp(pitch + dy, -89.0f, 89.0f);
}


// =============================================================
//  main
// =============================================================
void Run(GLFWwindow* window)
{
    // re-register callbacks for this scene
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowTitle(window, "Corridor Scene  [WASD: move  ESC: quit]");

    // reset per-scene state
    gScene     = PLAYING;
    fadeAlpha  = 0.0f;
    camX       = 0.0f;
    camZ       = -12.0f;
    yaw        = 90.0f;
    pitch      = 0.0f;
    firstMouse = true;
    lastFrame  = (float)glfwGetTime();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---- Shaders ----
    Shader corridorShader ("shader/corridor.vs",   "shader/corridor.fs");
    std::cout << "[CHK] B: corridorShader\n" << std::flush;
    Shader depthShader    ("shader/depth.vs",      "shader/depth.fs");
    Shader emissiveShader ("shader/emissive.vs",   "shader/emissive.fs");
    Shader fadeShader     ("shader/fade.vs",        "shader/fade.fs");
    Shader particleShader ("shader/particle.vs",   "shader/particle.fs");
    Shader pbrShader      ("shader/pbr.vs",         "shader/pbr.fs");
    Shader wallNMShader   ("shader/wall_nm.vs",    "shader/wall_nm.fs");
    Shader brightShader   ("shader/bloom_quad.vs", "shader/bloom_bright.fs");
    Shader blurShader     ("shader/bloom_quad.vs", "shader/bloom_blur.fs");
    Shader bloomFinalShader("shader/bloom_quad.vs","shader/bloom_final.fs");
    // ---- Wine / explosion shaders ----
    Shader wineShader    ("shader/bloom_quad.vs",  "shader/wine.fs");
    std::cout << "[CHK] C: wineShader\n" << std::flush;
    Shader explodeShader ("shader/explode.vs",     "shader/explode.fs", "shader/explode.gs");
    std::cout << "[CHK] D: explodeShader\n" << std::flush;

    // ---- Textures ----
    // Floor: Terrazzo009 color + normal map
    unsigned int texFloor   = loadTextureFile("textures/Terrazzo009_2K-JPG_Color.jpg");
    // Walls: Marble014 color map
    unsigned int texWall    = loadTextureFile("textures/Marble014_2K-JPG_Color.jpg");
    // Wall normal map: Marble014 OpenGL-convention tangent-space normals
    unsigned int texWallNM  = loadTextureFile("textures/Marble014_2K-JPG_NormalGL.jpg");
    // Floor normal map: Terrazzo009 OpenGL-convention tangent-space normals
    unsigned int texFloorNM = loadTextureFile("textures/Terrazzo009_2K-JPG_NormalGL.jpg");
    unsigned int texCeil    = genCeilingTex();
    unsigned int texDoor    = genDoorTex();


    // ---- Geometry ----
    CorridorMesh corridor = buildCorridorMesh();
    WallNMMesh   wallNM   = buildWallNMMesh();      // wall mesh with T/B attribs
    FloorNMMesh  floorNM  = buildFloorNMMesh();     // floor mesh with T/B attribs

    // Pre-bind NM shader texture units (constant for the whole run)
    wallNMShader.use();
    wallNMShader.setInt("diffuseTex",  0);
    wallNMShader.setInt("normalMap",   1);
    wallNMShader.setInt("shadowMap0",  2);
    wallNMShader.setInt("shadowMap1",  3);
    wallNMShader.setInt("shadowMap2",  4);
    int doorCnt, baseCnt;
    unsigned int doorVAO     = buildDoorVAO(doorCnt);
    unsigned int baseVAO     = buildBaseVAO(baseCnt);
    unsigned int fadeVAO     = buildFadeVAO();
    int handleCnt;
    unsigned int handleVAO   = buildHandleVAO(handleCnt);


    // ---- 3D Props: OBJ plant + chair ----
    unsigned int texWood       = genWoodTex();
    unsigned int texGrassColor      = loadTextureFile("textures/Grass005_2K-JPG_Color.jpg");
    unsigned int texGrassNM         = loadTextureFile("textures/Grass005_2K-JPG_NormalGL.jpg");
    // 화분(pot) 텍스처: Terrazzo004
    unsigned int texPotColor        = loadTextureFile("textures/Terrazzo004_2K-JPG_Color.jpg");
    unsigned int texPotNM           = loadTextureFile("textures/Terrazzo004_2K-JPG_NormalGL.jpg");
    // 강아지집 텍스처: Fabric080
    unsigned int texDogHouseColor   = loadTextureFile("textures/Fabric080_2K-JPG_Color.jpg");
    unsigned int texDogHouseNM      = loadTextureFile("textures/Fabric080_2K-JPG_NormalGL.jpg");

    std::cout << "[CHK] E: Textures done\n" << std::flush;
    // OBJ 식물 모델 로드 (assimp)
    Model plant("resources/objects/plant/Low-Poly Plant_.obj");
    std::cout << "[CHK] F: plant loaded (" << plant.meshes.size() << " meshes)\n" << std::flush;
    { FILE* _dbf=nullptr; fopen_s(&_dbf,"diag.txt","a"); if(_dbf){fputs("[DIAG:AFTER_F]\n",_dbf);fflush(_dbf);fclose(_dbf);} }
    // OBJ 강아지 모델 로드 (assimp)
    // 원점 오프셋: Y_min≈-11(발바닥→0), Z_center≈20(모델 중앙)
    // 스케일: 0.038 (약 0.69m 높이)
    Model dogOBJ("resources/objects/dog/10680_Dog_v2.obj");
    std::cout << "[CHK] G: dog loaded (" << dogOBJ.meshes.size() << " meshes)\n" << std::flush;
    { FILE* _dbf=nullptr; fopen_s(&_dbf,"diag.txt","a"); if(_dbf){fputs("[DIAG:AFTER_G]\n",_dbf);fflush(_dbf);fclose(_dbf);} }
    std::cout << "[CHK] G0a: before PLANT_SCALE\n" << std::flush;
    // 식물 배치 행렬: 좌측 벽 두 위치에 스케일 조정 후 배치
    const float PLANT_SCALE = 0.85f;
    std::cout << "[CHK] G0b: before plantMat1\n" << std::flush;
    const glm::mat4 plantMat1 = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(-1.50f, -1.0f, -9.5f)),
        glm::vec3(PLANT_SCALE));
    std::cout << "[CHK] G0c: before plantMat2\n" << std::flush;
    const glm::mat4 plantMat2 = glm::scale(
        glm::translate(glm::mat4(1.0f), glm::vec3(-1.50f, -1.0f, -4.0f)),
        glm::vec3(PLANT_SCALE));
    std::cout << "[CHK] G1: plant matrices\n" << std::flush;

    // Two chairs against the left wall (x=-2), facing into the corridor (+X)
    // cx=-1.78: back edge of backrest sits ~1cm from wall
    // cz spaced 0.60m apart so chairs sit comfortably side by side
    int chairCnt1, chairCnt2;
    unsigned int chairVAO1 = buildChairVAO(-1.78f, -7.1f, chairCnt1);
    unsigned int chairVAO2 = buildChairVAO(-1.78f, -7.7f, chairCnt2);
    std::cout << "[CHK] G2: chairVAOs\n" << std::flush;

    // ---- Dog house (back-left corner, brick texture reused from wall) ----
    // Position: cx=-1.55 keeps house clear of baseboard; cz=-11.8 is deep in corridor
    int dogHouseCnt, dogHouseDoorCnt;
    unsigned int dogHouseVAO     = buildDogHouseVAO    (-1.55f, -11.8f, dogHouseCnt);
    unsigned int dogHouseDoorVAO = buildDogHouseDoorVAO(-1.55f, -11.8f, dogHouseDoorCnt);
    std::cout << "[CHK] G3: dogHouseVAOs\n" << std::flush;

    // ---- Glass vase (right side, z = -8.8) ----
    int vaseCnt;
    unsigned int vaseVAO = buildVaseVAO(1.40f, -1.0f, -8.8f, vaseCnt);
    std::cout << "[CHK] G4: vaseVAO\n" << std::flush;

    // ---- Picture frame borders + paintings (procedural textures) ----
    unsigned int borderVAO; int borderCnt;
    unsigned int paintVAO[3]; int paintCnt[3];
    buildFrameMeshes(borderVAO, borderCnt, paintVAO, paintCnt);
    unsigned int texPaint[3] = { genPainting0(), genPainting1(), genPainting2() };
    std::cout << "[CHK] G4b: frames built\n" << std::flush;

    // ---- Dog companion: textures ----
    // 강아지 털 color: 절차적 생성 (Shiba Inu warm orange-gold)
    // 강아지 털 NM  : Carpet016 결 재활용 (wavy fiber → fur느낌)
    unsigned int texDogFurColor = genDogFurTex();
    unsigned int texDogFurNM    = loadTextureFile("textures/Carpet016_NormalGL.jpg");
    std::cout << "[CHK] G5: dog textures\n" << std::flush;

    // Helper lambda: allocate a dynamic VAO in the 8-float (pos+nrm+uv) format
    // used by the shadow depth pass and the detail (eyes/nose) pass.
    auto make8VAO = [](unsigned int& VAO, unsigned int& VBO, int maxV) {
        glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, maxV * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
        glBindVertexArray(0);
    };
    // Helper lambda: allocate a dynamic VAO in the 14-float NM format
    // (pos+nrm+uv+tangent+bitangent) used by wallNMShader.
    auto make14VAO = [](unsigned int& VAO, unsigned int& VBO, int maxV) {
        const int S = 14 * (int)sizeof(float);
        glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, maxV * 14 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,S,(void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,S,(void*)(3*sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,S,(void*)(6*sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,S,(void*)(8*sizeof(float)));
        glEnableVertexAttribArray(4); glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,S,(void*)(11*sizeof(float)));
        glBindVertexArray(0);
    };

    // 8-float VAOs ─────────────────────────────────────────────────────────────
    // dogBodyVAO: full body in 8-float format → used ONLY in shadow depth pass
    // (4 legs×96 + 4 paws×36 + 11 boxes×36 + 3 tail×96 = 1572 < 2048)
    const int DOG_BODY_MAXV   = 2048;
    const int DOG_DETAIL_MAXV = 256;
    unsigned int dogBodyVAO, dogBodyVBO;
    make8VAO(dogBodyVAO, dogBodyVBO, DOG_BODY_MAXV);

    // dogDetailVAO: eyes + nose — dark tint, 8-float format
    unsigned int dogDetailVAO, dogDetailVBO;
    make8VAO(dogDetailVAO, dogDetailVBO, DOG_DETAIL_MAXV);
    std::cout << "[CHK] G6: dog body VAOs\n" << std::flush;

    // 14-float NM VAOs ─────────────────────────────────────────────────────────
    // Orange: 4 legs(96) + 3 tail(96) + 6 boxes(36) = 384+288+216 = 888 < 1024
    const int DOG_ORANGE_MAXV = 1024;
    // Cream:  3 boxes (chest+cheek×2+snout) = 4×36 = 144 < 256
    const int DOG_CREAM_MAXV  = 256;
    // Paws:   4 boxes = 4×36 = 144 < 256
    const int DOG_PAW_MAXV    = 256;
    unsigned int dogOrangeVAO, dogOrangeVBO;
    make14VAO(dogOrangeVAO, dogOrangeVBO, DOG_ORANGE_MAXV);
    unsigned int dogCreamVAO, dogCreamVBO;
    make14VAO(dogCreamVAO, dogCreamVBO, DOG_CREAM_MAXV);
    unsigned int dogPawVAO, dogPawVBO;
    make14VAO(dogPawVAO, dogPawVBO, DOG_PAW_MAXV);
    std::cout << "[CHK] G7: dog NM VAOs\n" << std::flush;

    // ---- Particle VAO / VBO (dynamic, updated every frame) ----
    // 6 floats per vertex: pos(3) + uv(2) + alpha(1)
    // 6 vertices per particle (2 triangles)
    unsigned int particleVAO, particleVBO;
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 6 * 6 * sizeof(float),
                 nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(5*sizeof(float)));
    glBindVertexArray(0);
    std::cout << "[CHK] G8: particle VAO\n" << std::flush;

    initParticles();
    std::cout << "[CHK] G9: initParticles\n" << std::flush;

    // ---- HDR framebuffer (scene renders here) ----
    unsigned int hdrFBO, hdrColorTex, hdrDepthRBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    glGenTextures(1, &hdrColorTex);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, hdrColorTex, 0);

    glGenRenderbuffers(1, &hdrDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                          SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, hdrDepthRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "[CHK] G10: HDR FBO\n" << std::flush;

    // ---- Bright-extract FBO ----
    unsigned int brightFBO, brightTex;
    glGenFramebuffers(1, &brightFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, brightFBO);
    glGenTextures(1, &brightTex);
    glBindTexture(GL_TEXTURE_2D, brightTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, brightTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "[CHK] G11: bright FBO\n" << std::flush;

    // ---- Ping-pong FBOs for Gaussian blur ----
    unsigned int pingpongFBO[2], pingpongTex[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongTex);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
                     SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, pingpongTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "[CHK] G12: pingpong FBOs\n" << std::flush;

    // ---- Wine post-process FBO ──────────────────────────────────────────────
    // bloom-final output is redirected here when wine is active;
    // wine.fs then reads it and applies drunk-screen effects to the screen.
    unsigned int wineFBO, wineColorTex;
    glGenFramebuffers(1, &wineFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, wineFBO);
    glGenTextures(1, &wineColorTex);
    glBindTexture(GL_TEXTURE_2D, wineColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, wineColorTex, 0);
    // No depth RBO needed — full-screen quad pass only
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "[CHK] G13: wine FBO\n" << std::flush;

    // ---- Screen quad VAO (pos + uv) for post-process passes ----
    float screenQuadVerts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int screenQuadVAO, screenQuadVBO;
    glGenVertexArrays(1, &screenQuadVAO);
    glGenBuffers(1, &screenQuadVBO);
    glBindVertexArray(screenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuadVerts),
                 screenQuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);
    std::cout << "[CHK] G14: screen quad VAO\n" << std::flush;

    // ---- Shadow maps: one per ceiling lamp ----
  
    glm::vec3 doorLightPos(0.f,  0.8f,  2.5f);
    glm::vec3 doorLightCol(0.5f, 0.65f, 1.0f);

    // Set constant shader uniforms (texture unit bindings never change)
    corridorShader.use();
    corridorShader.setInt("diffuseTex",  0);

    glm::mat4 identity(1.f);

    // Wine shader: texture unit binding
    wineShader.use();
    wineShader.setInt("screenTex", 0);

    std::cout << "[CHK] H: Init done, entering render loop\n" << std::flush;
    // ---- Render loop ----
    while (!glfwWindowShouldClose(window) && gScene != DONE)
    {
        float now = (float)glfwGetTime();
        deltaTime = now - lastFrame;
        lastFrame = now;

        // ---- ESC ----
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // ---- F : toggle wine / drunk post-process effect ----
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!wineKeyHeld) { wineActive = !wineActive; wineKeyHeld = true; }
        } else { wineKeyHeld = false; }

        // ---- G : trigger vase explosion ----
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
            if (!vaseKeyHeld && !vaseGone) { vaseExploding = true; vaseKeyHeld = true; }
        } else { vaseKeyHeld = false; }
        if (vaseExploding) {
            vaseExplodeTime += deltaTime;
            if (vaseExplodeTime >= 2.5f) vaseGone = true;
        }

        // ---- Camera direction from mouse yaw/pitch ----
        glm::vec3 front;
        front.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        front.y = sinf(glm::radians(pitch));
        front.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        front = glm::normalize(front);
        glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
        glm::vec3 flatRight = glm::normalize(glm::cross(flatFront, glm::vec3(0,1,0)));

        // ---- WASD movement (relative to look direction) ----
        isMoving = false;
        if (gScene == PLAYING) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                camX += flatFront.x * WALK_SPEED * deltaTime;
                camZ += flatFront.z * WALK_SPEED * deltaTime;
                isMoving = true;
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                camX -= flatFront.x * WALK_SPEED * deltaTime;
                camZ -= flatFront.z * WALK_SPEED * deltaTime;
                isMoving = true;
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                camX -= flatRight.x * WALK_SPEED * deltaTime;
                camZ -= flatRight.z * WALK_SPEED * deltaTime;
                isMoving = true;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                camX += flatRight.x * WALK_SPEED * deltaTime;
                camZ += flatRight.z * WALK_SPEED * deltaTime;
                isMoving = true;
            }
            camZ = glm::clamp(camZ, BOUND_Z_MIN, BOUND_Z_MAX);
            camX = glm::clamp(camX, -BOUND_X, BOUND_X);

            if (camZ >= DOOR_TRIGGER_Z) gScene = FADING_OUT;
        } else if (gScene == FADING_OUT) {
            fadeAlpha += FADE_SPEED * deltaTime;
            if (fadeAlpha >= 1.0f) { fadeAlpha = 1.0f; gScene = DONE; }
        }

        // ---- Camera bobbing (only while moving) ----
        if (isMoving) walkTimer += deltaTime;
        float bobY = sinf(walkTimer * BOB_FREQ * 2.0f * 3.14159265f) * BOB_AMP;
        glm::vec3 camPos(camX, BASE_Y + bobY, camZ);

        glm::mat4 proj = glm::perspective(glm::radians(75.f), (float)SCR_WIDTH/SCR_HEIGHT, 0.05f, 50.f);
        glm::mat4 view = glm::lookAt(camPos, camPos + front, glm::vec3(0,1,0));

        // ---- Update particles and build billboard VBO ----
        updateParticles(deltaTime);

        // Camera right/up in world space  (inverse of view = transpose for orthonormal)
        glm::vec3 pRight(view[0][0], view[1][0], view[2][0]);
        glm::vec3 pUp   (view[0][1], view[1][1], view[2][1]);

        static std::vector<float> pv;
        pv.clear();
        pv.reserve(MAX_PARTICLES * 36);

        for (int pi = 0; pi < MAX_PARTICLES; pi++) {
            Particle& p = gParticles[pi];
            float ratio    = p.age / p.maxAge;
            float lifeFade = (ratio < 0.2f) ? (ratio * 5.0f)
                           : (ratio > 0.8f) ? ((1.0f - ratio) * 5.0f) : 1.0f;
            float a = p.baseAlpha * lifeFade;
            float s = p.size;

            glm::vec3 bl = p.pos - pRight*s - pUp*s;
            glm::vec3 br = p.pos + pRight*s - pUp*s;
            glm::vec3 tr = p.pos + pRight*s + pUp*s;
            glm::vec3 tl = p.pos - pRight*s + pUp*s;

            // Triangle 1: bl, br, tr
            pv.push_back(bl.x); pv.push_back(bl.y); pv.push_back(bl.z);
            pv.push_back(0.0f); pv.push_back(0.0f); pv.push_back(a);

            pv.push_back(br.x); pv.push_back(br.y); pv.push_back(br.z);
            pv.push_back(1.0f); pv.push_back(0.0f); pv.push_back(a);

            pv.push_back(tr.x); pv.push_back(tr.y); pv.push_back(tr.z);
            pv.push_back(1.0f); pv.push_back(1.0f); pv.push_back(a);

            // Triangle 2: bl, tr, tl
            pv.push_back(bl.x); pv.push_back(bl.y); pv.push_back(bl.z);
            pv.push_back(0.0f); pv.push_back(0.0f); pv.push_back(a);

            pv.push_back(tr.x); pv.push_back(tr.y); pv.push_back(tr.z);
            pv.push_back(1.0f); pv.push_back(1.0f); pv.push_back(a);

            pv.push_back(tl.x); pv.push_back(tl.y); pv.push_back(tl.z);
            pv.push_back(0.0f); pv.push_back(1.0f); pv.push_back(a);
        }

        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizeiptr)(pv.size() * sizeof(float)), pv.data());

        // ---- Dog companion: follow AI ----
        {
            // Follow distance: 5.5 m behind the player
            const float FOLLOW_DIST = 5.5f;
            glm::vec3 tgt(
                camPos.x - flatFront.x * FOLLOW_DIST,
                -1.f,
                camPos.z - flatFront.z * FOLLOW_DIST);
            tgt.x = glm::clamp(tgt.x, -BOUND_X + 0.22f,    BOUND_X - 0.22f);
            tgt.z = glm::clamp(tgt.z,  BOUND_Z_MIN + 0.22f,  BOUND_Z_MAX - 0.22f);

            glm::vec3 prevPos = gDogPos;

            // Dog position updates ONLY while the player is walking.
            // The moment the player stops, the dog freezes in place.
            if (isMoving) {
                // Very small alpha → dog creeps forward a tiny bit each frame
                float posAlpha = glm::min(0.25f * deltaTime, 1.0f);
                gDogPos.x = glm::mix(gDogPos.x, tgt.x, posAlpha);
                gDogPos.z = glm::mix(gDogPos.z, tgt.z, posAlpha);
            }
            gDogPos.y = -1.0f;

            float moved = glm::length(
                glm::vec2(gDogPos.x - prevPos.x, gDogPos.z - prevPos.z));

            // Dog is "moving" only if the player is walking AND the dog actually moved
            gDogMoving = isMoving && (moved > 0.00005f);

            // Dog always faces the player (so turning around shows the dog's face)
            glm::vec3 toPlayer(camPos.x - gDogPos.x, 0.f, camPos.z - gDogPos.z);
            float distToPlayer = glm::length(toPlayer);
            if (distToPlayer > 0.1f) {
                glm::vec3 desired  = toPlayer / distToPlayer;
                float     dirAlpha = glm::min(2.5f * deltaTime, 1.0f);
                gDogDir = glm::normalize(glm::mix(gDogDir, desired, dirAlpha));
            }

            // Walk cycle advances with actual distance moved; idle = slow tail wag
            if (gDogMoving) {
                gDogWalk += moved * 20.0f;
            } else {
                gDogWalk += 0.5f * deltaTime;
            }
            if (gDogWalk > 6.28318f) gDogWalk -= 6.28318f;
        }

        // Dog model matrix
        // OBJ 분석: Y_min=-16.61=머리, Y_max=18.50=발 → OBJ가 뒤집혀 저장됨
        // 수정: X축 180° 회전 → 발Y=0, 머리 위로.  코가 로컬+Z → +π 불필요
        // 보행 애니: 상하 바운싱 + 롤(좌우 무게이동) → 발걸음 느낌
        // 꼬리: 정지 시 gDogWalk*25 고속 사인 → 꼬리흔들기 느낌
        float walkBob  = sinf(gDogWalk) * (gDogMoving ? 0.018f : 0.0f);
        float walkRoll = gDogMoving
            ? sinf(gDogWalk * 2.0f) * 0.06f        // 보행 좌우 sway
            : sinf(gDogWalk * 25.0f) * 0.035f;     // 정지 꼬리흔들기 feel
        float gDogYawRad = atan2f(gDogDir.x, gDogDir.z);   // +π 제거: 코가 +Z
        const float DOG_SC = 0.020f;
        glm::vec3 dogWorldPos(gDogPos.x, gDogPos.y + walkBob, gDogPos.z);
        glm::mat4 dogMat = glm::translate(glm::mat4(1.f), dogWorldPos);
        dogMat = dogMat * glm::rotate(glm::mat4(1.f), gDogYawRad, glm::vec3(0.f, 1.f, 0.f));
        dogMat = dogMat * glm::rotate(glm::mat4(1.f), walkRoll,   glm::vec3(0.f, 0.f, 1.f));
        dogMat = dogMat * glm::scale(glm::mat4(1.f), glm::vec3(DOG_SC));
        // X축 180°: (x,y,z)→(x,-y,-z) → 발Y=0, 코Z=+13(+Z), 꼬리Z=-13(-Z)
        dogMat = dogMat * glm::rotate(glm::mat4(1.f), 3.14159265f, glm::vec3(1.f, 0.f, 0.f));
        dogMat = dogMat * glm::translate(glm::mat4(1.f), glm::vec3(0.f, 18.50f, 13.0f));

        // ===========================================
        // PASS 2 - Main render  →  HDR framebuffer
        // ===========================================
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Shared corridor shader setup
        corridorShader.use();
        corridorShader.setMat4("projection",        proj);
        corridorShader.setMat4("view",              view);
        corridorShader.setMat4("model",             identity);
        corridorShader.setVec3("viewPos",           camPos);
        corridorShader.setVec3("lampPositions[0]",  glm::vec3(0.0f, 2.3f, -2.0f));
        corridorShader.setVec3("lampPositions[1]",  glm::vec3(0.0f, 2.3f, -6.0f));
        corridorShader.setVec3("lampPositions[2]",  glm::vec3(0.0f, 2.3f, -10.0f));
        corridorShader.setVec3("lampColor",         glm::vec3(1.0f, 0.95f, 0.85f));
        corridorShader.setMat4("lightSpaceMatrix0", identity);
        corridorShader.setMat4("lightSpaceMatrix1", identity);
        corridorShader.setMat4("lightSpaceMatrix2", identity);
        corridorShader.setVec3("doorLightPos",      doorLightPos);
        corridorShader.setVec3("doorLightColor",    doorLightCol);

        // -- Floor rendered later in wallNMShader block (normal-mapped) --

        // -- Ceiling --
        glActiveTexture(GL_TEXTURE0);            // ← unit 0 active before diffuse bind
        glBindTexture(GL_TEXTURE_2D, texCeil);
        corridorShader.setVec3("colorTint", glm::vec3(1.0f));
        glBindVertexArray(corridor.ceilVAO);
        glDrawArrays(GL_TRIANGLES, 0, corridor.ceilVerts);

        // -- Walls: Normal-Mapped (Blinn-Phong in tangent space) --
        wallNMShader.use();
        wallNMShader.setMat4("model",             identity);
        wallNMShader.setMat4("view",              view);
        wallNMShader.setMat4("projection",        proj);
        wallNMShader.setVec3("viewPos",           camPos);
        wallNMShader.setVec3("lampPositions[0]",  glm::vec3(0.0f, 2.3f, -2.0f));
        wallNMShader.setVec3("lampPositions[1]",  glm::vec3(0.0f, 2.3f, -6.0f));
        wallNMShader.setVec3("lampPositions[2]",  glm::vec3(0.0f, 2.3f, -10.0f));
        wallNMShader.setVec3("lampColor",         glm::vec3(1.0f, 0.95f, 0.85f));
        wallNMShader.setMat4("lightSpaceMatrix0", identity);
        wallNMShader.setMat4("lightSpaceMatrix1", identity);
        wallNMShader.setMat4("lightSpaceMatrix2", identity);
        wallNMShader.setVec3("doorLightPos",      doorLightPos);
        wallNMShader.setVec3("doorLightColor",    doorLightCol);
        wallNMShader.setVec3("colorTint",         glm::vec3(1.0f));

        // -- Floor (normal-mapped, Marble) --
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texFloor);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texFloorNM);
        wallNMShader.setVec3("colorTint", glm::vec3(1.0f));
        glBindVertexArray(floorNM.VAO);
        glDrawArrays(GL_TRIANGLES, 0, floorNM.cnt);

        // -- Walls (normal-mapped, Bricks) --
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWall);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texWallNM);
        glBindVertexArray(wallNM.VAO);
        glDrawArrays(GL_TRIANGLES, 0, wallNM.cnt);

        // Restore corridorShader for subsequent draws
        corridorShader.use();
        glActiveTexture(GL_TEXTURE0);

        // -- Baseboard (dark tint) --
        corridorShader.setVec3("colorTint", glm::vec3(0.28f, 0.25f, 0.22f));
        glBindVertexArray(baseVAO);
        glDrawArrays(GL_TRIANGLES, 0, baseCnt);

        // -- Door (primitive flat quad, dark charcoal texture) --
        glBindTexture(GL_TEXTURE_2D, texDoor);
        corridorShader.setVec3("colorTint", glm::vec3(1.0f));
        glBindVertexArray(doorVAO);
        glDrawArrays(GL_TRIANGLES, 0, doorCnt);

        // -- Door handle (PBR: Cook-Torrance BRDF, gold/brass) --
        pbrShader.use();
        pbrShader.setMat4("model", identity);
        pbrShader.setMat4("view", view);
        pbrShader.setMat4("projection", proj);
        pbrShader.setVec3("viewPos", camPos);

        // Gold material
        pbrShader.setVec3("albedo", glm::vec3(1.00f, 0.71f, 0.29f));
        pbrShader.setFloat("metallic", 0.95f);
        pbrShader.setFloat("roughness", 0.20f);
        pbrShader.setFloat("ao", 1.00f);

        // 3 ceiling lamps as PBR point lights (위치 고정 상수)
        pbrShader.setInt("numLights", 3);
        pbrShader.setVec3("lightPositions[0]", glm::vec3(0.0f, 2.3f, -2.0f));
        pbrShader.setVec3("lightPositions[1]", glm::vec3(0.0f, 2.3f, -6.0f));
        pbrShader.setVec3("lightPositions[2]", glm::vec3(0.0f, 2.3f, -10.0f));

        // 💡 수정본: 변수 선언 없이 셰이더에 18.0f(물리 광량값)를 직접 전달
        pbrShader.setVec3("lightColors[0]", glm::vec3(18.0f, 18.0f, 18.0f));
        pbrShader.setVec3("lightColors[1]", glm::vec3(18.0f, 18.0f, 18.0f));
        pbrShader.setVec3("lightColors[2]", glm::vec3(18.0f, 18.0f, 18.0f));

        // 문고리 메쉬 그리기
        glBindVertexArray(handleVAO);
        glDrawArrays(GL_TRIANGLES, 0, handleCnt);

        // Restore corridorShader after PBR
        corridorShader.use();
        corridorShader.setMat4("view",       view);
        corridorShader.setMat4("projection", proj);
        corridorShader.setMat4("model",      identity);
        glActiveTexture(GL_TEXTURE0);

        // -- Ceiling fixture housing (grey metal) --
        glBindTexture(GL_TEXTURE_2D, texCeil);
        corridorShader.setVec3("colorTint", glm::vec3(0.62f, 0.62f, 0.64f));
       

        // -- Chairs (wood, two against left wall) --
        glBindTexture(GL_TEXTURE_2D, texWood);
        corridorShader.setVec3("colorTint", glm::vec3(1.0f));
        glBindVertexArray(chairVAO1); glDrawArrays(GL_TRIANGLES,0,chairCnt1);
        glBindVertexArray(chairVAO2); glDrawArrays(GL_TRIANGLES,0,chairCnt2);

        // -- Picture frame borders (dark wood) --
        glBindTexture(GL_TEXTURE_2D, texWood);
        corridorShader.setVec3("colorTint", glm::vec3(0.30f, 0.18f, 0.08f));
        glBindVertexArray(borderVAO);
        glDrawArrays(GL_TRIANGLES, 0, borderCnt);

        // -- Paintings (3 procedural types: city night / warm abstract / green nature) --
        corridorShader.setVec3("colorTint", glm::vec3(1.0f));
        for (int pi = 0; pi < 3; pi++) {
            glBindTexture(GL_TEXTURE_2D, texPaint[pi]);
            glBindVertexArray(paintVAO[pi]);
            glDrawArrays(GL_TRIANGLES, 0, paintCnt[pi]);
        }

        // -- Primitive dog (follows camera, walk-animated) --
        {
            static std::vector<float> primDogV;
            buildDogBody(primDogV, gDogWalk, gDogMoving);
            glBindBuffer(GL_ARRAY_BUFFER, dogBodyVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            (GLsizeiptr)(primDogV.size() * sizeof(float)),
                            primDogV.data());
            glm::mat4 primDogMat = glm::translate(glm::mat4(1.f),
                glm::vec3(gDogPos.x, gDogPos.y + walkBob, gDogPos.z));
            primDogMat = primDogMat * glm::rotate(glm::mat4(1.f),
                gDogYawRad, glm::vec3(0.f, 1.f, 0.f));
            corridorShader.setMat4("model", primDogMat);
            glBindTexture(GL_TEXTURE_2D, texDogFurColor);
            corridorShader.setVec3("colorTint", glm::vec3(1.0f));
            glBindVertexArray(dogBodyVAO);
            glDrawArrays(GL_TRIANGLES, 0, (int)(primDogV.size() / 8));
            corridorShader.setMat4("model", identity);
        }

        // -- Dog house body (brick, normal-mapped, wallNMShader) --
        // wallNMShader view/proj/lightSpaceMatrix/lampPositions were already set
        // in the wall+floor block above and are still valid (uniforms persist).
        wallNMShader.use();
        wallNMShader.setMat4("model", identity);        // dog house is world-space
        wallNMShader.setVec3("colorTint", glm::vec3(1.0f));
        // Rebind shadow maps to wallNMShader units 2/3/4
      
        // Fabric080 diffuse (unit 0) + NM (unit 1)
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texDogHouseColor);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texDogHouseNM);
        glBindVertexArray(dogHouseVAO); glDrawArrays(GL_TRIANGLES, 0, dogHouseCnt);

        // -- Potted plants: 잎=Grass NM / 화분=Terrazzo004 NM (wallNMShader) --
        // shadow maps 이미 units 2/3/4 에 바인딩, 기타 uniforms 유효
        wallNMShader.setVec3("colorTint", glm::vec3(1.0f));
        wallNMShader.setMat4("model", plantMat1);
        drawPlantLeavesNM(plant, texGrassColor, texGrassNM);
        drawPlantPotNM(plant, texPotColor, texPotNM);
        wallNMShader.setMat4("model", plantMat2);
        drawPlantLeavesNM(plant, texGrassColor, texGrassNM);
        drawPlantPotNM(plant, texPotColor, texPotNM);
        wallNMShader.setMat4("model", identity);

        // Door interior panel: corridorShader, very dark (shadowed-opening look)
        corridorShader.use();
    
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWall);
        corridorShader.setVec3("colorTint", glm::vec3(0.04f, 0.03f, 0.02f));
        glBindVertexArray(dogHouseDoorVAO); glDrawArrays(GL_TRIANGLES, 0, dogHouseDoorCnt);
        corridorShader.setVec3("colorTint", glm::vec3(1.0f)); // restore

        // -- Dog companion (OBJ 모델, wallNMShader) --
        // view/proj/lightSpaceMatrix/lampPositions 는 위 wall 블록에서 설정한 값 유효.
        wallNMShader.use();
        wallNMShader.setMat4("model", dogMat);
       
        // 실제 털 사진 텍스처 사용 → colorTint 1.0 (보정 없음)
        wallNMShader.setVec3("colorTint", glm::vec3(1.0f));
        drawPlantNM(dogOBJ, texDogFurColor, texDogFurNM);


        // ---- Emissive pass (unlit: lamp strips, colored accents) ----
        emissiveShader.use();
        emissiveShader.setMat4("projection", proj);
        emissiveShader.setMat4("view",       view);
        emissiveShader.setMat4("model",      identity);
        emissiveShader.setFloat("emissiveAlpha", 1.0f);

        // -- Lamp emissive strips (over-bright to trigger bloom) --
        emissiveShader.setInt("useTexture", 0);
        emissiveShader.setVec3("emissiveColor", glm::vec3(3.2f, 3.0f, 2.6f));
     
        

        // -- Glass vase (right side, z = -8.8) --
        if (!vaseGone) {
            if (!vaseExploding) {
                // Intact: translucent glass look via emissiveShader
                emissiveShader.use();
                emissiveShader.setMat4("model",          identity);
                emissiveShader.setMat4("view",           view);
                emissiveShader.setMat4("projection",     proj);
                emissiveShader.setFloat("emissiveAlpha", 0.52f);
                emissiveShader.setInt ("useTexture",     0);
                emissiveShader.setVec3("emissiveColor",  glm::vec3(0.48f, 0.82f, 0.90f));
                glBindVertexArray(vaseVAO); glDrawArrays(GL_TRIANGLES, 0, vaseCnt);
            } else {
                // Exploding: geometry shader separates triangles, alpha fades out
                explodeShader.use();
                explodeShader.setMat4 ("model",        identity);
                explodeShader.setMat4 ("view",         view);
                explodeShader.setMat4 ("projection",   proj);
                explodeShader.setFloat("explodeTime",  vaseExplodeTime);
                explodeShader.setVec3 ("colorTint",    glm::vec3(0.48f, 0.82f, 0.90f));
                glBindVertexArray(vaseVAO); glDrawArrays(GL_TRIANGLES, 0, vaseCnt);
            }
        }

        // -- Dust / light particles (additive blend, no depth write) --
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);          // additive: particles add light
        particleShader.use();
        particleShader.setMat4("view",           view);
        particleShader.setMat4("projection",     proj);
        particleShader.setVec3("particleColor",  glm::vec3(1.0f, 0.95f, 0.78f));
        glBindVertexArray(particleVAO);
        glDrawArrays(GL_TRIANGLES, 0, MAX_PARTICLES * 6);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // restore
        glDepthMask(GL_TRUE);

        // End HDR pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ===========================================
        // PASS 3 - Extract bright pixels
        // ===========================================
        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, brightFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        brightShader.use();
        brightShader.setInt("scene", 0);
        brightShader.setFloat("threshold", 0.85f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        glBindVertexArray(screenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ===========================================
        // PASS 4 - Gaussian blur  (10 ping-pong passes)
        // ===========================================
        blurShader.use();
        blurShader.setInt("image", 0);
        bool horizontal = true, firstIter = true;
        for (int i = 0; i < 10; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal ? 1 : 0]);
            blurShader.setInt("horizontal", horizontal ? 1 : 0);
            glBindTexture(GL_TEXTURE_2D,
                          firstIter ? brightTex : pingpongTex[horizontal ? 0 : 1]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
            firstIter  = false;
        }
        // After 10 iters, last write was to pingpongFBO[0] → pingpongTex[0]

        // ===========================================
        // PASS 5 – Bloom final composite
        //   Wine OFF → render directly to screen
        //   Wine ON  → render to wineFBO; PASS 5.5 applies drunk fx
        // ===========================================
        if (wineActive) {
            glBindFramebuffer(GL_FRAMEBUFFER, wineFBO);
            glClear(GL_COLOR_BUFFER_BIT);         // wineFBO has no depth RBO
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        bloomFinalShader.use();
        bloomFinalShader.setInt("scene",       0);
        bloomFinalShader.setInt("bloomBlur",   1);
        bloomFinalShader.setFloat("exposure",      1.1f);
        bloomFinalShader.setFloat("bloomStrength", 0.75f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[0]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_BLEND);

        // ===========================================
        // PASS 5.5 – Wine / drunk screen effect
        //   Reads wineFBO texture → applies blur,
        //   distortion, chromatic aberration, shake.
        //   Only executes when wine is active (key F).
        // ===========================================
        if (wineActive) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            wineShader.use();
            wineShader.setFloat("time",      now);
            wineShader.setFloat("intensity", 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, wineColorTex);
            glBindVertexArray(screenQuadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // -- Fade overlay (applied directly on the final screen) --
        if (fadeAlpha > 0.0f) {
            glDisable(GL_DEPTH_TEST);
            fadeShader.use();
            fadeShader.setFloat("alpha", fadeAlpha);
            glBindVertexArray(fadeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

} // namespace Corridor
