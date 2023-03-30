#include "hud.hpp"

HUD::HUD(UIPipeline& ui_pipeline, TextPipeline& text_pipeline)
    : m_show_debug(false)
    , m_hotbar(ui_pipeline)
    , m_crosshair(ui_pipeline)
    , m_debug_overlay(text_pipeline)
    , m_console(text_pipeline)
    , m_button(ui_pipeline, 10.0f)
{
    m_hotbar.set_item(0, 1);
    m_hotbar.set_item(1, 2);
    m_hotbar.set_item(2, 3);
    m_hotbar.set_item(3, 4);
    m_hotbar.set_item(4, 5);
    m_hotbar.set_item(5, 6);
    m_hotbar.set_item(6, 7);
    m_hotbar.set_item(7, 8);
    m_hotbar.set_item(8, 9);
}
void HUD::resize(mve::Vector2i extent)
{
    m_hotbar.resize(extent);
    m_debug_overlay.resize(extent);
    m_console.resize(extent);
}

void HUD::draw()
{
    m_crosshair.draw();
    m_hotbar.draw();
    if (m_show_debug) {
        m_debug_overlay.draw();
    }
    m_console.draw();
    m_button.draw();
}

void HUD::toggle_debug()
{
    m_show_debug = !m_show_debug;
}
