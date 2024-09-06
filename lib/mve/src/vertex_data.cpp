#include <mve/vertex_data.hpp>

#include <stdexcept>

#include <mve/common.hpp>

#include <game_performance_profiler.hpp>

namespace mve {

int get_vertex_layout_bytes(const VertexLayout& vertex_layout)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    int byte_count = 0;
    for (const VertexAttributeType type : vertex_layout) {
        switch (type) {
        case VertexAttributeType::scalar:
            byte_count += sizeof(float);
            break;
        case VertexAttributeType::vec2:
            byte_count += sizeof(nnm::Vector2f);
            break;
        case VertexAttributeType::vec3:
            byte_count += sizeof(nnm::Vector3f);
            break;
        case VertexAttributeType::vec4:
            byte_count += sizeof(nnm::Vector4f);
            break;
        }
    }
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    return byte_count;
}

VertexData::VertexData(VertexLayout layout)
    : m_layout(std::move(layout))
{
    MVE_ASSERT(!m_layout.empty(), "[VertexData] Empty vertex layout")
}

void VertexData::push_back(const float value)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    MVE_VAL_ASSERT(next_type() == VertexAttributeType::scalar, "[VertexData] Invalid type: scalar")

    m_data.push_back(value);

    m_data_count++;
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void VertexData::push_back(nnm::Vector2f value)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    MVE_VAL_ASSERT(next_type() == VertexAttributeType::vec2, "[VertexData] Invalid type: vec2")

    m_data.push_back(value[0]);
    m_data.push_back(value[1]);

    m_data_count++;
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void VertexData::push_back(nnm::Vector3f value)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    MVE_VAL_ASSERT(next_type() == VertexAttributeType::vec3, "[VertexData] Invalid type: vec3")

    m_data.push_back(value[0]);
    m_data.push_back(value[1]);
    m_data.push_back(value[2]);

    m_data_count++;
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void VertexData::push_back(nnm::Vector4f value)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    MVE_VAL_ASSERT(next_type() == VertexAttributeType::vec4, "[VertexData] Invalid type: vec4")

    m_data.push_back(value[0]);
    m_data.push_back(value[1]);
    m_data.push_back(value[2]);
    m_data.push_back(value[3]);

    m_data_count++;
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

VertexAttributeType VertexData::next_type() const noexcept
{
    return m_layout[m_data_count % m_layout.size()];
}

const float* VertexData::data_ptr() const noexcept
{
    return m_data.data();
}

int VertexData::data_count() const noexcept
{
    return m_data_count;
}

bool VertexData::is_complete() const noexcept
{
    return m_data_count % m_layout.size() == 0;
}

VertexLayout VertexData::layout() const noexcept
{
    return m_layout;
}

int VertexData::vertex_count() const noexcept
{
    return m_data_count / static_cast<int>(m_layout.size());
}
}