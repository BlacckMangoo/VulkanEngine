#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

enum class ProjectionType { Perspective, Orthographic };
// Base (quaternion + quat) - > convert to euler for user and ui -> let user modify euler angles -> convert back to quat for internal representation
// camera start with standard basis vectors where forward is -z , up is +y, right is +x.



class Camera {
public:
    glm::vec3 position{ 0.0f };
    glm::quat orientation{ 1.0f, 0.0f, 0.0f, 0.0f }; // identity

    ProjectionType projectionType = ProjectionType::Perspective;
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 10.0f;
    float orthoHeight = 4.0f;

    // Sets position + orientation to exactly match glm::lookAt(eye, target, worldUp)
    void lookAt( const glm::vec3& target, const glm::vec3& worldUp);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    glm::vec3 getForward() const { return orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
    glm::vec3 getRight()   const { return orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
    glm::vec3 getUp()      const { return orientation * glm::vec3(0.0f, 1.0f, 0.0f); }
};
