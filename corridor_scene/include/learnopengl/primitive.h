#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include <glad/glad.h>
// stb_image.h is NOT included here to avoid duplicate implementation.
// The including .cpp must define STB_IMAGE_IMPLEMENTATION and include stb_image.h before this header.
#include <iostream>

struct PrimMesh
{
    unsigned int VAO;
    int          indexCount;
};

// Flat XZ plane centered at origin, normal pointing +Y.
// texRepeatU/V tiles the texture independently per axis — use the same value for square planes,
// or set U = width/tileSize and V = depth/tileSize to avoid stretching on rectangular planes.
PrimMesh CreatePlane(float width, float depth, float texRepeatU = 1.0f, float texRepeatV = 0.0f)
{
    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    float tu = texRepeatU;
    float tv = (texRepeatV > 0.0f) ? texRepeatV : texRepeatU;  // default: same as U (square)

    float verts[] = {
        -hw, 0.0f, -hd,  0.0f,1.0f,0.0f,  0.0f, tv,
         hw, 0.0f, -hd,  0.0f,1.0f,0.0f,  tu,   tv,
         hw, 0.0f,  hd,  0.0f,1.0f,0.0f,  tu,   0.0f,
        -hw, 0.0f,  hd,  0.0f,1.0f,0.0f,  0.0f, 0.0f
    };
    unsigned int indices[] = { 0,1,2, 0,2,3 };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glBindVertexArray(0);

    return { VAO, 6 };
}

// Box centered at origin, all 6 faces with correct outward normals.
PrimMesh CreateBox(float w, float h, float d)
{
    float hw = w*0.5f, hh = h*0.5f, hd = d*0.5f;

    float verts[] = {
        // front (+Z)
        -hw,-hh, hd,  0,0,1,  0,0,
         hw,-hh, hd,  0,0,1,  1,0,
         hw, hh, hd,  0,0,1,  1,1,
        -hw, hh, hd,  0,0,1,  0,1,
        // back (-Z)
         hw,-hh,-hd,  0,0,-1,  0,0,
        -hw,-hh,-hd,  0,0,-1,  1,0,
        -hw, hh,-hd,  0,0,-1,  1,1,
         hw, hh,-hd,  0,0,-1,  0,1,
        // left (-X)
        -hw,-hh,-hd,  -1,0,0,  0,0,
        -hw,-hh, hd,  -1,0,0,  1,0,
        -hw, hh, hd,  -1,0,0,  1,1,
        -hw, hh,-hd,  -1,0,0,  0,1,
        // right (+X)
         hw,-hh, hd,  1,0,0,  0,0,
         hw,-hh,-hd,  1,0,0,  1,0,
         hw, hh,-hd,  1,0,0,  1,1,
         hw, hh, hd,  1,0,0,  0,1,
        // top (+Y)
        -hw, hh, hd,  0,1,0,  0,0,
         hw, hh, hd,  0,1,0,  1,0,
         hw, hh,-hd,  0,1,0,  1,1,
        -hw, hh,-hd,  0,1,0,  0,1,
        // bottom (-Y)
        -hw,-hh,-hd,  0,-1,0,  0,0,
         hw,-hh,-hd,  0,-1,0,  1,0,
         hw,-hh, hd,  0,-1,0,  1,1,
        -hw,-hh, hd,  0,-1,0,  0,1,
    };
    unsigned int indices[] = {
         0, 1, 2,  0, 2, 3,
         4, 5, 6,  4, 6, 7,
         8, 9,10,  8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glBindVertexArray(0);

    return { VAO, 36 };
}

// 1x1 solid-color texture, no file needed.
unsigned int SolidTexture(unsigned char r, unsigned char g, unsigned char b)
{
    unsigned int texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    unsigned char data[3] = { r, g, b };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texID;
}

// Load texture from file. Requires STB_IMAGE_IMPLEMENTATION in exactly one .cpp.
unsigned int LoadTexture(const char* path)
{
    unsigned int texID;
    glGenTextures(1, &texID);
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = nrChannels == 4 ? GL_RGBA : (nrChannels == 3 ? GL_RGB : GL_RED);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // grayscale: replicate R to G and B so the texture renders gray, not red
        if (nrChannels == 1)
        {
            GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
        }
        stbi_image_free(data);
    }
    else
    {
        std::cout << "ERROR::TEXTURE: Failed to load " << path << std::endl;
        stbi_image_free(data);
    }
    return texID;
}

#endif
