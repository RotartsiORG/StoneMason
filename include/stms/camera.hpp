/**
 * @file stms/camera.hpp
 * @brief File providing functionality for easily manipulating and constructing model, view, and projection matrices.
 * Created by grant on 6/5/20.
 */

#pragma once

#ifndef __STONEMASON_CAMERA_HPP
#define __STONEMASON_CAMERA_HPP
//!< Include guard

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

namespace stms {
    /**
     * @brief Identity 4x4 matrix
     *        {{1, 0, 0, 0},
     *         {0, 1, 0, 0},
     *         {0, 0, 1, 0},
     *         {0, 0, 0, 1}}
     */
    constexpr glm::mat4 idMat = glm::mat4(1.0f);

    /**
     * @brief Transform a `glm::vec3` by some `glm::quat`.
     * @param in Vector to transform
     * @param quat Quaternion to transform it by
     * @return The transformed Vector.
     */
    glm::vec3 transByQuat(const glm::vec3 &in, const glm::quat &quat);

    /// Struct of components that can be used to describe and build a linear transformation (Model matrix)
    struct TransformInfo {
        glm::quat rot = glm::quat(glm::vec3{0, 0, 0}); //!< Rotation component of the transformation
        glm::vec3 pos = glm::vec3{0, 0, 0}; //!< Translation component of the transformation
        glm::vec3 scale = glm::vec3{1, 1, 1}; //!< Scale component of the transformation

        glm::mat4 mat4; //!< Final transformation matrix describing the transform.

        /// Forward vector describing the rotation. Useful for moving an object "forward" or "backward"
        glm::vec3 forward = {0, 0, -1};
        /// Up vector describing the rotation. Useful for moving an object "up" or "down"
        glm::vec3 up = {0, 1, 0};
        /// Right vector describing the rotation. Useful for moving an object "right" or "left"
        glm::vec3 right = {1, 0, 0};

        /**
         * @brief Set the `stms::TransformInfo::rot` component to a `glm::vec3` euler angle.
         * @param euler A vector containing {pitch, yaw, roll}
         */
        inline void setEuler(const glm::vec3 &euler) {
            rot = glm::quat(euler); // x=pitch, y=yaw, z=roll
        }

        /**
         * @brief Rotate to look in a certain direction represented by a *normalized* vec3
         * @param normDirection NORMALIZED direction vector (use `glm::normalize` to normalize)
         * @param lookUp Up vector
         */
        inline void lookDir(const glm::vec3 &normDirection, const glm::vec3 &lookUp = {0, 1, 0}) {
            rot = glm::quatLookAt(normDirection, lookUp);
        }

        /**
         * @brief Construct a 4x4 matrix based on `stms::TransformInfo::rot`, `stms::TransformInfo::pos`,
         *        and `stms::TransformInfo::scale` and store the result in `stms::TransformInfo::mat4`.
         * @return The matrix that was constructed
         */
        glm::mat4 &buildMatM();
        /**
         * @brief Construct the vectors `stms::TransformInfo::forward`, `stms::TransformInfo::up`, and
         *        `stms::TransformInfo::right` based on `stms::TransformInfo::rot`, `stms::TransformInfo::pos`,
         *        and `stms::TransformInfo::scale`.
         */
        void buildVecs();

        /// Shorthand for `buildMatM()` then `buildVecs()`
        inline void buildAll() {
            buildMatM();
            buildVecs();
        }
    };

    /// Class for manipulating and constructing view and projection matrices. Similar to `TransformInfo`.
    class Camera {
    public:
        TransformInfo trans{}; //!< Transformations of the camera

        glm::mat4 matV = glm::mat4(1.0f); //!< Constructed view matrix of the camera
        glm::mat4 matP = glm::mat4(1.0f); //!< Constructed projection matrix of the camera

        float fov = 70; //!< Field of view of the camera in degrees

        Camera() = default; //!< Default constructor
        virtual ~Camera() = default; //!< Default virtual destructor

        /**
         * @brief Set a new field of view for the camera (`stms::Camera::fov`)
         * @param angle New FOV
         */
        inline void setFov(const float &angle) {
            fov = angle;
        }

        /**
         * @brief Build a perspective projection matrix and store it in `matP`. See GLM documentation for
         *        `glm::perspective`.
         * @param aspect Aspect ratio of the window. Equal to the window's width divided by its height
         * @param near Near clipping plane. Any fragments closer than this to the camera would be discarded.
         *             Ideally you want this to be as big as possible.
         * @param far Far clipping plane. Any fragments further away than this from the camera would be discarded.
         *            Ideally you want this to be as small as possible.
         * @return Reference to the matrix that was constructed
         */
        glm::mat4 &buildPersp(const float &aspect, const float &near = 0.1f, const float &far = 100.0f);

        /**
         * @brief Build an orthographic projection matrix and store it in `matP`. See GLM documentation for `glm::ortho`
         * @param left The X coordinate that corresponds to the very left of the screen.
         * @param oRight The X coordinate that corresponds to the very right of the screen.
         * @param bottom The Y coordinate that corresponds to the bottom of the screen.
         * @param top The Y coordinate that corresponds to the top of the screen.
         * @param near Near clipping plane. Any fragments closer than this to the camera would be discarded.
         *             Ideally you want this to be as big as possible.
         * @param far Far clipping plane. Any fragments further away than this from the camera would be discarded.
         *            Ideally you want this to be as small as possible.
         * @return Reference to the matrix that was constructed
         */
        glm::mat4 &buildOrtho(const float &left, const float &oRight, const float &bottom, const float &top, const float &near = 1.0f, const float &far = 100.0f);

        /**
         * @brief Build the view matrix and store it in `stms::Camera::matV`
         *        **NOTE: The view matrix is simply the inverse of the model matrix (`trans.mat4`). Therefore,
         *        `trans.buildMatM()` is called by this function.**
         * @return The constructed view matrix
         */
        glm::mat4 &buildMatV();

        /**
         * @brief Shorthand for `buildMatV()` and `trans.buildVecs()`. This builds `matV`, `trans.mat4`,
         *        and the rotation vectors of `trans`.
         */
        inline void buildAll() {
            buildMatV();
            trans.buildVecs();
        }
    };
}

#endif //__STONEMASON_CAMERA_HPP
