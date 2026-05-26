#include <glad/glad.h>
#include <GLFW/glfw3.h>

// stb_image implementation compiled once via corridor_scene.cpp → model.h
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/scene.h>
#include <learnopengl/lighting.h>
#include <learnopengl/ray_caster.h>
#include <learnopengl/interactable.h>
#include <learnopengl/primitive.h>

#include <iostream>
#include <string>
#include <vector>

namespace Office {

// forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void ResolvePlayerCollision(glm::vec3& pos, float radius);
unsigned int createQuadVAO();
// LoadTexture and SolidTexture come from the global primitive.h include above

// settings
const unsigned int SCR_WIDTH   = 1280;
const unsigned int SCR_HEIGHT  = 720;
const unsigned int SHADOW_WIDTH  = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
const unsigned int POINT_SHADOW_SIZE = 1024;
const float        POINT_SHADOW_FAR  = 15.0f;

// camera clamp boundaries (0.2m margin from each wall)
const glm::vec3 ROOM_MIN(-6.6f, 1.2f, -6.6f);
const glm::vec3 ROOM_MAX( 6.6f, 1.35f, 6.6f);

// player collision cylinder radius (XZ plane, meters)
const float PLAYER_RADIUS = 0.30f;

// room object: a primitive mesh with its textures and transform
struct RoomObject
{
    PrimMesh     mesh;
    unsigned int diffuseTex;
    unsigned int specularTex;
    glm::mat4    modelMatrix;
};

// scene
SceneState sceneState;

// camera — starts inside room facing toward desk (default Front = -Z)
Camera camera(glm::vec3(0.0f, 1.275f, 2.625f));
float lastX      = SCR_WIDTH  / 2.0f;
float lastY      = SCR_HEIGHT / 2.0f;
bool  firstMouse = true;

// mouse delta accumulated each frame for HUD arm wobble
float mouseDeltaX = 0.0f;
float mouseDeltaY = 0.0f;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// walking bob state
float bobPhase  = 0.0f;
float bobOffY   = 0.0f;
float bobOffX   = 0.0f;
bool  isWalking = false;

// collision boxes for major furniture (XZ only)
std::vector<AABB> gCollisionBoxes;

// lighting
PointLight ceilingLight;
PointLight lampLight1;
PointLight lampLight2;

// -----------------------------------------------------------------------
// BuildScene: creates room structure (permanent primitive) and furniture
// (primitive, to be replaced by .obj later). Fills interactables list.
// Room: 6.75m wide x 2.25m tall x 6.75m deep, centred on XZ origin.
// -----------------------------------------------------------------------




void BuildScene(std::vector<RoomObject>& objs, std::vector<InteractableObject>& interactables)
{
    // shared plane meshes (room: 13.5m wide x 2.25m tall x 13.5m deep)
    PrimMesh plane99 = CreatePlane(13.5f, 13.5f, 4.0f);  // floor / ceiling
    PrimMesh plane93 = CreatePlane(13.5f, 2.25f, 2.0f); // front / back wall
    PrimMesh plane39 = CreatePlane(2.25f, 13.5f, 2.0f); // left / right wall

    // [수정] 중복 선언을 제거하고 업로드하신 WoodFloor014 에셋의 정확한 파일명으로 로드합니다.
    // (에셋 파일이 resources/textures/ 폴더 안에 들어가 있다고 가정합니다)
    unsigned int floorTex  = LoadTexture("resources/textures/WoodFloor.jpg");
    unsigned int wallTex   = LoadTexture("resources/textures/Concrete.jpg");
    unsigned int ceilTex   = LoadTexture("resources/textures/Concrete.jpg");
    unsigned int carpetTex = LoadTexture("resources/textures/Carpet006_Color.jpg");
    unsigned int carpetTex2 = LoadTexture("resources/textures/Carpet4.jpg");

    // solid-color textures (나머지 가구 및 벽면은 기존 단색 유지)
    unsigned int deskTex = SolidTexture(225, 225, 225);  // neutral white desk
    unsigned int chairTex = SolidTexture(200, 200, 200);  // neutral gray
    unsigned int monitorTex = SolidTexture(15, 15, 15);  // black (keep)
    unsigned int shelfTex = SolidTexture(228, 228, 228);  // neutral shelf
    unsigned int frameTex = SolidTexture(238, 238, 238);  // neutral frame
    unsigned int defaultSpec = SolidTexture(55, 55, 55);  // moderate spec
    unsigned int cabinetTex = SolidTexture(218, 218, 218);  // neutral cabinet
    unsigned int sofaTex = SolidTexture(210, 210, 210);  // neutral sofa
    unsigned int cTblTex = SolidTexture(232, 232, 232);  // neutral table
    unsigned int confTblTex = SolidTexture(228, 228, 228);  // neutral conf table
    unsigned int confChTex = SolidTexture(28, 28, 28);  // black accent (keep)
    unsigned int wbTex = SolidTexture(248, 248, 248);  // neutral board
    unsigned int tvTex = SolidTexture(8, 8, 8);  // black TV (keep)
    unsigned int tvStandTex = SolidTexture(200, 200, 200);  // neutral stand
    unsigned int potTex = SolidTexture(225, 225, 225);  // neutral ceramic
    unsigned int plantTex = SolidTexture(35, 115, 40);  // green (keep)
    unsigned int windowFrTex = SolidTexture(228, 228, 228);  // white window frame
    unsigned int glassTex = SolidTexture(195, 215, 235);  // pale sky blue
    unsigned int lampBaseTex = SolidTexture(172, 172, 172);  // brushed chrome
    unsigned int lampShdTex = SolidTexture(242, 242, 242);  // white shade
    unsigned int artFrTex = SolidTexture(185, 185, 185);  // gray art frame
    unsigned int art1Tex = SolidTexture(162, 180, 198);  // soft blue art
    unsigned int art2Tex = SolidTexture(178, 196, 178);  // soft green art
    unsigned int keyboardTex = SolidTexture(38, 38, 38);  // dark keyboard
    unsigned int mugTex = SolidTexture(242, 242, 242);  // white mug
    unsigned int rugTex = SolidTexture(178, 182, 188);  // cool gray rug
    unsigned int clockFrTex = SolidTexture(32, 32, 35);  // dark clock frame
    unsigned int floatShfTex = SolidTexture(230, 228, 224);  // floating shelf
    unsigned int book1Tex = SolidTexture(25, 45, 120);  // navy
    unsigned int book2Tex = SolidTexture(110, 20, 30);  // burgundy
    unsigned int book3Tex = SolidTexture(20, 75, 40);  // forest green
    unsigned int book4Tex = SolidTexture(170, 80, 20);  // amber
    unsigned int book5Tex = SolidTexture(80, 20, 95);  // plum
    unsigned int art3Tex = SolidTexture(85, 165, 170);  // teal
    unsigned int art4Tex = SolidTexture(190, 140, 60);  // amber art
    unsigned int art5Tex = SolidTexture(80, 90, 160);  // indigo
    unsigned int art6Tex = SolidTexture(195, 100, 80);  // coral
    unsigned int pedestalTex = SolidTexture(225, 225, 225);  // white pedestal body
    unsigned int holder1Tex = SolidTexture(70, 110, 155);  // steel-blue placeholder (left)
    unsigned int holder2Tex = SolidTexture(155, 110, 70);  // warm-amber placeholder (right)

    // helper: push a RoomObject
    auto Push = [&](PrimMesh mesh, unsigned int diff, glm::mat4 mat)
    {
        RoomObject obj;
        obj.mesh = mesh;
        obj.diffuseTex = diff;
        obj.specularTex = defaultSpec;
        obj.modelMatrix = mat;
        objs.push_back(obj);
    };

    // ----- room structure (permanent primitive) -----

    // floor — plane in XZ, normal +Y, y=0 (여기에 이미지 텍스처인 floorTex가 바인딩됩니다)
    Push(plane99, floorTex, glm::mat4(1.0f));

    // desk carpet — 2.5m x 2.0m, y=0.005 to avoid z-fighting with floor
    {
        PrimMesh carpetMesh = CreatePlane(4.5f, 4.0f, 3.0f);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.005f, -5.0f));
        Push(carpetMesh, carpetTex, m);
    }

    // sofa carpet — 2.5m x 2.0m, y=0.005 to avoid z-fighting with floor
    {
        PrimMesh carpetMesh = CreatePlane(5.3f, 5.6f, 3.0f);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.005f, -0.2f));
        Push(carpetMesh, carpetTex2, m);
    }

    // ceiling — rotate 180 around X so normal faces -Y, translate to y=2.25
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 2.25f, 0));
        m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1, 0, 0));
        Push(plane99, ceilTex, m);
    }

    // back wall — z=-6.75, rotate +90 around X so normal faces +Z
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.125f, -6.75f));
        m = glm::rotate(m, glm::radians(90.0f), glm::vec3(1, 0, 0));
        Push(plane93, wallTex, m);
    }

    // front wall — z=+6.75, rotate -90 around X so normal faces -Z
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.125f, 6.75f));
        m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1, 0, 0));
        Push(plane93, wallTex, m);
    }

    // left wall — x=-6.75, rotate -90 around Z so normal faces +X
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.75f, 1.125f, 0));
        m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(0, 0, 1));
        Push(plane39, wallTex, m);
    }

    // right wall — x=+6.75, rotate +90 around Z so normal faces -X
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.75f, 1.125f, 0));
        m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0, 0, 1));
        Push(plane39, wallTex, m);
    }

    // ----- interactables -----
    // monitor → GitHub
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-0.55f, 0.8f, -3.3f), glm::vec3(0.55f, 1.15f, -2.5f) };
        obj.label = "GitHub";
        obj.url = "https://sumting.co.kr/";
        interactables.push_back(obj);
    }
    // picture frame → Portfolio
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(1.6f, 0.9f, -9.1f), glm::vec3(2.4f, 2.1f, -8.8f) };
        obj.label = "Portfolio";
        obj.url = "https://yourportfolio.com";
        interactables.push_back(obj);
    }
    // left pedestal placeholder → Project A
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-8.55f, 0.9f, -5.85f), glm::vec3(-7.85f, 1.75f, -5.15f) };
        obj.label = "Project A";
        obj.url = "https://github.com/yourname/project-a";
        interactables.push_back(obj);
    }
    // right pedestal placeholder → Project B
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(7.85f, 0.9f, -5.85f), glm::vec3(8.55f, 1.75f, -5.15f) };
        obj.label = "Project B";
        obj.url = "https://github.com/yourname/project-b";
        interactables.push_back(obj);
    }

    // ----- left wall paintings -----
    // painting.glb at (-6.74, 1.3, 5.5) scale 1.0 — roughly 1.0m wide, 0.8m tall
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-6.75f, 0.9f, 5.0f), glm::vec3(-6.40f, 1.9f, 6.0f) };
        obj.label  = "Painting 1";
        obj.url    = "https://github.com/leegoeun-art";
        interactables.push_back(obj);
    }
    // psx_painting at (-6.74, 0.8, 4.0) scale 0.7
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-6.75f, 0.5f, 3.55f), glm::vec3(-6.40f, 1.35f, 4.45f) };
        obj.label  = "Painting 2 (PSX)";
        obj.url    = "https://sumting.co.kr/";
        interactables.push_back(obj);
    }
    // lowpoly_painting at (-6.74, 1.3, 2.5) scale 0.5
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-6.75f, 1.0f, 2.1f), glm::vec3(-6.40f, 1.7f, 2.9f) };
        obj.label  = "Painting 3 (Lowpoly)";
        obj.url    = "https://google.com";
        interactables.push_back(obj);
    }

    // ----- right wall trophies -----
    // golden trophy — col z=6.0, row y=0.6 & 1.1, x≈6.6, scale 0.066
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(6.35f, 0.55f, 5.65f), glm::vec3(6.75f, 1.45f, 6.35f) };
        obj.label  = "Golden Trophy";
        obj.url    = "https://github.com/somgam6373";
        interactables.push_back(obj);
    }
    // paw trophy — col z=4.5, row y=0.6 & 1.1, x≈6.5, scale 0.044
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(6.30f, 0.55f, 4.15f), glm::vec3(6.75f, 1.45f, 4.85f) };
        obj.label  = "Paw Trophy";
        obj.url    = "https://sumting.co.kr/";
        interactables.push_back(obj);
    }
    // world cup trophy — col z=3.0, row y=0.6 & 1.1, x≈6.6, scale 0.01
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(6.35f, 0.55f, 2.65f), glm::vec3(6.75f, 1.45f, 3.35f) };
        obj.label  = "World Cup Trophy";
        obj.url    = "https://www.loakong.com/";
        interactables.push_back(obj);
    }
}

