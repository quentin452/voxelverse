#include "app.hpp"

#include <chrono>

#include <glm/ext.hpp>

#include "logger.hpp"
#include "renderer.hpp"
#include "shader.hpp"
#include "util.hpp"
#include "window.hpp"

namespace app {

void run()
{
    LOG->debug("Creating window");

    mve::Window window("Mini Vulkan Engine", glm::ivec2(800, 600));

    window.set_min_size({ 800, 600 });

    mve::Renderer renderer(window, "Vulkan Testing", 0, 0, 1);

    mve::Shader vertex_shader("../res/bin/shader/simple.vert.spv", mve::ShaderType::vertex);
    mve::Shader fragment_shader("../res/bin/shader/simple.frag.spv", mve::ShaderType::fragment);

    mve::ModelData model_data = mve::load_model("../res/viking_room.obj");

    mve::VertexBuffer model_vertex_buffer = renderer.create_vertex_buffer(model_data.vertex_data);
    mve::IndexBuffer model_index_buffer = renderer.create_index_buffer(model_data.indices);

    mve::GraphicsPipeline graphics_pipeline
        = renderer.create_graphics_pipeline(vertex_shader, fragment_shader, model_data.vertex_data.layout());

    mve::DescriptorSet descriptor_set = graphics_pipeline.create_descriptor_set(0);

    mve::UniformBuffer uniform_buffer = renderer.create_uniform_buffer(vertex_shader.descriptor_set(0).binding(0));

    descriptor_set.write_binding(vertex_shader.descriptor_set(0).binding(0), uniform_buffer);

    std::chrono::high_resolution_clock::time_point begin_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    mve::UniformLocation view_location = vertex_shader.descriptor_set(0).binding(0).member("view").location();
    mve::UniformLocation model_location = vertex_shader.descriptor_set(0).binding(0).member("model").location();
    mve::UniformLocation proj_location = vertex_shader.descriptor_set(0).binding(0).member("proj").location();

    uniform_buffer.update(view_location, view);

    auto resize_func = [&](glm::ivec2 new_size) {
        renderer.resize(window);
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), (float)renderer.extent().x / (float)renderer.extent().y, 0.1f, 10.0f);
        proj[1][1] *= -1;
        uniform_buffer.update(proj_location, proj);
    };

    window.set_resize_callback(resize_func);

    std::invoke(resize_func, window.size());

    mve::Texture texture = renderer.create_texture("../res/viking_room.png");

    descriptor_set.write_binding(fragment_shader.descriptor_set(0).binding(1), texture);

    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    while (!window.should_close()) {
        window.poll_events();

        if (window.is_key_pressed(mve::InputKey::escape)) {
            break;
        }

        if (window.is_key_pressed(mve::InputKey::f)) {
            if (!window.is_fullscreen()) {
                window.fullscreen(true);
            }
            else {
                window.windowed();
            }
        }

        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        if (window.is_key_down(mve::InputKey::left)) {
            model = glm::rotate(model, glm::radians(0.1f), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        if (window.is_key_down(mve::InputKey::right)) {
            model = glm::rotate(model, glm::radians(-0.1f), glm::vec3(0.0f, 0.0f, 1.0f));
        }

        uniform_buffer.update(model_location, model, false);

        renderer.begin(window);

        renderer.bind_graphics_pipeline(graphics_pipeline);

        renderer.bind_descriptor_set(descriptor_set);

        renderer.bind_vertex_buffer(model_vertex_buffer);

        renderer.draw_index_buffer(model_index_buffer);

        renderer.end(window);

        std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();

        if (std::chrono::duration_cast<std::chrono::microseconds>(end_time - begin_time).count() >= 1000000) {
            begin_time = std::chrono::high_resolution_clock::now();
            LOG->info("Framerate: {}", frame_count);
            frame_count = 0;
        }

        frame_count++;
    }
}
}
