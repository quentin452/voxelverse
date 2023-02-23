#pragma once

#include "math.h"
#include <functional>
#include <vector>

struct Quad {
    mve::Vector3 top_left;
    mve::Vector3 top_right;
    mve::Vector3 bottom_right;
    mve::Vector3 bottom_left;
};

struct BoundingBox {
    mve::Vector3 min;
    mve::Vector3 max;
};

enum class Direction { front = 0, back, left, right, top, bottom };

inline bool collides(const BoundingBox& a, const BoundingBox& b)
{
    bool collision = true;

    if ((a.max.x >= b.min.x) && (a.min.x <= b.max.x)) {
        if ((a.max.y < b.min.y) || (a.min.y > b.max.y)) {
            collision = false;
        }
        if ((a.max.z < b.min.z) || (a.min.z > b.max.z)) {
            collision = false;
        }
    }
    else {
        collision = false;
    }

    return collision;
}

inline Quad transform(const Quad& quad, const mve::Matrix4& matrix)
{
    return {
        quad.top_left.transform(matrix),
        quad.top_right.transform(matrix),
        quad.bottom_right.transform(matrix),
        quad.bottom_left.transform(matrix),
    };
}

inline void transform_vertices(std::vector<mve::Vector3>& vertices, const mve::Matrix4& matrix)
{
    for (mve::Vector3& vertex : vertices) {
        vertex = vertex.transform(matrix);
    }
}

inline void for_2d(mve::Vector2i from, mve::Vector2i to, std::function<void(mve::Vector2i)> func)
{
    for (int x = from.x; x < to.x; x++) {
        for (int y = from.y; y < to.y; y++) {
            std::invoke(func, mve::Vector2i(x, y));
        }
    }
}

inline void for_3d(mve::Vector3i from, mve::Vector3i to, std::function<void(mve::Vector3i)> func)
{
    for (int x = from.x; x < to.x; x++) {
        for (int y = from.y; y < to.y; y++) {
            for (int z = from.z; z < to.z; z++) {
                std::invoke(func, mve::Vector3i(x, y, z));
            }
        }
    }
}

struct QuadUVs {
    mve::Vector2 top_left;
    mve::Vector2 top_right;
    mve::Vector2 bottom_right;
    mve::Vector2 bottom_left;
};

inline QuadUVs uvs_from_atlas(mve::Vector2i atlas_size, mve::Vector2i pos)
{
    mve::Vector2 atlas_unit = mve::Vector2(1.0f / atlas_size.x, 1.0f / atlas_size.y);

    QuadUVs uvs;
    uvs.top_left = mve::Vector2(pos.x * atlas_unit.x, pos.y * atlas_unit.y);
    uvs.top_right = uvs.top_left + mve::Vector2(atlas_unit.x, 0.0f);
    uvs.bottom_right = uvs.top_right + mve::Vector2(0.0f, atlas_unit.y);
    uvs.bottom_left = uvs.bottom_right + mve::Vector2(-atlas_unit.x, 0.0f);

    return uvs;
}

inline mve::Vector2i block_uv(uint8_t block_type, Direction face)
{
    switch (block_type) {
    case 1:
        switch (face) {
        case Direction::top:
            return { 1, 0 };
        case Direction::bottom:
            return { 0, 1 };
        default:
            return { 0, 0 };
        }
    case 2:
        return { 1, 1 };
    case 3:
        return { 2, 0 };
    case 4:
        return { 0, 1 };
    case 5:
        switch (face) {
        case Direction::top:
        case Direction::bottom:
            return { 3, 1 };
        default:
            return { 2, 1 };
        }
    case 6:
        return { 0, 2 };
    case 7:
        return { 1, 2 };
    case 8:
        return { 2, 2 };
    default:
        return { 0, 0 };
    }
}
