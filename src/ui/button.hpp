#pragma once

#include "../text_buffer.hpp"
#include "nine_patch.hpp"

class Button {
public:
    explicit Button(UIPipeline& ui_pipeline, TextPipeline& text_pipeline, const std::string& text, float scale = 1.0f);

    void draw() const;

    void set_position(const mve::Vector2& pos);

    void set_scale(float scale);

    [[nodiscard]] inline mve::Vector2 position() const
    {
        return m_position;
    }

    [[nodiscard]] inline float scale() const
    {
        return m_scale;
    }

private:
    NinePatch m_patch;
    TextBuffer m_text;
    mve::Vector2 m_position;
    float m_scale;
};