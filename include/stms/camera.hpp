//
// Created by grant on 6/5/20.
//

#pragma once

#ifndef __STONEMASON_CAMERA_HPP
#define __STONEMASON_CAMERA_HPP

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

namespace stms {
    constexpr glm::mat4 idMat = glm::mat4(1.0f);

    glm::vec3 quatTrans(const glm::vec3 &in, const glm::quat &quat);

    struct TransformInfo {
        glm::quat rot;
        glm::vec3 pos;
        glm::vec3 scale;

        glm::mat4 mat4;

        glm::vec3 forward = {0, 0, 1};
        glm::vec3 up = {0, 1, 0};
        glm::vec3 right = {1, 0, 0};

        inline void setEuler(const glm::vec3 &euler) {
            rot = glm::quat(euler); // x=pitch, y=yaw, z=roll
        }

        glm::mat4 buildMatM();
        void buildVecs();

        inline void buildAll() {
            buildMatM();
            buildVecs();
        }
    };

    class Camera {
    public:
        glm::vec3 pos = {0.0f, 0.0f, 0.0f};
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
        glm::quat rot = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

        glm::mat4 matV = glm::mat4(1.0f);
        glm::mat4 matP = glm::mat4(1.0f);

        glm::mat4 matM = glm::mat4(1.0f);

        glm::vec3 forward = {0, 0, 1};
        glm::vec3 up = {0, 1, 0};
        glm::vec3 right = {1, 0, 0};

        float fov = 70;

        Camera() = default;
        virtual ~Camera() = default;

        inline void setFov(const float &angle) {
            fov = angle;
        }

        inline void setEuler(const glm::vec3 &euler) {
            rot = glm::quat(euler);
        }

        inline void lookAt(const glm::vec3 &target, const glm::vec3 &lookUp = {0, 1, 0}) {
            lookDir(target - pos, lookUp);
        };

        inline void lookDir(const glm::vec3 &direction, const glm::vec3 &lookUp = {0, 1, 0}) {
            rot = glm::quatLookAt(glm::normalize(direction), lookUp);
        }

        /**
         * Assume that `direction` is already normalized and skip the normalization process. Small optimization.
         */
        inline void lookDirNorm(const glm::vec3 &direction, const glm::vec3 &lookUp = {0, 1, 0}) {
            rot = glm::quatLookAt(direction, lookUp);
        }

        glm::mat4 buildPersp(const float &aspect, const float &near = 0.1f, const float &far = 100.0f);
        glm::mat4 buildOrtho(const float &left, const float &oRight, const float &bottom, const float &top, const float &near = 1.0f, const float &far = 100.0f);
        glm::mat4 buildMatV();
        glm::mat4 buildMatM();
        void buildVecs();

        inline void buildAll() {
            buildMatV();
            buildMatM();
            buildVecs();
        }
    };
}

#endif //__STONEMASON_CAMERA_HPP
