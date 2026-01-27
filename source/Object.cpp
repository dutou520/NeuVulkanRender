#include "Core/Object.h"

namespace neurender {

Object::Object() : m_UUID(UUID::Generate()), m_Name("Object") {}

Object::Object(const std::string &name)
    : m_UUID(UUID::Generate()), m_Name(name) {}

} // namespace neurender
