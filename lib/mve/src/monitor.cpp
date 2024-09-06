#include <game_performance_profiler.hpp>
#include <mve/monitor.hpp>

mve::Monitor::Monitor(GLFWmonitor* monitor)
    : m_monitor(monitor)
{
}
int mve::Monitor::count()
{
    PROFILE_START("VOXELVERSE:mve::Monitor::count()")
    int monitor_count;
    glfwGetMonitors(&monitor_count);
    PROFILE_STOP("VOXELVERSE:mve::Monitor::count()")
    return monitor_count;
}

nnm::Vector2i mve::Monitor::size() const
{
    PROFILE_START("VOXELVERSE:mve::Monitor::size()")
    const GLFWvidmode* mode = glfwGetVideoMode(m_monitor);
    PROFILE_STOP("VOXELVERSE:mve::Monitor::size()")
    return { mode->width, mode->height };
}

nnm::Vector2i mve::Monitor::position() const
{
    PROFILE_START("VOXELVERSE:mve::Monitor::position()")
    nnm::Vector2i pos;
    glfwGetMonitorPos(m_monitor, &pos.x, &pos.y);
    PROFILE_STOP("VOXELVERSE:mve::Monitor::position()")
    return pos;
}
nnm::Vector2i mve::Monitor::physical_size() const
{
    PROFILE_START("VOXELVERSE:mve::Monitor::physical_size()")
    nnm::Vector2i physical_size;
    glfwGetMonitorPhysicalSize(m_monitor, &physical_size.x, &physical_size.y);
    PROFILE_STOP("VOXELVERSE:mve::Monitor::physical_size()")
    return physical_size;
}

GLFWmonitor* mve::Monitor::glfw_handle() const
{
    return m_monitor;
}

std::vector<mve::Monitor> mve::Monitor::list()
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    int monitor_count;
    GLFWmonitor** glfw_monitors = glfwGetMonitors(&monitor_count);
    std::vector<Monitor> monitors;
    monitors.reserve(monitor_count);

    for (int i = 0; i < monitor_count; i++) {
        monitors.emplace_back(glfw_monitors[i]);
    }
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    return monitors;
}

int mve::Monitor::refresh_rate() const
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    const GLFWvidmode* mode = glfwGetVideoMode(m_monitor);
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    return mode->refreshRate;
}
std::string mve::Monitor::name() const
{
    return glfwGetMonitorName(m_monitor);
}
