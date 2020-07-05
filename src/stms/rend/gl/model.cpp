//
// Created by grant on 7/4/20.
//

#include "stms/config.hpp"
#ifdef STMS_ENABLE_OPENGL

#include "stms/rend/gl/model.hpp"

#include <stms/logging.hpp>

namespace stms {

    GLScene::GLScene(GLModelImporter *imp, const std::string &path, unsigned postProc) :
        scene(imp->ReadFile(path, postProc)) {
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            STMS_WARN("Failed to import model {}: {}", path, imp->GetErrorString());
            return;
        }
    }
}

#endif