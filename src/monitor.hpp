#pragma once

#include <string>
#include <vector>

#include "math/vector2i.hpp"
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

namespace mve {

class Monitor {
public:
    Monitor(GLFWmonitor* monitor);

    [[nodiscard]] mve::Vector2i size() const;

    [[nodiscard]] mve::Vector2i position() const;

    [[nodiscard]] mve::Vector2i physical_size() const;

    [[nodiscard]] GLFWmonitor* glfw_handle();

    [[nodiscard]] int refresh_rate() const;

    [[nodiscard]] std::string name() const;

    [[nodiscard]] static int count();

    [[nodiscard]] static std::vector<Monitor> list();

private:
    GLFWmonitor* m_monitor;
};

}