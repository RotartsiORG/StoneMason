//
// Created by grant on 6/5/20.
//

#include <stms/camera.hpp>

namespace stms {
    glm::mat4 &TransformInfo::buildMatM() {
        // Hopefully this is the right order. We want scale, rotate, then translate.
        mat4 = glm::translate(pos) * glm::toMat4(rot) * glm::scale(scale);

        return mat4;
    }

    void TransformInfo::buildVecs() {
        right = transByQuat(glm::vec4(1, 0, 0, 1), rot);
        up = transByQuat(glm::vec4(0, 1, 0, 1), rot);
        forward = transByQuat(glm::vec4(0, 0, -1, 1), rot);
    }

    glm::mat4 &Camera::buildPersp(const float &aspect, const float &near, const float &far) {
        matP = glm::perspective(fov, aspect, near, far);
        return matP;
    }

    glm::mat4 &Camera::buildOrtho(const float &left, const float &oRight, const float &bottom, const float &top, const float &near, const float &far) {
        matP = glm::ortho(left, oRight, bottom, top, near, far);
        return matP;
    }

    glm::mat4 &Camera::buildMatV() {
        trans.buildMatM();
        matV = glm::inverse(trans.mat4);

        return matV;
    }

    glm::vec3 transByQuat(const glm::vec3 &in, const glm::quat &quat) {
        glm::vec3 u{quat.x, quat.y, quat.z};

        // https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
        // This is just magic to me. Screw linear algebra. I've already watched too many 3blue1brown videos
        return 2.0f * glm::dot(u, in) * u +
               (quat.w * quat.w - glm::dot(u, u)) * in +
               2.0f * quat.w * glm::cross(u, in);
    }
}
