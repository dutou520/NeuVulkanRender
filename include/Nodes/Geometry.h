#pragma once
#include "Nodes/Node.h"

namespace neurender {
class Geometry : public Node {
public:
  Geometry();
  Geometry(const Geometry &) = delete;
  Geometry &operator=(const Geometry &) = delete;
};
} // namespace neurender