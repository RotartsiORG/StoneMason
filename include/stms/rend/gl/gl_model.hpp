//
// Created by grant on 7/4/20.
//

#pragma once

#ifndef __STONEMASON_GL_MODEL_HPP
#define __STONEMASON_GL_MODEL_HPP

#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL


#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "stms/rend/gl/gl_buffers.hpp"
#include "gl_shaders.hpp"
#include <unordered_set>
#include <string>

namespace stms {
    typedef Assimp::Importer GLModelImporter;
    typedef aiPostProcessSteps GLPostProcFX;

    constexpr unsigned postProcOptimal = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes;

    class GLScene {
    private:
        const aiScene *scene = nullptr;

        struct Mesh {
            GLVertexBuffer vbo;
            GLVertexArray vao; // should this be part of the scene/static? probably.
            GLIndexBuffer ibo;
        };

        struct Node {
            std::string name;
            glm::mat4 transform;
            std::vector<unsigned> meshIndices;
            std::vector<Node> children;

            void draw(); // draws this node and it's children.
        };

        std::vector<Mesh> meshes;
        //std::unordered_map<std::string, GLTexture> textures;

        Node root;

    public:
        GLScene(GLModelImporter *imp, const std::string &path, unsigned postProc);

        void draw();
    };
}

#endif

#endif //__STONEMASON_GL_MODEL_HPP
