#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace neurender {

/**
 * @brief 自由相机类
 *
 * 支持 WASD 移动和鼠标旋转
 */
class Camera {
public:
  enum class Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

  Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
         glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f,
         float pitch = 0.0f);

  /**
   * @brief 获取视图矩阵
   */
  glm::mat4 GetViewMatrix() const;

  /**
   * @brief 获取投影矩阵
   * @param aspectRatio 宽高比
   */
  glm::mat4 GetProjectionMatrix(float aspectRatio) const;

  /**
   * @brief 处理键盘输入
   * @param direction 移动方向
   * @param deltaTime 帧时间差
   */
  void ProcessKeyboard(Movement direction, float deltaTime);

  /**
   * @brief 处理鼠标移动
   * @param xoffset X方向偏移
   * @param yoffset Y方向偏移
   * @param constrainPitch 是否限制俯仰角
   */
  void ProcessMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);

  /**
   * @brief 处理鼠标滚轮（缩放）
   * @param yoffset Y方向偏移
   */
  void ProcessMouseScroll(float yoffset);

  /**
   * @brief 重置相机到初始位置
   */
  void Reset();

  // Getters
  glm::vec3 GetPosition() const { return m_Position; }
  glm::vec3 GetFront() const { return m_Front; }
  glm::vec3 GetUp() const { return m_Up; }
  glm::vec3 GetRight() const { return m_Right; }
  float GetYaw() const { return m_Yaw; }
  float GetPitch() const { return m_Pitch; }
  float GetFov() const { return m_Fov; }
  float GetMovementSpeed() const { return m_MovementSpeed; }
  float GetMouseSensitivity() const { return m_MouseSensitivity; }

  // Setters
  void SetPosition(const glm::vec3 &position) { m_Position = position; }
  void SetMovementSpeed(float speed) { m_MovementSpeed = speed; }
  void SetMouseSensitivity(float sensitivity) {
    m_MouseSensitivity = sensitivity;
  }
  void SetFov(float fov) { m_Fov = glm::clamp(fov, 1.0f, 120.0f); }
  void SetNearPlane(float nearPlane) { m_NearPlane = nearPlane; }
  void SetFarPlane(float farPlane) { m_FarPlane = farPlane; }

private:
  void UpdateCameraVectors();

  // 相机属性
  glm::vec3 m_Position;
  glm::vec3 m_Front;
  glm::vec3 m_Up;
  glm::vec3 m_Right;
  glm::vec3 m_WorldUp;

  // 欧拉角
  float m_Yaw;
  float m_Pitch;

  // 相机参数
  float m_MovementSpeed = 2.5f;
  float m_MouseSensitivity = 0.1f;
  float m_Fov = 45.0f;
  float m_NearPlane = 0.1f;
  float m_FarPlane = 100.0f;

  // 初始值（用于重置）
  glm::vec3 m_InitialPosition;
  float m_InitialYaw;
  float m_InitialPitch;
};

} // namespace neurender
