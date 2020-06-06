//
// Created by grant on 6/5/20.
//

#include <stms/camera.hpp>

namespace stms {
    glm::mat4 TransformInfo::buildMatM() {
        // Hopefully this is the right order. We want scale, rotate, then translate.
        mat4 = glm::translate(idMat, pos) * glm::toMat4(rot) * glm::scale(idMat, scale);

        return mat4;
    }

    void TransformInfo::buildVecs() {
        right = quatTrans(glm::vec4(1, 0, 0, 1), rot);
        up = quatTrans(glm::vec4(0, 1, 0, 1), rot);
        forward = quatTrans(glm::vec4(0, 0, 1, 1), rot);
    }

    glm::mat4 Camera::buildPersp(const float &aspect, const float &near, const float &far) {
        matP = glm::perspective(fov, aspect, near, far);
        return matP;
    }

    glm::mat4 Camera::buildOrtho(const float &left, const float &oRight, const float &bottom, const float &top, const float &near, const float &far) {
        matP = glm::ortho(left, oRight, bottom, top, near, far);
        return matP;
    }

    glm::mat4 Camera::buildMatV() {
        // Invert rot (hopefully this is right). Translate by -pos
        // and scale by 1.0/scale. We apply scaling, translation, then rotation.

        matV = glm::toMat4(glm::inverse(rot)) * glm::translate(idMat, glm::vec3(0.0f, 0.0f, 0.0f) - pos)
                * glm::scale(idMat, glm::vec3(1.0f, 1.0f, 1.0f) / scale);

        return matV;
    }

    glm::mat4 Camera::buildMatM() {
        matM = glm::translate(idMat, pos) * glm::toMat4(rot) * glm::scale(idMat, scale);
        return matM;
    }

    void Camera::buildVecs() {
        right = quatTrans(glm::vec4(1, 0, 0, 1), rot);
        up = quatTrans(glm::vec4(0, 1, 0, 1), rot);
        forward = quatTrans(glm::vec4(0, 0, 1, 1), rot);
    }


    glm::vec3 quatTrans(const glm::vec3 &in, const glm::quat &quat) {
        glm::vec3 u{quat.x, quat.y, quat.z};

        // https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
        return 2.0f * glm::dot(u, in) * u +
               (quat.w * quat.w - glm::dot(u, u)) * in +
               2.0f * quat.w * glm::cross(u, in);
    }
}
