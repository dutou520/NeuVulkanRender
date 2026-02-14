#include "Camera.h"
#include <cmath>

namespace neurender {

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_Position(position), m_WorldUp(up), m_Yaw(yaw), m_Pitch(pitch),
      m_InitialPosition(position), m_InitialYaw(yaw), m_InitialPitch(pitch) {
  UpdateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
  return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
  glm::mat4 proj = glm::perspective(glm::radians(m_Fov), aspectRatio,
                                    m_NearPlane, m_FarPlane);
  // Vulkan Y轴翻转
  proj[1][1] *= -1;

  // TAA Jitter
  proj[2][0] += m_JitterOffset.x;
  proj[2][1] += m_JitterOffset.y;

  return proj;
}

void Camera::ProcessKeyboard(Movement direction, float deltaTime) {
  float velocity = m_MovementSpeed * deltaTime;

  switch (direction) {
  case Movement::FORWARD:
    m_Position += m_Front * velocity;
    break;
  case Movement::BACKWARD:
    m_Position -= m_Front * velocity;
    break;
  case Movement::LEFT:
    m_Position -= m_Right * velocity;
    break;
  case Movement::RIGHT:
    m_Position += m_Right * velocity;
    break;
  case Movement::UP:
    m_Position += m_WorldUp * velocity;
    break;
  case Movement::DOWN:
    m_Position -= m_WorldUp * velocity;
    break;
  }
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  xoffset *= m_MouseSensitivity;
  yoffset *= m_MouseSensitivity;

  m_Yaw += xoffset;
  m_Pitch += yoffset;

  // 限制俯仰角，避免翻转
  if (constrainPitch) {
    m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
  }

  UpdateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
  m_Fov -= yoffset;
  m_Fov = glm::clamp(m_Fov, 1.0f, 120.0f);
}

void Camera::Reset() {
  m_Position = m_InitialPosition;
  m_Yaw = m_InitialYaw;
  m_Pitch = m_InitialPitch;
  m_Fov = 45.0f;
  UpdateCameraVectors();
}

void Camera::UpdateCameraVectors() {
  // 计算新的前向量
  glm::vec3 front;
  front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
  front.y = sin(glm::radians(m_Pitch));
  front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
  m_Front = glm::normalize(front);

  // 重新计算右向量和上向量
  m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
  m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}

} // namespace neurender
