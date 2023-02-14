#pragma once

#include <unordered_map>

#include "camera.hpp"
#include "chunk_mesh.hpp"
#include "common.hpp"
#include "frustum.hpp"
#include "mve/math/math.hpp"
#include "select_box_mesh.hpp"

class WorldRenderer {
public:
    WorldRenderer(mve::Renderer& renderer);

    void add_data(const ChunkData& chunk_data, const WorldData& world_data);

    bool contains_data(mve::Vector3i position);

    void remove_data(mve::Vector3i position);

    void set_view(const mve::Matrix4& view);

    void resize();

    void set_selection_position(mve::Vector3 position);

    inline void hide_selection()
    {
        m_selection_box.is_shown = false;
    }

    inline void show_selection()
    {
        m_selection_box.is_shown = true;
    }

    void draw(const Camera& camera);

    inline static mve::VertexLayout vertex_layout()
    {
        return {
            mve::VertexAttributeType::vec3, // Position
            mve::VertexAttributeType::vec3, // Color
            mve::VertexAttributeType::vec2 // UV
        };
    }

private:
    struct SelectionBox {
        bool is_shown;
        SelectBoxMesh mesh;
    };

    void rebuild_mesh_lookup();

    mve::Renderer* m_renderer;
    mve::Shader m_vertex_shader;
    mve::Shader m_fragment_shader;
    mve::GraphicsPipeline m_graphics_pipeline;
    std::shared_ptr<mve::Texture> m_block_texture;
    mve::UniformBuffer m_global_ubo;
    mve::UniformBuffer m_chunk_ubo;
    mve::DescriptorSet m_global_descriptor_set;
    mve::DescriptorSet m_chunk_descriptor_set;
    mve::UniformLocation m_view_location;
    mve::UniformLocation m_proj_location;
    std::unordered_map<mve::Vector3i, size_t> m_chunk_mesh_lookup {};
    std::vector<std::optional<ChunkMesh>> m_chunk_meshes;
    Frustum m_frustum;
    SelectionBox m_selection_box;
};