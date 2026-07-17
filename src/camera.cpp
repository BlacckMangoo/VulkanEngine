#include "camera.h"

void Camera::lookAt( const glm::vec3& target, const glm::vec3& worldUp) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 upDir = glm::cross(right, forward);
    glm::mat3 basis(right, upDir, -forward);
    orientation = glm::quat_cast(basis);
}

glm::mat4 Camera::getViewMatrix() const {
    glm::mat4 rot = glm::mat4_cast(orientation);
    glm::mat4 rotInv = glm::transpose(rot);
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), -position);
    return rotInv * trans;
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    glm::mat4 proj;
    if (projectionType == ProjectionType::Perspective) {
        proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }
    else {
        float orthoWidth = orthoHeight * aspectRatio;
        proj = glm::ortho(-orthoWidth / 2.0f, orthoWidth / 2.0f,
            -orthoHeight / 2.0f, orthoHeight / 2.0f,
            nearPlane, farPlane);
    }
    proj[1][1] *= -1;
    return proj;
}