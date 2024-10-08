#include "fixed_loop.hpp"

#include <chrono>

#include <game_performance_profiler.hpp>

namespace util {

FixedLoop::FixedLoop(const float rate)
    : m_end(std::chrono::steady_clock::now())
{
    m_delta = 0;
    m_is_ready = false;
    m_rate = static_cast<int64_t>(1.0 / rate * static_cast<int64_t>(1000000000));
    m_blend = 0;
}

void FixedLoop::update(const int max_loops, std::optional<std::function<void()>> callback)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    update_state();
    int loop_count = 0;
    while (m_is_ready) {
        if (callback.has_value()) {
            std::invoke(callback.value());
        }
        update_state();
        if (++loop_count >= max_loops) {
            m_delta = 0;
            break;
        }
    }
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void FixedLoop::set_rate(const float rate)
{
    m_rate = static_cast<int64_t>(static_cast<double>(1.0f / rate) * static_cast<int64_t>(1000000000));
}

float FixedLoop::blend() const
{
    return static_cast<float>(m_blend);
}

void FixedLoop::reset()
{
    m_end = std::chrono::steady_clock::now();
    m_delta = 0;
    m_is_ready = false;
}

void FixedLoop::update_state()
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    m_start = std::chrono::steady_clock::now();
    m_delta += std::chrono::duration_cast<std::chrono::nanoseconds>(m_start - m_end).count();
    m_end = m_start;
    if (m_delta >= m_rate) {
        m_is_ready = true;
        m_delta -= m_rate;
    }
    else {
        m_is_ready = false;
    }
    m_blend = static_cast<double>(m_delta) / static_cast<double>(m_rate);
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

}
