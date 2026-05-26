#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <learnopengl/mesh.h>
#include <learnopengl/shader.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

class Model 
{
public:
    // model data 
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;

    // constructor, expects a filepath to a 3D model.
    // preTransform=true bakes node hierarchy transforms into vertices (needed for
    // Sketchfab GLBs that embed axis-conversion in the root node matrix).
    Model(string const &path, bool gamma = false, bool preTransform = false) : gammaCorrection(gamma)
    {
        loadModel(path, preTransform);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader &shader)
    {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    // draws only meshes whose name does NOT contain any of the given substrings (case-insensitive)
    void DrawExcluding(Shader &shader, const vector<string>& excludeKeywords)
    {
        for (auto& mesh : meshes)
        {
            string lowerName = mesh.name;
            for (char& c : lowerName) c = (char)tolower(c);
            bool skip = false;
            for (const auto& kw : excludeKeywords)
            {
                string lowerKw = kw;
                for (char& c : lowerKw) c = (char)tolower(c);
                if (lowerName.find(lowerKw) != string::npos) { skip = true; break; }
            }
            if (!skip) mesh.Draw(shader);
        }
    }
    
private:
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path, bool preTransform = false)
    {
        // GLB/GLTF UV (0,0) is top-left; stbi_set_flip_vertically_on_load already corrects this.
        // Applying aiProcess_FlipUVs on top would double-flip and misplace textures.
        // OBJ UV (0,0) is bottom-left, so it needs aiProcess_FlipUVs to match OpenGL convention.
        bool isGltf = path.size() >= 4 &&
                      (path.compare(path.size() - 4, 4, ".glb")  == 0 ||
                       path.compare(path.size() - 5, 5, ".gltf") == 0);
        // GLTF/GLB already stores pre-computed normals and tangents — avoid any
        // post-processing that rewrites vertex data and may produce NaN on flat quads.
        unsigned int importFlags = aiProcess_Triangulate;
        if (!isGltf) importFlags |= aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;
        // preTransform: bake node hierarchy into vertices so models with non-identity
        // root matrices (e.g. Sketchfab axis-conversion) appear in correct orientation.
        if (preTransform) importFlags |= aiProcess_PreTransformVertices;

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, importFlags);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene)
    {
        // process each mesh located at the current node
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene.
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }

    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each of the mesh's vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                // tangent — mTangents may be null if CalcTangentSpace failed on degenerate geometry
                if (mesh->mTangents)
                {
                    vector.x = mesh->mTangents[i].x;
                    vector.y = mesh->mTangents[i].y;
                    vector.z = mesh->mTangents[i].z;
                    vertex.Tangent = vector;
                }
                if (mesh->mBitangents)
                {
                    vector.x = mesh->mBitangents[i].x;
                    vector.y = mesh->mBitangents[i].y;
                    vector.z = mesh->mBitangents[i].z;
                    vertex.Bitangent = vector;
                }
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);        
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. diffuse maps (OBJ/legacy) + GLTF2 baseColor fallback
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        if (diffuseMaps.empty())
            diffuseMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", scene);
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps — HEIGHT for OBJ/MTL bump, NORMALS for glTF/FBX
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
        if (normalMaps.empty())
            normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height", scene);
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        
        // read MTL Kd color as fallback when no diffuse texture exists
        aiColor3D kdColor(1.0f, 1.0f, 1.0f);
        material->Get(AI_MATKEY_COLOR_DIFFUSE, kdColor);
        glm::vec3 diffuseColor(kdColor.r, kdColor.g, kdColor.b);

        // GLTF glass: alphaMode=BLEND + baseColor=(0,0,0) → replace with light sky blue
        float opacity = 1.0f;
        material->Get(AI_MATKEY_OPACITY, opacity);
        if (opacity < 0.5f && kdColor.r < 0.1f && kdColor.g < 0.1f && kdColor.b < 0.1f)
            diffuseColor = glm::vec3(0.72f, 0.88f, 0.98f);

        // return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures, diffuseColor, string(mesh->mName.C_Str()));
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName, const aiScene* scene)
    {
        vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }
            if(!skip)
            {
                Texture texture;
                // GLB embedded texture: path is "*N" where N is the index into scene->mTextures
                if(str.C_Str()[0] == '*')
                {
                    int idx = atoi(str.C_Str() + 1);
                    if (idx < 0 || (unsigned int)idx >= scene->mNumTextures) continue;
                    texture.id = TextureFromEmbedded(scene->mTextures[idx]);
                }
                else
                {
                    texture.id = TextureFromFile(str.C_Str(), this->directory);
                }
                if (texture.id == 0) continue;  // load failed — fall back to Kd color
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }

    unsigned int TextureFromEmbedded(const aiTexture* tex)
    {
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char* data;
        bool isCompressed = (tex->mHeight == 0);

        if(isCompressed)
            data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(tex->pcData),
                                         tex->mWidth, &width, &height, &nrComponents, 0);
        else
        {
            width       = tex->mWidth;
            height      = tex->mHeight;
            nrComponents = 4;
            data        = reinterpret_cast<unsigned char*>(tex->pcData);
        }

        if(data)
        {
            GLenum format = GL_RGBA;
            if(nrComponents == 1) format = GL_RED;
            else if(nrComponents == 3) format = GL_RGB;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            if(isCompressed) stbi_image_free(data);
        }
        else
        {
            std::cout << "ERROR: embedded texture failed to load" << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }

        return textureID;
    }
};


inline unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGBA;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;  // signal failure — caller must not add to textures vector
    }

    return textureID;
}
#endif
