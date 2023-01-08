#include "vertex_buffer.hpp"

#include "renderer.hpp"
#include "vertex_data.hpp"

namespace mve {

VertexBuffer::VertexBuffer(Renderer& renderer, const VertexData& data)
    : m_renderer(&renderer)
    , m_valid(true)
    , m_handle(renderer.create_vertex_buffer_handle(data))
{
}

VertexBuffer::VertexBuffer(VertexBuffer&& other)
    : m_valid(other.m_valid)
    , m_renderer(other.m_renderer)
    , m_handle(other.m_handle)
{
    other.m_valid = false;
}

VertexBuffer::~VertexBuffer()
{
    if (m_valid) {
        m_renderer->queue_destroy(m_handle);
    }
}
VertexBufferHandle VertexBuffer::handle() const
{
    return m_handle;
}
bool VertexBuffer::is_valid() const
{
    return m_valid;
}

VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other)
{
    if (m_valid) {
        m_renderer->queue_destroy(m_handle);
    }

    m_valid = other.m_valid;
    m_renderer = other.m_renderer;
    m_handle = other.m_handle;

    other.m_valid = false;

    return *this;
}
bool VertexBuffer::operator==(const VertexBuffer& other) const
{
    return m_valid == other.m_valid && m_renderer == other.m_renderer && m_handle == other.m_handle;
}
bool VertexBuffer::operator<(const VertexBuffer& other) const
{
    return m_handle < other.m_handle;
}

VertexBuffer::VertexBuffer(Renderer& renderer, VertexBufferHandle handle)
    : m_valid(true)
    , m_renderer(&renderer)
    , m_handle(handle)
{
}

VertexBufferHandle::VertexBufferHandle(uint64_t value)
    : m_initialized(true)
    , m_value(value)
{
}
uint64_t VertexBufferHandle::value() const
{
    return m_value;
}
bool VertexBufferHandle::operator==(const VertexBufferHandle& other) const
{
    return m_value == other.m_value;
}
bool VertexBufferHandle::operator<(const VertexBufferHandle& other) const
{
    return m_value < other.m_value;
}

VertexBufferHandle::VertexBufferHandle()
    : m_initialized(false)
{
}
void VertexBufferHandle::set(uint64_t value)
{
    m_initialized = true;
    m_value = value;
}

}