void DrawPrimitive(Shader& shader, const RoomObject& obj)
{
    shader.setMat4("model", obj.modelMatrix);
    shader.setBool("hasDiffuseTex", true);
    shader.setBool("hasNormalTex",  false);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, obj.diffuseTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, obj.specularTex);
    glBindVertexArray(obj.mesh.VAO);
    glDrawElements(GL_TRIANGLES, obj.mesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void DrawPrimitiveShadow(Shader& shader, const RoomObject& obj)
{
    shader.setMat4("model", obj.modelMatrix);
    glBindVertexArray(obj.mesh.VAO);
    glDrawElements(GL_TRIANGLES, obj.mesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Run(GLFWwindow* window)
{
    // re-register callbacks for this scene
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // reset per-scene state
    firstMouse = true;
    lastX = SCR_WIDTH / 2.0f;
    lastY = SCR_HEIGHT / 2.0f;
    lastFrame = (float)glfwGetTime();
    camera = Camera(glm::vec3(0.0f, 1.275f, 2.625f));

    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // shadow map FBO
    // ------------------------------------------------------------------------
    unsigned int shadowFBO;
    glGenFramebuffers(1, &shadowFBO);
    unsigned int shadowMap;
    glGenTextures(1, &shadowMap);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // point light shadow cubemap (ceiling light — omnidirectional)
    // ------------------------------------------------------------------------
    unsigned int pointShadowFBO;
    glGenFramebuffers(1, &pointShadowFBO);
    unsigned int pointShadowMap;
    glGenTextures(1, &pointShadowMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowMap);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                     POINT_SHADOW_SIZE, POINT_SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, pointShadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // HDR FBO (멀티샘플, RGBA16F) — 씬을 선형 HDR 색공간으로 렌더링
    // ------------------------------------------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    unsigned int hdrColorMSTex;
    glGenTextures(1, &hdrColorMSTex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, hdrColorMSTex);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE, hdrColorMSTex, 0);

    unsigned int hdrDepthStencilRBO;
    glGenRenderbuffers(1, &hdrDepthStencilRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthStencilRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, hdrDepthStencilRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR: HDR framebuffer not complete" << std::endl;

    // resolve FBO (단일 샘플, RGBA16F) — MSAA blit 후 톤매핑이 읽는 버퍼
    // ------------------------------------------------------------------------
    unsigned int hdrResolveFBO;
    glGenFramebuffers(1, &hdrResolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrResolveFBO);

    unsigned int hdrColorTex;
    glGenTextures(1, &hdrColorTex);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR: HDR resolve framebuffer not complete" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 풀스크린 쿼드 VAO (톤매핑 패스용, NDC -1~+1)
    // ------------------------------------------------------------------------
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuadVerts), screenQuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // build and compile shaders
    // -------------------------
    Shader officeShader("shader/office.vs", "shader/office.fs");
    Shader hudShader("shader/hud.vs",       "shader/hud.fs");
    Shader uiShader("shader/ui.vs",         "shader/ui.fs");
    Shader glassShader("shader/glass.vs",   "shader/glass.fs");
    Shader outlineShader("shader/outline.vs", "shader/outline.fs");
    Shader shadowShader(     "shader/shadow.vs",       "shader/shadow.fs");
    Shader pointShadowShader("shader/point_shadow.vs", "shader/point_shadow.fs", "shader/point_shadow.gs");
    Shader hdrShader(        "shader/hdr.vs",          "shader/hdr.fs");

    // bind texture units once — officeShader: 0=diffuse, 1=specular, 2=normal, 10=shadowMap, 11=pointShadowMap
    officeShader.use();
    officeShader.setInt("texture_diffuse1",    0);
    officeShader.setInt("texture_specular1",   1);
    officeShader.setInt("texture_normal1",     2);
    officeShader.setBool("hasNormalTex",       false);
    officeShader.setFloat("uEmissiveStrength", 0.0f);
    officeShader.setInt("shadowMap",           10);
    officeShader.setInt("shadowCubeMap",       11);
    officeShader.setFloat("shadowFarPlane",    POINT_SHADOW_FAR);


    // HUD weapon — Sailor Moon stick (held first-person, no arm visible)
    Model moonStickModel("resources/hud/moon_stick.glb");

    // office desk model (Adjustable Desk by myDesk.obj)
    Model deskModel("resources/office/desk/myDesk.obj");
    Model deskModel2("resources/office/desk2/desk2.obj");
    // office chair model
    Model chairModel("resources/office/chair/chair.obj");
    // dual monitor on sit-stand arm
    Model monitorModel("resources/office/monitor/monitor.obj");
    // floor lamp (shared for both lamp positions)
    Model lampModel("resources/office/lamp/lamp.obj");
    Model filingCabinetModel("resources/office/filing_cabinet/filing_cabinet.obj");
    Model whiteboardModel("resources/office/filing_cabinet/whiteboard.glb");
    Model bookcaseModel("resources/office/bookcase/bookcase.obj");
    Model bookshelfModel("resources/office/bookshelf/bookshelf.glb");
    Model closetModel("resources/office/closet/closet_2.glb");
    Model wineRackModel("resources/office/wine_rack/wine_rack.glb");
    Model kitchenShelfModel("resources/office/kitchen_shelf/old_kitchen_shelf.glb");
    Model trophyModel("resources/office/trophy/golden_trophy.glb");
    Model pawTrophyModel("resources/office/trophy/paw_trophy.glb");
    Model worldCupTrophyModel("resources/office/trophy/world_cup_trophy.glb");
    Model armchairModel("resources/office/armchair/armchair.obj");
    Model sofaModel("resources/office/sofa/sofa.obj");
    Model windowModel("resources/office/window/door_window_thick_double_2x4.obj");
    Model ceilingFixtureModel("resources/office/ceiling_fixture/11824_ceiling_fixture_v1_L3.obj");
    Model psxPaintingModel("resources/office/painting/psx_painting.glb");
    Model lowpolyPaintingModel("resources/office/painting/painting_lowpoly.glb");
    Model paintingModel("resources/office/painting/painting.glb");
    Model hangerModel("resources/office/hanger/hanger_with_hat_and_scarf.glb");
    Model wineCabinetModel("resources/office/wine_cabinet/wine_cabinet.glb");
    Model doorModel("resources/office/door/old_wooden_door.glb");
    Model carModel("resources/office/car/pontiac_firebird_formula_1974.glb", false, true);

    // ceiling point light
    // -------------------
    ceilingLight.position  = glm::vec3(0.0f, 1.85f, 0.0f);  // fixture bottom (ceiling y=2.25, 40cm pendant)
    ceilingLight.ambient   = glm::vec3(0.10f, 0.10f, 0.12f);  // cool white ambient
    ceilingLight.diffuse   = glm::vec3(1.00f, 1.00f, 1.00f);  // neutral white
    ceilingLight.specular  = glm::vec3(0.90f);
    ceilingLight.constant  = 1.0f;
    ceilingLight.linear    = 0.020f;
    ceilingLight.quadratic = 0.0020f;
    ceilingLight.enabled   = true;

    // floor lamp 1 (right of desk, x=+1.56, z=-3.50) — warm amber
    lampLight1.position  = glm::vec3(1.56f, 1.60f, -3.50f);
    lampLight1.ambient   = glm::vec3(0.08f, 0.05f, 0.02f);
    lampLight1.diffuse   = glm::vec3(1.00f, 0.65f, 0.25f);
    lampLight1.specular  = glm::vec3(0.50f, 0.33f, 0.13f);
    lampLight1.constant  = 1.0f;
    lampLight1.linear    = 0.14f;
    lampLight1.quadratic = 0.07f;
    lampLight1.enabled   = true;

    // floor lamp 2 (left of desk, x=-1.56, z=-3.50) — same warm amber
    lampLight2.position  = glm::vec3(-1.56f, 1.60f, -3.50f);
    lampLight2.ambient   = lampLight1.ambient;
    lampLight2.diffuse   = lampLight1.diffuse;
    lampLight2.specular  = lampLight1.specular;
    lampLight2.constant  = lampLight1.constant;
    lampLight2.linear    = lampLight1.linear;
    lampLight2.quadratic = lampLight1.quadratic;
    lampLight2.enabled   = true;

    // build scene
    // -----------
    std::vector<RoomObject>         sceneObjects;
    std::vector<InteractableObject> interactables;
    BuildScene(sceneObjects, interactables);

    // lamp interactables (no URL — toggled by L key)
    // AABB covers full lamp body at scale 2.0: ±0.8m wide, 0~2m tall
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(0.76f, 0.0f, -4.30f), glm::vec3(2.36f, 2.0f, -2.70f) };
        obj.label = "Lamp 1";
        obj.url   = "";
        interactables.push_back(obj);
    }
    const int lamp1InteractIdx = (int)interactables.size() - 1;

    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-2.36f, 0.0f, -4.30f), glm::vec3(-0.76f, 2.0f, -2.70f) };
        obj.label = "Lamp 2";
        obj.url   = "";
        interactables.push_back(obj);
    }
    const int lamp2InteractIdx = (int)interactables.size() - 1;

    // desk interactable — 3-second hover opens URL
    {
        InteractableObject obj;
        obj.bounds = { glm::vec3(-1.0f, 0.0f, -5.5f), glm::vec3(1.0f, 0.9f, -4.5f) };
        obj.label  = "Desk";
        obj.url    = "https://your-desk-url.com";
        interactables.push_back(obj);
    }
    const int deskInteractIdx = (int)interactables.size() - 1;

    // collision boxes for major furniture (XZ only, Y ignored at runtime)
    // 카페트 평면 자체는 충돌 없음 — 아래 박스는 실제 가구 위치만 커버
    // ------------------------------------------------------------------------
    gCollisionBoxes = {
        // ── 책상 (deskModel2, z=-5.0, -270°Y 회전으로 로컬X→월드Z, 로컬Z→월드X)
        // 확장 박스: 다리/모니터암 돌출 포함, 카페트 측면 통로 확보
        { {-1.50f, 0.0f, -4.10f}, { 1.50f, 1.2f, -3.00f} },

        // ── 플로어 램프 (scale 2.0, 얇은 폴 → 작은 XZ 반경)
        { { 1.10f, 0.0f, -3.85f}, { 2.05f, 2.1f, -3.15f} },  // lamp 1 (right, x=1.56)
        { {-2.05f, 0.0f, -3.85f}, {-1.10f, 2.1f, -3.15f} },  // lamp 2 (left,  x=-1.56)

        // ── 소파 (scale 0.5; 각 소파 ~1.1m wide x 0.8m deep 추정)
        { {-2.65f, 0.0f, -2.25f}, {-1.35f, 1.0f, 1.05f} },  // sofa L-front (z=-1.2)
        { {-2.65f, 0.0f,  1.05f}, {-1.35f, 1.0f,  2.25f} },  // sofa L-back  (z=+1.2)
        { { 1.35f, 0.0f, -2.25f}, { 2.65f, 1.0f, 1.05f} },  // sofa R-front
        { { 1.35f, 0.0f,  1.05f}, { 2.65f, 1.0f,  2.25f} },  // sofa R-back

        // ── 왼쪽 벽
        { {-6.75f, 0.0f, -6.61f}, {-6.50f, 1.5f, 2.80f} },  // 파일 캐비넷 4개 (z=-1.2~-3.0)
        { {-6.75f, 0.0f, -6.61f}, {-5.30f, 2.2f, -3.60f} },  // 책장 (z=-3.8, 확장: 풀 모델 커버)
        { {-6.75f, 0.0f, -2.60f}, {-6.25f, 2.0f,  1.40f} },  // 화이트보드 (z=0.5, scale 0.015)

        // ── 오른쪽 벽
        { { 5.50f, 0.0f, -2.80f}, { 6.75f, 2.2f, -1.20f} },  // 옷장 (z=-2.0)
        { { 5.55f, 0.0f,  0.75f}, { 6.75f, 1.8f,  1.85f} },  // 와인 랙 (z=1.3)
        { { 5.55f, 0.0f, -0.80f}, { 6.75f, 2.0f,  0.30f} },  // 옷걸이 (z=-0.25)
        { { 5.30f, 0.0f, -5.50f}, { 6.75f, 2.2f, 2.60f} },  // 책장 (z=-5.5, 확장: 풀 모델 커버)
        { { 6.10f, 0.0f,  2.60f}, { 6.75f, 1.6f,  6.35f} },  // 트로피 선반

        // ── 뒷벽 (z≈-6.5) — 와인 캐비넷 개별 박스로 분리 (카페트 중앙 통로 확보)
        { {-1.70f, 0.0f, -6.75f}, { 0.70f, 2.0f, -5.90f} },  // wine cabinet L (cx=-0.2)
        { { 0.70f, 0.0f, -6.75f}, { 1.70f, 2.0f, -5.90f} },  // wine cabinet R (cx=+1.2)

        // ── 창문 4개 (back wall, z=-6.695, scale 0.01 → 1.4m wide x 1.7m tall)
        { {-5.80f, 0.0f, -6.75f}, {-4.20f, 1.75f, -6.10f} }, // window 1 (cx=-5.0)
        { {-3.30f, 0.0f, -6.75f}, {-1.70f, 1.75f, -6.10f} }, // window 2 (cx=-2.5)
        { { 1.70f, 0.0f, -6.75f}, { 3.30f, 1.75f, -6.10f} }, // window 3 (cx=+2.5)
        { { 4.20f, 0.0f, -6.75f}, { 5.80f, 1.75f, -6.10f} }, // window 4 (cx=+5.0)

        // ── 앞벽
        { {-0.95f, 0.0f,  6.15f}, { -0.05f, 2.5f, 6.75f} },  // 문 (x=0, z=6.8, scale 0.7)

        // ── 자동차 (x=-4.5, y=2.2, z=1.0, scale 0.7; Pontiac ~3.4m long, ~1.3m wide)
        { {-5.00f, 0.0f, 2.70f}, {-3.60f, 3.0f,  5.70f} },
    };

    // progress bar quad VAO
    unsigned int quadVAO = createQuadVAO();

    // sky texture for back glass wall
    unsigned int skyTex = LoadTexture("resources/textures/DaySky.jpg");

    // glass pane VAO — four quads matching the four window openings, interleaved pos(xyz) + uv(st)
    // z=-6.61: between Glass mesh front (-6.658) and frame front (-6.580)
    // cx positions: -5.0, -2.5, +2.5, +5.0  →  x ranges: ±0.70 around each cx
    float glassVerts[] = {
        // window 1 (cx=-5.0)
        -5.70f, 0.00f, -6.61f,  0.0f, 0.0f,
        -4.30f, 0.00f, -6.61f,  1.0f, 0.0f,
        -4.30f, 1.70f, -6.61f,  1.0f, 1.0f,
        -5.70f, 1.70f, -6.61f,  0.0f, 1.0f,
        // window 2 (cx=-2.5)
        -3.20f, 0.00f, -6.61f,  0.0f, 0.0f,
        -1.80f, 0.00f, -6.61f,  1.0f, 0.0f,
        -1.80f, 1.70f, -6.61f,  1.0f, 1.0f,
        -3.20f, 1.70f, -6.61f,  0.0f, 1.0f,
        // window 3 (cx=+2.5)
         1.80f, 0.00f, -6.61f,  0.0f, 0.0f,
         3.20f, 0.00f, -6.61f,  1.0f, 0.0f,
         3.20f, 1.70f, -6.61f,  1.0f, 1.0f,
         1.80f, 1.70f, -6.61f,  0.0f, 1.0f,
        // window 4 (cx=+5.0)
         4.30f, 0.00f, -6.61f,  0.0f, 0.0f,
         5.70f, 0.00f, -6.61f,  1.0f, 0.0f,
         5.70f, 1.70f, -6.61f,  1.0f, 1.0f,
         4.30f, 1.70f, -6.61f,  0.0f, 1.0f,
    };
    unsigned int glassIdx[] = {
         0, 1, 2,  0, 2, 3,
         4, 5, 6,  4, 6, 7,
         8, 9,10,  8,10,11,
        12,13,14, 12,14,15,
    };
    unsigned int glassVAO, glassVBO, glassEBO;
    glGenVertexArrays(1, &glassVAO);
    glGenBuffers(1, &glassVBO);
    glGenBuffers(1, &glassEBO);
    glBindVertexArray(glassVAO);
    glBindBuffer(GL_ARRAY_BUFFER, glassVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glassVerts), glassVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glassEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glassIdx), glassIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // sunlight patch VAO — 방 전체 바닥을 커버, 셰이더가 지수 감쇄로 자연스럽게 처리
    float sunVerts[] = {
        -6.75f, 0.01f, -6.74f,  0.0f, 0.0f,
         6.75f, 0.01f, -6.74f,  1.0f, 0.0f,
         6.75f, 0.01f,  6.60f,  1.0f, 1.0f,
        -6.75f, 0.01f,  6.60f,  0.0f, 1.0f,
    };
    unsigned int sunIdx[] = { 0,1,2, 0,2,3 };
    unsigned int sunVAO, sunVBO, sunEBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glGenBuffers(1, &sunEBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sunVerts), sunVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sunEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sunIdx), sunIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // directional light shadow matrix (static — light direction is fixed)
    // ------------------------------------------------------------------------
    glm::vec3 shadowLightDir   = glm::normalize(glm::vec3(0.10f, -0.50f, 0.85f));
    glm::vec3 shadowLightPos   = -shadowLightDir * 15.0f;
    glm::mat4 lightView        = glm::lookAt(shadowLightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProj        = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
    glm::mat4 lightSpaceMatrix = lightProj * lightView;

    // point light shadow matrices for ceiling light (static — position is fixed)
    // ------------------------------------------------------------------------
    glm::mat4 pointShadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, POINT_SHADOW_FAR);
    glm::vec3 pLightPos = ceilingLight.position;
    std::vector<glm::mat4> pointShadowTransforms = {
        pointShadowProj * glm::lookAt(pLightPos, pLightPos + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
        pointShadowProj * glm::lookAt(pLightPos, pLightPos + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        pointShadowProj * glm::lookAt(pLightPos, pLightPos + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
        pointShadowProj * glm::lookAt(pLightPos, pLightPos + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
        pointShadowProj * glm::lookAt(pLightPos, pLightPos + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
        pointShadowProj * glm::lookAt(pLightPos, pLightPos + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
    };

    // set static point shadow uniforms (position doesn't change between frames)
    officeShader.use();
    officeShader.setVec3("shadowPointPos", pLightPos);

    // hudShader: static uniforms (texture units + shadow uniforms that never change)
    hudShader.use();
    hudShader.setInt("shadowMap",        10);
    hudShader.setInt("shadowCubeMap",    11);
    hudShader.setFloat("shadowFarPlane", POINT_SHADOW_FAR);
    hudShader.setVec3("shadowPointPos",  pLightPos);
    hudShader.setFloat("shininess",      32.0f);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);
        ResolvePlayerCollision(camera.Position, PLAYER_RADIUS);
        camera.ApplyRoomBounds(ROOM_MIN, ROOM_MAX);

        // update window directional light from system clock
        DirLight dirLight = CalcTimeDirLight();

        // ray casting and hover timer update
        Ray pickRay    = MakePickingRay(camera);
        int hoveredIdx = UpdateInteractables(interactables, pickRay, 3.0f, deltaTime);

        // stencil outline setup
        // ------------------------------------------------------------------------
        struct OutlineDraw { Model* model; glm::mat4 mat; float thickness; };
        std::vector<OutlineDraw> outlineDraws;

        auto beginStencilWrite = [&]()
        {
            glEnable(GL_STENCIL_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilMask(0xFF);
        };
        auto endStencilWrite = [&]()
        {
            glStencilMask(0x00);
            glDisable(GL_STENCIL_TEST);
        };

        // L key: toggle lamp being looked at (edge-triggered)
        // ------------------------------------------------------------------------
        static bool lWasPressed = false;
        bool lNowPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
        if (lNowPressed && !lWasPressed)
        {
            if (hoveredIdx == lamp1InteractIdx) lampLight1.enabled = !lampLight1.enabled;
            if (hoveredIdx == lamp2InteractIdx) lampLight2.enabled = !lampLight2.enabled;
        }
        lWasPressed = lNowPressed;

        // draw all shadow casters — reused by both directional and point shadow passes
        // ------------------------------------------------------------------------
        auto DrawShadowCasters = [&](Shader& sh)
        {
            auto draw = [&](glm::mat4 m, Model& mdl)
            {
                sh.setMat4("model", m);
                mdl.Draw(sh);
            };

            for (const auto& obj : sceneObjects)
                DrawPrimitiveShadow(sh, obj);

            // desk
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
                m = glm::rotate(m, glm::radians(-270.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.2f));
                draw(m, deskModel2);
            }
            // floor lamps
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(1.56f, 0.0f, -3.50f));
                m = glm::scale(m, glm::vec3(2.0f));
                draw(m, lampModel);
            }
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-1.56f, 0.0f, -3.50f));
                m = glm::scale(m, glm::vec3(2.0f));
                draw(m, lampModel);
            }
            // filing cabinets
            {
                const float cabinetZ[] = { -1.20f, -1.80f, -2.40f, -3.00f };
                for (float tz : cabinetZ)
                {
                    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.48f, 0.311f, tz));
                    m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    m = glm::scale(m, glm::vec3(0.0025f));
                    draw(m, filingCabinetModel);
                }
            }
            // whiteboard
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.55f, 0.0f, 0.5f));
                m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.015f));
                draw(m, whiteboardModel);
            }
            // paintings
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.74f, 1.3f, 5.5f));
                m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                draw(m, paintingModel);
            }
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.74f, 1.3f, 2.5f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.5f));
                draw(m, lowpolyPaintingModel);
            }
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.74f, 0.8f, 4.0f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.7f));
                draw(m, psxPaintingModel);
            }
            // bookshelves
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-6.35f, 0.0f, -3.8f));
                m = glm::scale(m, glm::vec3(0.005f));
                draw(m, bookshelfModel);
            }
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.35f, 0.0f, -5.5f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.005f));
                draw(m, bookshelfModel);
            }
            // trophy shelves and trophies
            {
                const float colZ[] = { 6.0f, 4.5f, 3.0f };
                const float rowY[] = { 0.6f, 1.1f };
                for (float sz : colZ)
                    for (float sy : rowY)
                    {
                        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.68f, sy, sz));
                        m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                        m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                        m = glm::scale(m, glm::vec3(0.15f));
                        draw(m, kitchenShelfModel);
                    }
                for (float sy : rowY)
                {
                    {
                        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.6f, sy + 0.14f, 6.0f));
                        m = glm::scale(m, glm::vec3(0.066f));
                        draw(m, trophyModel);
                    }
                    {
                        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.5f, sy + 0.05f, 4.5f));
                        m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                        m = glm::scale(m, glm::vec3(0.044f));
                        draw(m, pawTrophyModel);
                    }
                    {
                        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.6f, sy + 0.01f, 3.0f));
                        m = glm::scale(m, glm::vec3(0.01f));
                        draw(m, worldCupTrophyModel);
                    }
                }
            }
            // wine rack
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.2f, 0.0f, 1.3f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.9f));
                draw(m, wineRackModel);
            }
            // hanger
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.2f, 0.0f, -0.25f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                draw(m, hangerModel);
            }
            // closet
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(6.35f, 0.0f, -2.0f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.5f));
                draw(m, closetModel);
            }
            // sofas
            {
                const float sofaZ[] = { -1.2f, 1.2f };
                for (float tz : sofaZ)
                {
                    glm::mat4 mL = glm::translate(glm::mat4(1.0f), glm::vec3(-2.000f, 0.0f, tz));
                    mL = glm::scale(mL, glm::vec3(0.55f));
                    draw(mL, sofaModel);
                    glm::mat4 mR = glm::translate(glm::mat4(1.0f), glm::vec3(2.000f, 0.0f, tz));
                    mR = glm::rotate(mR, glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    mR = glm::scale(mR, glm::vec3(0.55f));
                    draw(mR, sofaModel);
                }
            }
            // windows
            for (float cx : { -5.0f, -2.5f, 2.5f, 5.0f })
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(cx, -0.30f, -6.695f));
                m = glm::scale(m, glm::vec3(0.01f));
                draw(m, windowModel);
            }
            // wine cabinets
            for (float cx : { -0.20f, 1.20f })
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(cx, 0.5f, -6.5f));
                m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.009f));
                draw(m, wineCabinetModel);
            }
            // car
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(-4.5f, 2.2f, 1.0f));
                m = glm::scale(m, glm::vec3(0.7f));
                draw(m, carModel);
            }
            // door
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 6.8f));
                m = glm::rotate(m, glm::radians(-180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                m = glm::scale(m, glm::vec3(0.7f));
                draw(m, doorModel);
            }
            // ceiling fixture
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.848f, 0.0f));
                m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.01f));
                draw(m, ceilingFixtureModel);
            }
        };

        // directional shadow pass — depth from sun direction
        // ------------------------------------------------------------------------
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        shadowShader.use();
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        DrawShadowCasters(shadowShader);

        // point light shadow pass — depth cubemap for ceiling light
        // ------------------------------------------------------------------------
        glViewport(0, 0, POINT_SHADOW_SIZE, POINT_SHADOW_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        pointShadowShader.use();
        for (int i = 0; i < 6; ++i)
            pointShadowShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", pointShadowTransforms[i]);
        pointShadowShader.setVec3("lightPos",  ceilingLight.position);
        pointShadowShader.setFloat("farPlane", POINT_SHADOW_FAR);
        DrawShadowCasters(pointShadowShader);

        // 씬을 HDR FBO에 렌더링 (선형 RGBA16F, MSAA x4)
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        // render
        // ------
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                                (float)SCR_WIDTH / (float)SCR_HEIGHT,
                                                0.1f, 100.0f);

        // walking headbob — 2 footsteps/sec vertical, 1 sway/sec lateral
        // --------------------
        const float BOB_SPEED = 8.0f;   // rad/sec (약 2.5 footsteps/sec)
        const float BOB_AMP_Y = 0.055f; // 5.5cm 수직 bob
        const float BOB_AMP_X = 0.028f; // 2.8cm 좌우 흔들림

        if (isWalking)
            bobPhase += deltaTime * BOB_SPEED;
        float smoothT = 1.0f - expf(-deltaTime * 15.0f);
        float targetY = isWalking ? -fabsf(sinf(bobPhase)) * BOB_AMP_Y : 0.0f;
        float targetX = isWalking ?  sinf(bobPhase)         * BOB_AMP_X : 0.0f;
        bobOffY += (targetY - bobOffY) * smoothT;
        bobOffX += (targetX - bobOffX) * smoothT;

        glm::vec3 bobPos = camera.Position
                         + camera.Right                      * bobOffX
                         + glm::vec3(0.0f, 1.0f, 0.0f)      * bobOffY;
        glm::mat4 view = glm::lookAt(bobPos, bobPos + camera.Front, camera.Up);

        // ----- office scene -----
        officeShader.use();
        officeShader.setMat4("projection", projection);
        officeShader.setMat4("view",       view);
        officeShader.setVec3("viewPos",    camera.Position);
        officeShader.setFloat("shininess", 32.0f);
        officeShader.setFloat("uEmissiveStrength", 0.0f);   // default: no emission
        officeShader.setBool("uEmissiveAllSides",  false);  // default: downward-only (ceiling dome)
        SetPointLightUniforms(officeShader, ceilingLight);
        SetNamedPointLightUniforms(officeShader, lampLight1, "lampLight1");
        SetNamedPointLightUniforms(officeShader, lampLight2, "lampLight2");
        SetDirLightUniforms(officeShader,   dirLight);
        officeShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, shadowMap);
        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowMap);

        for (const auto& obj : sceneObjects)
            DrawPrimitive(officeShader, obj);

        {
            glm::mat4 deskMat = glm::mat4(1.0f);

            // 1. 위치 이동: 사장실 안쪽 벽면(Z축 방향 -3.0f) 중심에 배치
            // 만약 책상이 공중에 떠 있거나 바닥에 묻히면 Y값(0.0f)을 미세하게 조절해 보세요.
            deskMat = glm::translate(deskMat, glm::vec3(0.0f, 0.0f, -5.0f));

            // 2. 축 정렬: Blender(Z-up) 에셋을 OpenGL(Y-up) 좌표계에 맞추기 위해 X축 기준 -90도 회전
            deskMat = glm::rotate(deskMat, glm::radians(-270.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // 3. 방향 조정: 책상 정면이 사장실 입구를 바라보도록 필요시 Y축 회전 추가 가능
            // deskMat = glm::rotate(deskMat, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // 4. 스케일 조정: 블렌더 크기 단위를 고려해 0.01f로 대폭 축소 시작
            // 렌더링 후 너무 작으면 0.05f 등으로 키우고, 너무 크면 0.005f 등으로 줄이시면 됩니다.
            deskMat = glm::scale(deskMat, glm::vec3(0.2f));

            bool hovD = (hoveredIdx == deskInteractIdx);
            if (hovD) beginStencilWrite();
            officeShader.setMat4("model", deskMat);
            deskModel2.Draw(officeShader); // 새로 선언하신 deskModel2 변수명 적용
            if (hovD) { endStencilWrite(); outlineDraws.push_back({&deskModel2, deskMat, 0.030f / 0.2f}); }
        }

        // ----- floor lamp model (drawn twice for 2 positions) -----
        // lamp.obj: X center=-0.06, Y min=0, Z center=0.06  (meters, scale 1.0)
        // lamp 1: right of desk
        {
            officeShader.setVec3("uEmissiveColor",    glm::vec3(1.0f, 0.85f, 0.5f));
            officeShader.setFloat("uEmissiveStrength", lampLight1.enabled ? 1.0f : 0.0f);
            officeShader.setBool("uEmissiveAllSides",  true);
            glm::mat4 lampMat = glm::mat4(1.0f);
            lampMat = glm::translate(lampMat, glm::vec3(1.56f, 0.0f, -3.50f));
            lampMat = glm::scale(lampMat, glm::vec3(2.0f));
            bool hovL1 = (hoveredIdx == lamp1InteractIdx);
            if (hovL1) beginStencilWrite();
            officeShader.setMat4("model", lampMat);
            lampModel.Draw(officeShader);
            if (hovL1) { endStencilWrite(); outlineDraws.push_back({&lampModel, lampMat, 0.030f / 2.0f}); }
            officeShader.setFloat("uEmissiveStrength", 0.0f);
            officeShader.setBool("uEmissiveAllSides",  false);
        }
        // lamp 2: left of desk
        {
            officeShader.setVec3("uEmissiveColor",    glm::vec3(1.0f, 0.85f, 0.5f));
            officeShader.setFloat("uEmissiveStrength", lampLight2.enabled ? 1.0f : 0.0f);
            officeShader.setBool("uEmissiveAllSides",  true);
            glm::mat4 lampMat = glm::mat4(1.0f);
            lampMat = glm::translate(lampMat, glm::vec3(-1.56f, 0.0f, -3.50f));
            lampMat = glm::scale(lampMat, glm::vec3(2.0f));
            bool hovL2 = (hoveredIdx == lamp2InteractIdx);
            if (hovL2) beginStencilWrite();
            officeShader.setMat4("model", lampMat);
            lampModel.Draw(officeShader);
            if (hovL2) { endStencilWrite(); outlineDraws.push_back({&lampModel, lampMat, 0.030f / 2.0f}); }
            officeShader.setFloat("uEmissiveStrength", 0.0f);
            officeShader.setBool("uEmissiveAllSides",  false);
        }

        // ----- filing cabinet / 라커 (4 units, left wall) -----
        // order: 그림1-그림2-그림3-화이트보드-라커-책장 (front→back)
        // cabinets span z=-2.1~-4.5 (step 0.8m)
        {
            const float cabinetZ[] = { -1.20f, -1.80f, -2.40f, -3.00f };
            for (float tz : cabinetZ)
            {
                glm::mat4 cabinetMat = glm::mat4(1.0f);
                cabinetMat = glm::translate(cabinetMat, glm::vec3(-6.48f, 0.311f, tz));
                cabinetMat = glm::rotate(cabinetMat, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                cabinetMat = glm::scale(cabinetMat, glm::vec3(0.0025f));
                officeShader.setMat4("model", cabinetMat);
                filingCabinetModel.Draw(officeShader);
            }
        }

        // ----- whiteboard (left wall, z=-0.5) -----
        // order slot 4: 그림3(+1.5) → 화이트보드(-0.5) → 라커(-2.1~)
        {
            glm::mat4 wbMat = glm::mat4(1.0f);
            wbMat = glm::translate(wbMat, glm::vec3(-6.55f, 0.0f, 0.5f));
            wbMat = glm::rotate(wbMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            wbMat = glm::scale(wbMat, glm::vec3(0.015f));
            officeShader.setMat4("model", wbMat);
            whiteboardModel.Draw(officeShader);
        }


        // ----- painting (left wall, z=+5.5, between whiteboard and front wall) -----
        {
            glm::mat4 paintingMat = glm::mat4(1.0f);
            paintingMat = glm::translate(paintingMat, glm::vec3(-6.74f, 1.3f, 5.5f));
            paintingMat = glm::rotate(paintingMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            paintingMat = glm::scale(paintingMat, glm::vec3(1.0f));
            bool hov4 = (hoveredIdx == 4);
            if (hov4) beginStencilWrite();
            officeShader.setMat4("model", paintingMat);
            paintingModel.Draw(officeShader);
            if (hov4) { endStencilWrite(); outlineDraws.push_back({&paintingModel, paintingMat, 0.030f}); }
        }

        // ----- 그림3: lowpoly painting (left wall, z=+1.5) -----
        {
            glm::mat4 paintingMat = glm::mat4(1.0f);
            paintingMat = glm::translate(paintingMat, glm::vec3(-6.74f, 1.3f, 2.5f));
            paintingMat = glm::rotate(paintingMat, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            paintingMat = glm::scale(paintingMat, glm::vec3(0.5f));
            bool hov6 = (hoveredIdx == 6);
            if (hov6) beginStencilWrite();
            officeShader.setMat4("model", paintingMat);
            lowpolyPaintingModel.Draw(officeShader);
            if (hov6) { endStencilWrite(); outlineDraws.push_back({&lowpolyPaintingModel, paintingMat, 0.060f}); }
        }

        // ----- 그림2: psx painting (left wall, z=+3.5) -----
        {
            glm::mat4 paintingMat = glm::mat4(1.0f);
            paintingMat = glm::translate(paintingMat, glm::vec3(-6.74f, 0.8f, 4.0f));
            paintingMat = glm::rotate(paintingMat, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            paintingMat = glm::scale(paintingMat, glm::vec3(0.7f));
            bool hov5 = (hoveredIdx == 5);
            if (hov5) beginStencilWrite();
            officeShader.setMat4("model", paintingMat);
            psxPaintingModel.Draw(officeShader);
            if (hov5) { endStencilWrite(); outlineDraws.push_back({&psxPaintingModel, paintingMat, 0.043f}); }
        }

        // ----- 책장 (left wall, z=-5.8) -----
        {
            glm::mat4 bsMat = glm::mat4(1.0f);
            bsMat = glm::translate(bsMat, glm::vec3(-6.35f, 0.0f, -3.8f));
            bsMat = glm::scale(bsMat, glm::vec3(0.005f));
            officeShader.setMat4("model", bsMat);
            bookshelfModel.DrawExcluding(officeShader, {"book"});
        }

        // ----- trophy shelves: 2 rows x 3 cols (right wall) -----
        // col z: 6.0 / 4.5 / 3.0   row y: 0.6 / 1.1
        // col1=golden  col2=paw  col3=worldcup
        {
            const float colZ[] = { 6.0f, 4.5f, 3.0f };
            const float rowY[] = { 0.6f, 1.1f };

            // shelves
            for (float sz : colZ)
                for (float sy : rowY)
                {
                    glm::mat4 sMat = glm::mat4(1.0f);
                    sMat = glm::translate(sMat, glm::vec3(6.68f, sy, sz));
                    sMat = glm::rotate(sMat, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
                    sMat = glm::rotate(sMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    sMat = glm::scale(sMat, glm::vec3(0.15f));
                    officeShader.setMat4("model", sMat);
                    kitchenShelfModel.Draw(officeShader);
                }

            // col 1 (z=6.0): golden trophy
            {
                bool hov7 = (hoveredIdx == 7);
                if (hov7) beginStencilWrite();
                for (float sy : rowY)
                {
                    glm::mat4 tMat = glm::mat4(1.0f);
                    tMat = glm::translate(tMat, glm::vec3(6.6f, sy + 0.14f, 6.0f));
                    tMat = glm::scale(tMat, glm::vec3(0.066f));
                    officeShader.setMat4("model", tMat);
                    trophyModel.Draw(officeShader);
                    if (hov7) outlineDraws.push_back({&trophyModel, tMat, 0.030f / 0.066f});
                }
                if (hov7) endStencilWrite();
            }

            // col 2 (z=4.5): paw trophy
            {
                bool hov8 = (hoveredIdx == 8);
                if (hov8) beginStencilWrite();
                for (float sy : rowY)
                {
                    glm::mat4 tMat = glm::mat4(1.0f);
                    tMat = glm::translate(tMat, glm::vec3(6.5f, sy + 0.05f, 4.5f));
                    tMat = glm::rotate(tMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    tMat = glm::scale(tMat, glm::vec3(0.044f));
                    officeShader.setMat4("model", tMat);
                    pawTrophyModel.Draw(officeShader);
                    if (hov8) outlineDraws.push_back({&pawTrophyModel, tMat, 0.030f / 0.044f});
                }
                if (hov8) endStencilWrite();
            }

            // col 3 (z=3.0): world cup trophy
            {
                bool hov9 = (hoveredIdx == 9);
                if (hov9) beginStencilWrite();
                for (float sy : rowY)
                {
                    glm::mat4 tMat = glm::mat4(1.0f);
                    tMat = glm::translate(tMat, glm::vec3(6.6f, sy + 0.01f, 3.0f));
                    tMat = glm::scale(tMat, glm::vec3(0.01f));
                    officeShader.setMat4("model", tMat);
                    worldCupTrophyModel.Draw(officeShader);
                    if (hov9) outlineDraws.push_back({&worldCupTrophyModel, tMat, 0.030f / 0.01f});
                }
                if (hov9) endStencilWrite();
            }
        }

        // -----빈티지 캐비넷 (right wall, z=1.5) — 앞→뒤 슬롯3 -----
        {
            glm::mat4 wrMat = glm::mat4(1.0f);
            wrMat = glm::translate(wrMat, glm::vec3(6.2f, 0.0f, 1.3f));
            wrMat = glm::rotate(wrMat, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            wrMat = glm::rotate(wrMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            wrMat = glm::scale(wrMat, glm::vec3(0.9f));
            officeShader.setMat4("model", wrMat);
            wineRackModel.Draw(officeShader);
        }

        // ----- 옷걸이 (right wall, 프린터-옷장 사이 z=-0.25) -----
        {
            glm::mat4 hMat = glm::mat4(1.0f);
            hMat = glm::translate(hMat, glm::vec3(6.2f, 0.0f, -0.25f));
            hMat = glm::rotate(hMat, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            hMat = glm::rotate(hMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            hMat = glm::scale(hMat, glm::vec3(1.0f));
            officeShader.setMat4("model", hMat);
            hangerModel.Draw(officeShader);
        }

        // ----- 옷장 (right wall, z=-2.5) — 앞→뒤 슬롯4 -----
        {
            glm::mat4 cMat = glm::mat4(1.0f);
            cMat = glm::translate(cMat, glm::vec3(6.35f, 0.0f, -2.0f));
            cMat = glm::rotate(cMat, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            cMat = glm::rotate(cMat, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            cMat = glm::scale(cMat, glm::vec3(0.5f));
            officeShader.setMat4("model", cMat);
            closetModel.Draw(officeShader);
        }

        // ----- 책장 (right wall, z=-5.5) — 앞→뒤 슬롯5 -----
        {
            glm::mat4 bsMat = glm::mat4(1.0f);
            bsMat = glm::translate(bsMat, glm::vec3(6.35f, 0.0f, -5.5f));
            bsMat = glm::rotate(bsMat, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            bsMat = glm::scale(bsMat, glm::vec3(0.005f));
            officeShader.setMat4("model", bsMat);
            bookshelfModel.DrawExcluding(officeShader, {"book"});
        }


   

  
        // 소파 크기를 고려해 Z축 간격을 대칭으로 조정 (예: -1.2f, 1.2f)
        {
            const float sofaZ[] = { -1.2f, 1.2f };

            for (float tz : sofaZ)
            {
                // 1. 좌측 행 (Left side) — 우측(-X에서 +X 방향)을 바라보도록 Y축 90도 회전
                glm::mat4 sofaMatLeft = glm::mat4(1.0f);
                sofaMatLeft = glm::translate(sofaMatLeft, glm::vec3(-2.000f, 0.0f, tz)); // 소파 폭을 고려해 X축 외곽 이동(-1.17 -> -1.5)

                sofaMatLeft = glm::scale(sofaMatLeft, glm::vec3(0.55f)); // 기존에 잡으셨던 소파 스케일 0.5f 적용
                officeShader.setMat4("model", sofaMatLeft);
                sofaModel.Draw(officeShader);

                // 2. 우측 행 (Right side) — 좌측(+X에서 -X 방향)을 바라보도록 Y축 -90도(270도) 회전
                glm::mat4 sofaMatRight = glm::mat4(1.0f);
                sofaMatRight = glm::translate(sofaMatRight, glm::vec3(2.000f, 0.0f, tz));  // 소파 폭을 고려해 X축 외곽 이동(1.17 -> 1.5)
                sofaMatRight = glm::rotate(sofaMatRight, glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                sofaMatRight = glm::scale(sofaMatRight, glm::vec3(0.55f));
                officeShader.setMat4("model", sofaMatRight);
                sofaModel.Draw(officeShader);
            }
        }

        // ----- double-door windows (four, symmetric on back wall) -----
        // model: cm units, X:±70cm(140cm wide), Y:30~200cm(170cm tall), Z:~2.5cm thick
        // at scale 0.01: 1.40m wide, 1.70m tall, bottom at y=0.30m
        {
            for (float cx : { -5.0f, -2.5f, 2.5f, 5.0f })
            {
                glm::mat4 windowMat = glm::mat4(1.0f);
                windowMat = glm::translate(windowMat, glm::vec3(cx, -0.30f, -6.695f));
                windowMat = glm::scale(windowMat, glm::vec3(0.01f));
                officeShader.setMat4("model", windowMat);
                windowModel.Draw(officeShader);
            }
        }


        // ----- 와인 캐비닛 x2 (back wall, window2~3 사이 좌우 대칭) -----
        for (float cx : { -0.20f, 1.20f })
        {
            glm::mat4 wcMat = glm::mat4(1.0f);
            wcMat = glm::translate(wcMat, glm::vec3(cx, 0.5f, -6.5f));
            wcMat = glm::rotate(wcMat, glm::radians(180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            wcMat = glm::rotate(wcMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            wcMat = glm::rotate(wcMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            wcMat = glm::scale(wcMat, glm::vec3(0.009f));
            officeShader.setMat4("model", wcMat);
            wineCabinetModel.Draw(officeShader);
        }

        // ----- Pontiac Firebird (front wall, left of door) -----
        // preTransform bakes the Sketchfab root matrix, so vertices arrive in Y-up space.
        // Y offset compensates for the model origin being above the car body.
        {
            glm::mat4 carMat = glm::mat4(1.0f);
            carMat = glm::translate(carMat, glm::vec3(-4.5f, 2.2f, 1.0f));
            carMat = glm::scale(carMat, glm::vec3(0.7f));
            officeShader.setMat4("model", carMat);
            carModel.Draw(officeShader);
        }

        // ----- old wooden door (front wall center, z=+6.75) -----
        {
            glm::mat4 doorMat = glm::mat4(1.0f);
            doorMat = glm::translate(doorMat, glm::vec3(0.0f, 0.0f, 6.8f));
            doorMat = glm::rotate(doorMat, glm::radians(-180.0f), glm::vec3(1.0f, 1.0f, 0.0f));
            doorMat = glm::rotate(doorMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            doorMat = glm::rotate(doorMat, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
       
   
            doorMat = glm::scale(doorMat, glm::vec3(0.7f));
            officeShader.setMat4("model", doorMat);
            doorModel.Draw(officeShader);
        }

        // ----- ceiling fixture (mounted flush at y=2.25, Z-up→Y-up, scale 0.01) -----
        // model top (Z=40.19) → world y=2.25 (ceiling), bottom (Z=0.23) → world y≈1.85
        {
            // emissive: white globe areas glow bright when on, dark metal barely glows
            officeShader.setVec3("uEmissiveColor",    glm::vec3(1.0f, 1.0f, 0.95f));
            officeShader.setFloat("uEmissiveStrength", ceilingLight.enabled ? 1.5f : 0.0f);
            glm::mat4 fixtureMat = glm::mat4(1.0f);
            fixtureMat = glm::translate(fixtureMat, glm::vec3(0.0f, 1.848f, 0.0f));
            fixtureMat = glm::rotate(fixtureMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            fixtureMat = glm::scale(fixtureMat, glm::vec3(0.01f));
            officeShader.setMat4("model", fixtureMat);
            ceilingFixtureModel.Draw(officeShader);
            officeShader.setFloat("uEmissiveStrength", 0.0f);  // reset for subsequent draws
        }

        // ----- glass back wall (sky outside window) -----
        // --------------------
        glassShader.use();
        glassShader.setMat4("projection", projection);
        glassShader.setMat4("view",       view);
        glassShader.setVec3("viewPos",    camera.Position);
        glassShader.setBool("uIsSunPatch", false);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, skyTex);
        glassShader.setInt("uColorTex", 0);
        glBindVertexArray(glassVAO);
        glDrawElements(GL_TRIANGLES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // ----- sunlight patch on floor (analytic projection) -----
        // --------------------
        // 가산 블렌딩: 바닥 픽셀에 햇빛 에너지를 직접 더함 (HDR 파이프라인에 적합)
        glBlendFunc(GL_ONE, GL_ONE);
        glassShader.setBool("uIsSunPatch", true);
        glassShader.setVec4("uColor", glm::vec4(0.10f, 0.085f, 0.05f, 1.0f));
        glassShader.setVec3("uSunDir", glm::normalize(glm::vec3(0.10f, -0.50f, 0.85f)));
        glBindVertexArray(sunVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        // 일반 알파 블렌딩 복원
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ----- stencil outline pass -----
        // ------------------------------------------------------------------------
        if (!outlineDraws.empty())
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            glStencilMask(0x00);

            outlineShader.use();
            outlineShader.setMat4("projection", projection);
            outlineShader.setMat4("view",       view);
            outlineShader.setVec4("uOutlineColor", glm::vec4(1.0f, 0.85f, 0.15f, 1.0f));

            for (auto& od : outlineDraws)
            {
                outlineShader.setMat4("model",       od.mat);
                outlineShader.setFloat("uThickness", od.thickness);
                od.model->Draw(outlineShader);
            }

            glStencilMask(0xFF);
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            glDisable(GL_STENCIL_TEST);
        }

        // ----- MSAA resolve + HDR 톤매핑 패스 -----
        // --------------------
        // 멀티샘플 hdrFBO → 단일샘플 hdrResolveFBO로 blit
        glBindFramebuffer(GL_READ_FRAMEBUFFER, hdrFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrResolveFBO);
        glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT,
                          0, 0, SCR_WIDTH, SCR_HEIGHT,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // 기본 프레임버퍼에 Reinhard 톤매핑 + 감마 인코딩 적용
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        hdrShader.setInt("hdrBuffer",  0);
        hdrShader.setFloat("exposure", 1.0f);
        glBindVertexArray(screenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);

        // ----- HUD moon stick -----
        // clear depth so stick always renders on top
        glClear(GL_DEPTH_BUFFER_BIT);

        float wobblePitch =  mouseDeltaY * 0.008f;
        float wobbleYaw   = -mouseDeltaX * 0.008f;
        mouseDeltaX *= 0.80f;
        mouseDeltaY *= 0.80f;

        glm::mat4 hudProj = glm::perspective(glm::radians(60.0f),
                                             (float)SCR_WIDTH / (float)SCR_HEIGHT,
                                             0.01f, 10.0f);

        // moon stick: Z-axis model (62~108 cm), centered then oriented upward
        // transform applied innermost-first:
        //   1. center Z origin (move Z midpoint to 0)
        //   2. rotate -90° X  → Z becomes +Y (stick points up)
        //   3. scale 0.01     → cm to meters
        //   4. slight rightward tilt (Z rotation)
        //   5. mouse wobble
        //   6. HUD position (lower-center, ornament visible, grip below frame)
        glm::mat4 stickMat = glm::mat4(1.0f);
        stickMat = glm::translate(stickMat, glm::vec3(0.15f, -0.45f, -1.0f));
        stickMat = glm::rotate(stickMat, wobblePitch, glm::vec3(1.0f, 0.0f, 0.0f));
        stickMat = glm::rotate(stickMat, wobbleYaw,   glm::vec3(0.0f, 1.0f, 0.0f));
        stickMat = glm::rotate(stickMat, glm::radians(12.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        stickMat = glm::scale(stickMat, glm::vec3(0.01f));
        stickMat = glm::rotate(stickMat, glm::radians(-150.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        stickMat = glm::translate(stickMat, glm::vec3(0.0f, 0.0f, -85.53f));

        // shadow maps persist on units 10/11 from the main pass, but rebind to be safe
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, shadowMap);
        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowMap);

        hudShader.use();
        hudShader.setMat4("hudProjection",  hudProj);
        hudShader.setMat4("hudModel",       stickMat);
        hudShader.setMat4("hudInvView",     glm::inverse(view));
        hudShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        hudShader.setVec3("viewPos",        camera.Position);
        SetPointLightUniforms(hudShader,      ceilingLight);
        SetNamedPointLightUniforms(hudShader, lampLight1, "lampLight1");
        SetNamedPointLightUniforms(hudShader, lampLight2, "lampLight2");
        SetDirLightUniforms(hudShader,        dirLight);
        moonStickModel.Draw(hudShader);

        // ----- progress bar UI -----
        if (hoveredIdx >= 0 && !interactables[hoveredIdx].url.empty())
        {
            float progress = interactables[hoveredIdx].hoverTimer / HOVER_REQUIRED;

            glDisable(GL_DEPTH_TEST);
            uiShader.use();
            glBindVertexArray(quadVAO);

            // background track
            uiShader.setVec2("uOffset", glm::vec2(-0.9f, -0.97f));
            uiShader.setVec2("uSize",   glm::vec2(1.8f, 0.03f));
            uiShader.setVec4("uColor",  glm::vec4(0.15f, 0.15f, 0.15f, 0.8f));
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // fill
            uiShader.setVec2("uOffset", glm::vec2(-0.9f, -0.97f));
            uiShader.setVec2("uSize",   glm::vec2(1.8f * progress, 0.03f));
            uiShader.setVec4("uColor",  glm::vec4(0.2f, 0.85f, 0.45f, 1.0f));
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }

        // ----- crosshair -----
        // drawn every frame at NDC center (0,0); dark outline + white fill
        {
            glDisable(GL_DEPTH_TEST);
            uiShader.use();
            glBindVertexArray(quadVAO);

            // shadow (dark, slightly larger)
            uiShader.setVec4("uColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));
            uiShader.setVec2("uOffset", glm::vec2(-0.018f, -0.004f));   // horizontal
            uiShader.setVec2("uSize",   glm::vec2( 0.036f,  0.008f));
            glDrawArrays(GL_TRIANGLES, 0, 6);
            uiShader.setVec2("uOffset", glm::vec2(-0.002f, -0.030f));   // vertical
            uiShader.setVec2("uSize",   glm::vec2( 0.004f,  0.060f));
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // white fill
            uiShader.setVec4("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.90f));
            uiShader.setVec2("uOffset", glm::vec2(-0.015f, -0.003f));   // horizontal
            uiShader.setVec2("uSize",   glm::vec2( 0.030f,  0.006f));
            glDrawArrays(GL_TRIANGLES, 0, 6);
            uiShader.setVec2("uOffset", glm::vec2(-0.0015f, -0.027f));  // vertical
            uiShader.setVec2("uSize",   glm::vec2( 0.0030f,  0.054f));
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

}

// normalized 0~1 quad for the progress bar
// -----------------------------------------
unsigned int createQuadVAO()
{
    float quadVerts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glBindVertexArray(0);
    return VAO;
}

// push player out of any overlapping furniture AABB (XZ only)
// -----------------------------------------
void ResolvePlayerCollision(glm::vec3& pos, float radius)
{
    for (const AABB& box : gCollisionBoxes)
    {
        float eMinX = box.min.x - radius, eMaxX = box.max.x + radius;
        float eMinZ = box.min.z - radius, eMaxZ = box.max.z + radius;

        if (pos.x <= eMinX || pos.x >= eMaxX || pos.z <= eMinZ || pos.z >= eMaxZ)
            continue;

        // penetration on each of the four XZ faces
        float dL = pos.x - eMinX;  // push -X
        float dR = eMaxX - pos.x;  // push +X
        float dN = pos.z - eMinZ;  // push -Z
        float dF = eMaxZ - pos.z;  // push +Z

        float minPen = dL;
        float resolveX = -dL, resolveZ = 0.0f;
        if (dR < minPen) { minPen = dR; resolveX =  dR; resolveZ = 0.0f; }
        if (dN < minPen) { minPen = dN; resolveX = 0.0f; resolveZ = -dN; }
        if (dF < minPen) {              resolveX = 0.0f; resolveZ =  dF; }

        pos.x += resolveX;
        pos.z += resolveZ;
    }
}

// process all input
// -----------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    isWalking = false;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.ProcessKeyboard(FORWARD,  deltaTime); isWalking = true; }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.ProcessKeyboard(BACKWARD, deltaTime); isWalking = true; }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.ProcessKeyboard(LEFT,     deltaTime); isWalking = true; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.ProcessKeyboard(RIGHT,    deltaTime); isWalking = true; }

    // F key: toggle ceiling point light (edge-triggered)
    static bool fWasPressed = false;
    bool fNowPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fNowPressed && !fWasPressed)
        ceilingLight.enabled = !ceilingLight.enabled;
    fWasPressed = fNowPressed;
}

// glfw callbacks
// -----------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;  // reversed: y-coordinates go bottom to top

    lastX = xpos;
    lastY = ypos;

    mouseDeltaX += xoffset;
    mouseDeltaY += yoffset;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

} // namespace Office
