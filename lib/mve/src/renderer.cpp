#include <mve/renderer.hpp>

#include <fstream>
#include <set>
#include <vector>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <mve/common.hpp>
#include <mve/math/math.hpp>
#include <mve/vertex_data.hpp>
#include <mve/window.hpp>

#include "renderer_utils.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace mve {

using namespace detail;

Renderer::Renderer(
    const Window& window,
    const std::string& app_name,
    const int app_version_major,
    const int app_version_minor,
    const int app_version_patch)
    : c_frames_in_flight(2)
    , m_vk_instance(create_vk_instance(app_name, app_version_major, app_version_minor, app_version_patch))
    , m_resource_handle_count(0)
    , m_deferred_function_id_count(0)
{
    m_vk_loader = vk::DispatchLoaderDynamic(m_vk_instance, vkGetInstanceProcAddr);
#ifdef MVE_ENABLE_VALIDATION
    m_vk_debug_utils_messenger = create_vk_debug_messenger(m_vk_instance);
#endif
    m_vk_surface = create_vk_surface(m_vk_instance, window.glfw_handle());
    m_vk_physical_device = pick_vk_physical_device(m_vk_instance, m_vk_loader, m_vk_surface);
    m_msaa_samples = vk::SampleCountFlagBits::e4; // get_max_sample_count(m_vk_loader, m_vk_physical_device);
    m_vk_queue_family_indices = get_vk_queue_family_indices(m_vk_loader, m_vk_physical_device, m_vk_surface);
    m_vk_device = create_vk_logical_device(m_vk_loader, m_vk_physical_device, m_vk_queue_family_indices);
    m_vk_loader = vk::DispatchLoaderDynamic(m_vk_instance, vkGetInstanceProcAddr, m_vk_device, vkGetDeviceProcAddr);

    auto [capabilities, formats, present_modes]
        = get_vk_swapchain_support_details(m_vk_loader, m_vk_physical_device, m_vk_surface);

    m_vk_swapchain_image_format = choose_vk_swapchain_surface_format(formats);
    m_vk_swapchain_extent = get_vk_swapchain_extent(capabilities, window.glfw_handle());
    m_vk_swapchain = create_vk_swapchain(
        m_vk_loader,
        m_vk_physical_device,
        m_vk_device,
        m_vk_surface,
        m_vk_swapchain_image_format,
        m_vk_swapchain_extent,
        m_vk_queue_family_indices);

    m_vk_swapchain_images = get_vk_swapchain_images(m_vk_loader, m_vk_device, m_vk_swapchain);

    m_vk_swapchain_image_views = create_vk_swapchain_image_views(
        m_vk_loader, m_vk_device, m_vk_swapchain_images, m_vk_swapchain_image_format.format);

    m_vk_graphics_queue = m_vk_device.getQueue(m_vk_queue_family_indices.graphics_family.value(), 0, m_vk_loader);
    m_vk_present_queue = m_vk_device.getQueue(m_vk_queue_family_indices.present_family.value(), 0, m_vk_loader);

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    VmaVulkanFunctions func = {};
    func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorCreateInfo.pVulkanFunctions = &func;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorCreateInfo.physicalDevice = m_vk_physical_device;
    allocatorCreateInfo.device = m_vk_device;
    allocatorCreateInfo.instance = m_vk_instance;
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    vmaCreateAllocator(&allocatorCreateInfo, &m_vma_allocator);

    m_vk_command_pool = create_vk_command_pool(m_vk_loader, m_vk_device, m_vk_queue_family_indices);

    m_color_image = create_color_image(
        m_vk_loader,
        m_vk_device,
        m_vma_allocator,
        m_vk_swapchain_extent,
        m_vk_swapchain_image_format.format,
        m_msaa_samples);
    m_depth_image = create_depth_image(
        m_vk_loader,
        m_vk_physical_device,
        m_vk_device,
        m_vk_command_pool,
        m_vk_graphics_queue,
        m_vma_allocator,
        m_vk_swapchain_extent,
        m_msaa_samples);

    m_vk_render_pass = create_vk_render_pass(
        m_vk_loader,
        m_vk_device,
        m_vk_swapchain_image_format.format,
        find_depth_format(m_vk_loader, m_vk_physical_device),
        m_msaa_samples);

    m_vk_render_pass_framebuffer = create_vk_render_pass_framebuffer(
        m_vk_loader,
        m_vk_device,
        m_vk_swapchain_image_format.format,
        find_depth_format(m_vk_loader, m_vk_physical_device),
        m_msaa_samples);

    m_vk_swapchain_framebuffers = create_vk_framebuffers(
        m_vk_loader,
        m_vk_device,
        m_vk_swapchain_image_views,
        m_vk_render_pass,
        m_vk_swapchain_extent,
        m_color_image.vk_image_view,
        m_depth_image.vk_image_view,
        m_msaa_samples);

    m_frames_in_flight = create_frames_in_flight(m_vk_loader, m_vk_device, m_vk_command_pool, c_frames_in_flight);

    m_current_draw_state.is_drawing = false;
    m_current_draw_state.frame_index = 0;
    m_current_draw_state.image_index = 0;
    m_current_draw_state.command_buffer = nullptr;
}

vk::PipelineLayout Renderer::create_vk_pipeline_layout(
    const vk::DispatchLoaderDynamic& loader, const std::vector<DescriptorSetLayoutHandleImpl>& layouts) const
{
    std::vector<vk::DescriptorSetLayout> vk_layouts;
    std::ranges::transform(layouts, std::back_inserter(vk_layouts), [&](const DescriptorSetLayoutHandleImpl handle) {
        return m_descriptor_set_layouts.at(handle);
    });

    const auto pipeline_layout_info
        = vk::PipelineLayoutCreateInfo()
              .setSetLayouts(vk_layouts)
              .setPushConstantRangeCount(0)
              .setPPushConstantRanges(nullptr);

    const vk::ResultValue<vk::PipelineLayout> pipeline_layout_result
        = m_vk_device.createPipelineLayout(pipeline_layout_info, nullptr, loader);
    MVE_ASSERT(pipeline_layout_result.result == vk::Result::eSuccess, "[Renderer] Failed to create pipline layout")
    return pipeline_layout_result.value;
}

Renderer::~Renderer()
{
#ifdef MVE_ENABLE_VALIDATION
    cleanup_vk_debug_messenger();
#endif
    // ReSharper disable once CppDFAUnreadVariable
    vk::Result _ = m_vk_device.waitIdle(m_vk_loader);

    cleanup_vk_swapchain();

    for (std::optional<FramebufferImpl>& framebuffer : m_framebuffers) {
        if (framebuffer.has_value()) {
            for (const vk::Framebuffer& buffer : framebuffer->vk_framebuffers) {
                m_vk_device.destroy(buffer, nullptr, m_vk_loader);
            }
            if (framebuffer->texture.is_valid()) {
                destroy(framebuffer->texture);
            }
        }
    }

    for (auto& [handle, texture] : m_textures) {
        m_vk_device.destroy(texture.vk_sampler, nullptr, m_vk_loader);
        m_vk_device.destroy(texture.vk_image_view, nullptr, m_vk_loader);
        vmaDestroyImage(m_vma_allocator, texture.image.vk_handle, texture.image.vma_allocation);
    }

    m_descriptor_set_allocator.cleanup(m_vk_loader, m_vk_device);

    // ReSharper disable once CppUseStructuredBinding
    for (FrameInFlight& frame : m_frames_in_flight) {
        for (std::optional<UniformBufferImpl>& uniform_buffer : frame.uniform_buffers) {
            if (uniform_buffer.has_value()) {
                vmaUnmapMemory(m_vma_allocator, uniform_buffer->buffer.vma_allocation);
                vmaDestroyBuffer(
                    m_vma_allocator, uniform_buffer->buffer.vk_handle, uniform_buffer->buffer.vma_allocation);
            }
        }
    }

    for (auto& [handle, layout] : m_descriptor_set_layouts) {
        m_vk_device.destroy(layout, nullptr, m_vk_loader);
    }

    for (std::optional<VertexBufferImpl>& vertex_buffer : m_vertex_buffers) {
        if (vertex_buffer.has_value()) {
            vmaDestroyBuffer(m_vma_allocator, vertex_buffer->buffer.vk_handle, vertex_buffer->buffer.vma_allocation);
        }
    }

    for (std::optional<IndexBufferImpl>& index_buffer : m_index_buffers) {
        if (index_buffer.has_value()) {
            vmaDestroyBuffer(m_vma_allocator, index_buffer->buffer.vk_handle, index_buffer->buffer.vma_allocation);
        }
    }

    vmaDestroyAllocator(m_vma_allocator);

    for (std::optional<GraphicsPipelineImpl>& pipeline : m_graphics_pipelines) {
        if (pipeline.has_value()) {
            m_vk_device.destroy(pipeline->pipeline, nullptr, m_vk_loader);
        }
    }

    for (std::optional<GraphicsPipelineLayoutImpl>& layout : m_graphics_pipeline_layouts) {
        if (layout.has_value()) {
            m_vk_device.destroy(layout->vk_handle, nullptr, m_vk_loader);
        }
    }

    m_vk_device.destroy(m_vk_render_pass, nullptr, m_vk_loader);
    m_vk_device.destroy(m_vk_render_pass_framebuffer, nullptr, m_vk_loader);

    // ReSharper disable once CppUseStructuredBinding
    for (const FrameInFlight& frame : m_frames_in_flight) {
        m_vk_device.destroy(frame.render_finished_semaphore, nullptr, m_vk_loader);
        m_vk_device.destroy(frame.image_available_semaphore, nullptr, m_vk_loader);
        m_vk_device.destroy(frame.in_flight_fence, nullptr, m_vk_loader);
    }

    m_vk_device.destroy(m_vk_command_pool, nullptr, m_vk_loader);

    m_vk_device.destroy(nullptr, m_vk_loader);

    m_vk_instance.destroy(m_vk_surface, nullptr, m_vk_loader);
    m_vk_instance.destroy(nullptr, m_vk_loader);
}

void Renderer::recreate_swapchain(const Window& window)
{
    Vector2i window_size;
    glfwGetFramebufferSize(window.glfw_handle(), &window_size.x, &window_size.y);

    while (window_size == Vector2i(0, 0)) {
        glfwGetFramebufferSize(window.glfw_handle(), &window_size.x, &window_size.y);
        Window::wait_for_events();
    }

    const vk::Result wait_result = m_vk_device.waitIdle(m_vk_loader);
    MVE_ASSERT(wait_result == vk::Result::eSuccess, "[Renderer] Failed to wait idle for swapchain recreation")

    cleanup_vk_swapchain();

    auto [capabilities, formats, present_modes]
        = get_vk_swapchain_support_details(m_vk_loader, m_vk_physical_device, m_vk_surface);

    m_vk_swapchain_extent = get_vk_swapchain_extent(capabilities, window.glfw_handle());

    m_vk_swapchain = create_vk_swapchain(
        m_vk_loader,
        m_vk_physical_device,
        m_vk_device,
        m_vk_surface,
        m_vk_swapchain_image_format,
        m_vk_swapchain_extent,
        m_vk_queue_family_indices);

    m_vk_swapchain_images = get_vk_swapchain_images(m_vk_loader, m_vk_device, m_vk_swapchain);

    m_vk_swapchain_image_views = create_vk_swapchain_image_views(
        m_vk_loader, m_vk_device, m_vk_swapchain_images, m_vk_swapchain_image_format.format);

    m_color_image = create_color_image(
        m_vk_loader,
        m_vk_device,
        m_vma_allocator,
        m_vk_swapchain_extent,
        m_vk_swapchain_image_format.format,
        m_msaa_samples);

    m_depth_image = create_depth_image(
        m_vk_loader,
        m_vk_physical_device,
        m_vk_device,
        m_vk_command_pool,
        m_vk_graphics_queue,
        m_vma_allocator,
        m_vk_swapchain_extent,
        m_msaa_samples);

    m_vk_swapchain_framebuffers = create_vk_framebuffers(
        m_vk_loader,
        m_vk_device,
        m_vk_swapchain_image_views,
        m_vk_render_pass,
        m_vk_swapchain_extent,
        m_color_image.vk_image_view,
        m_depth_image.vk_image_view,
        m_msaa_samples);

    recreate_framebuffers();
}

void Renderer::cleanup_vk_swapchain() const
{
    m_vk_device.destroy(m_color_image.vk_image_view, nullptr, m_vk_loader);
    vmaDestroyImage(m_vma_allocator, m_color_image.image.vk_handle, m_color_image.image.vma_allocation);

    m_vk_device.destroy(m_depth_image.vk_image_view, nullptr, m_vk_loader);
    vmaDestroyImage(m_vma_allocator, m_depth_image.image.vk_handle, m_depth_image.image.vma_allocation);

    for (const vk::Framebuffer framebuffer : m_vk_swapchain_framebuffers) {
        m_vk_device.destroy(framebuffer, nullptr, m_vk_loader);
    }
    for (const vk::ImageView image_view : m_vk_swapchain_image_views) {
        m_vk_device.destroy(image_view, nullptr, m_vk_loader);
    }
    m_vk_device.destroy(m_vk_swapchain, nullptr, m_vk_loader);
}

void Renderer::cleanup_vk_debug_messenger() const
{
    if (const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_vk_instance, "vkDestroyDebugUtilsMessengerEXT"));
        func != nullptr) {
        func(m_vk_instance, static_cast<VkDebugUtilsMessengerEXT>(m_vk_debug_utils_messenger), nullptr);
    }
}

void Renderer::destroy(VertexBuffer& vertex_buffer)
{
    MVE_VAL_ASSERT(vertex_buffer.is_valid(), "[Renderer] Attempted to destroy invalid vertex buffer")
    log().debug("[Renderer] Destroyed vertex buffer with ID: {}", vertex_buffer.handle());
    size_t handle = vertex_buffer.handle();
    vertex_buffer.invalidate();
    defer_after_all_frames([this, handle](uint32_t) {
        vmaDestroyBuffer(
            m_vma_allocator,
            m_vertex_buffers.at(handle)->buffer.vk_handle,
            m_vertex_buffers.at(handle)->buffer.vma_allocation);
        m_vertex_buffers[handle].reset();
    });
}

void Renderer::begin_render_pass_present() const
{
    // auto clear_color = vk::ClearValue(vk::ClearColorValue(std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f }));

    std::array<vk::ClearValue, 2> clear_values {};
    clear_values[0].setColor(vk::ClearColorValue(std::array { 0.0f, 0.0f, 0.0f, 1.0f }));
    clear_values[1].setDepthStencil(vk::ClearDepthStencilValue(1.0f, 0));

    const auto render_pass_begin_info
        = vk::RenderPassBeginInfo()
              .setRenderPass(m_vk_render_pass)
              .setFramebuffer(m_vk_swapchain_framebuffers[m_current_draw_state.image_index])
              .setRenderArea(vk::Rect2D().setOffset({ 0, 0 }).setExtent(m_vk_swapchain_extent))
              .setClearValueCount(clear_values.size())
              .setPClearValues(clear_values.data());

    m_current_draw_state.command_buffer.beginRenderPass(
        render_pass_begin_info, vk::SubpassContents::eInline, m_vk_loader);

    auto viewport
        = vk::Viewport()
              .setX(0.0f)
              .setY(0.0f)
              .setWidth(static_cast<float>(m_vk_swapchain_extent.width))
              .setHeight(static_cast<float>(m_vk_swapchain_extent.height))
              .setMinDepth(0.0f)
              .setMaxDepth(1.0f);

    m_current_draw_state.command_buffer.setViewport(0, { viewport }, m_vk_loader);

    auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(m_vk_swapchain_extent);

    m_current_draw_state.command_buffer.setScissor(0, { scissor }, m_vk_loader);
}

void Renderer::begin_render_pass_framebuffer(const Framebuffer& framebuffer) const
{
    constexpr auto clear_color = vk::ClearColorValue(std::array { 142.0f / 255.0f, 186.0f / 255.0f, 1.0f, 1.0f });

    std::array<vk::ClearValue, 2> clear_values {};
    clear_values[0].setColor(clear_color);
    clear_values[1].setDepthStencil(vk::ClearDepthStencilValue(1.0f, 0));

    const auto render_pass_begin_info
        = vk::RenderPassBeginInfo()
              .setRenderPass(m_vk_render_pass_framebuffer)
              .setFramebuffer(m_framebuffers[framebuffer.handle()]->vk_framebuffers[m_current_draw_state.image_index])
              .setRenderArea(vk::Rect2D().setOffset({ 0, 0 }).setExtent(m_vk_swapchain_extent))
              .setClearValueCount(clear_values.size())
              .setPClearValues(clear_values.data());

    m_current_draw_state.command_buffer.beginRenderPass(
        render_pass_begin_info, vk::SubpassContents::eInline, m_vk_loader);

    auto viewport
        = vk::Viewport()
              .setX(0.0f)
              .setY(0.0f)
              .setWidth(static_cast<float>(m_vk_swapchain_extent.width))
              .setHeight(static_cast<float>(m_vk_swapchain_extent.height))
              .setMinDepth(0.0f)
              .setMaxDepth(1.0f);

    m_current_draw_state.command_buffer.setViewport(0, { viewport }, m_vk_loader);

    auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(m_vk_swapchain_extent);

    m_current_draw_state.command_buffer.setScissor(0, { scissor }, m_vk_loader);
}

void Renderer::begin_frame(const Window& window)
{
    MVE_VAL_ASSERT(!m_current_draw_state.is_drawing, "[Renderer] Already drawing")

    m_current_draw_state.is_drawing = true;

    // ReSharper disable once CppUseStructuredBinding
    FrameInFlight& frame = m_frames_in_flight[m_current_draw_state.frame_index];

    wait_ready();

    const vk::ResultValue<uint32_t> acquire_result = m_vk_device.acquireNextImageKHR(
        m_vk_swapchain, UINT64_MAX, frame.image_available_semaphore, nullptr, m_vk_loader);
    if (acquire_result.result == vk::Result::eSuboptimalKHR) {
        recreate_swapchain(window);
        m_current_draw_state.is_drawing = false;
        return;
    }
    MVE_ASSERT(acquire_result.result == vk::Result::eSuccess, "[Renderer] Failed to acquire swapchain image")
    m_current_draw_state.image_index = acquire_result.value;

    vmaSetCurrentFrameIndex(m_vma_allocator, acquire_result.value);

    // ReSharper disable once CppExpressionWithoutSideEffects
    m_vk_device.resetFences({ frame.in_flight_fence }, m_vk_loader);

    frame.command_buffer.reset(vk::CommandBufferResetFlags(), m_vk_loader);

    while (!frame.funcs.empty()) {
        auto& [function, counter] = m_deferred_functions.at(frame.funcs.front());
        std::invoke(function, m_current_draw_state.frame_index);
        counter--;
        if (counter <= 0) {
            m_deferred_functions.erase(frame.funcs.front());
        }
        frame.funcs.pop();
    }

    std::queue<uint32_t> continue_defer;
    while (!m_wait_frames_deferred_functions.empty()) {
        uint32_t id = m_wait_frames_deferred_functions.front();
        m_wait_frames_deferred_functions.pop();
        auto& [function, counter] = m_deferred_functions.at(id);
        counter--;
        if (counter <= 0) {
            std::invoke(function, m_current_draw_state.frame_index);
            m_deferred_functions.erase(id);
        }
        else {
            continue_defer.push(id);
        }
    }
    m_wait_frames_deferred_functions = std::move(continue_defer);

    for (size_t i = 0; i < m_deferred_descriptor_writes.size(); i++) {
        auto& [counter, data_type, data_handle, descriptor_handle, binding] = m_deferred_descriptor_writes[i];
        vk::DescriptorBufferInfo buffer_info;
        vk::DescriptorImageInfo image_info;
        vk::WriteDescriptorSet descriptor_write;
        switch (data_type) {
        case DescriptorBindingType::uniform_buffer:
            buffer_info
                = vk::DescriptorBufferInfo()
                      .setBuffer(m_frames_in_flight[m_current_draw_state.frame_index]
                                     .uniform_buffers[data_handle]
                                     ->buffer.vk_handle)
                      .setOffset(0)
                      .setRange(
                          m_frames_in_flight[m_current_draw_state.frame_index].uniform_buffers[data_handle]->size);

            descriptor_write
                = vk::WriteDescriptorSet()
                      .setDstSet(m_frames_in_flight[m_current_draw_state.frame_index]
                                     .descriptor_sets[descriptor_handle]
                                     ->vk_handle)
                      .setDstBinding(binding)
                      .setDstArrayElement(0)
                      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                      .setDescriptorCount(1)
                      .setPBufferInfo(&buffer_info);

            m_vk_device.updateDescriptorSets(1, &descriptor_write, 0, nullptr, m_vk_loader);
            break;
        case DescriptorBindingType::texture:
            image_info = vk::DescriptorImageInfo()
                             .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                             .setImageView(m_textures[data_handle].vk_image_view)
                             .setSampler(m_textures[data_handle].vk_sampler);

            descriptor_write
                = vk::WriteDescriptorSet()
                      .setDstSet(m_frames_in_flight[m_current_draw_state.frame_index]
                                     .descriptor_sets[descriptor_handle]
                                     ->vk_handle)
                      .setDstBinding(binding)
                      .setDstArrayElement(0)
                      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                      .setDescriptorCount(1)
                      .setPImageInfo(&image_info);

            m_vk_device.updateDescriptorSets(1, &descriptor_write, 0, nullptr, m_vk_loader);
            break;
        }
        counter--;
        if (counter <= 0) {
            m_deferred_descriptor_writes.erase(m_deferred_descriptor_writes.begin() + static_cast<long long>(i));
            i--;
        }
    }
    for (size_t i = 0; i < m_deferred_uniform_updates.size(); i++) {
        auto& [counter, handle, location, data, data_size] = m_deferred_uniform_updates[i];
        update_uniform(handle, location, data.data(), data_size, m_current_draw_state.frame_index);
        counter--;
        if (counter <= 0) {
            m_deferred_uniform_updates.erase(m_deferred_uniform_updates.begin() + static_cast<long long>(i));
            i--;
        }
    }

    m_current_draw_state.command_buffer = m_frames_in_flight[m_current_draw_state.frame_index].command_buffer;

    constexpr auto buffer_begin_info = vk::CommandBufferBeginInfo();
    const vk::Result begin_result = m_current_draw_state.command_buffer.begin(buffer_begin_info, m_vk_loader);
    MVE_ASSERT(begin_result == vk::Result::eSuccess, "[Renderer] Failed to begin command buffer recording")

    while (!m_command_buffer_deferred_functions.empty()) {
        std::invoke(m_command_buffer_deferred_functions.front(), m_current_draw_state.command_buffer);
        m_command_buffer_deferred_functions.pop();
    }
}

void Renderer::end_frame(const Window& window)
{
    const vk::Result end_result = m_current_draw_state.command_buffer.end(m_vk_loader);
    MVE_ASSERT(end_result == vk::Result::eSuccess, "[Renderer] Failed to end command buffer recording")

    // ReSharper disable once CppUseStructuredBinding
    const FrameInFlight& frame = m_frames_in_flight[m_current_draw_state.frame_index];

    vk::Semaphore wait_semaphores[] = { frame.image_available_semaphore };
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::Semaphore signal_semaphores[] = { frame.render_finished_semaphore };

    auto submit_info
        = vk::SubmitInfo()
              .setWaitSemaphores(wait_semaphores)
              .setWaitDstStageMask(wait_stages)
              .setCommandBufferCount(1)
              .setPCommandBuffers(&frame.command_buffer)
              .setSignalSemaphores(signal_semaphores);

    const vk::Result graphics_submit_result
        = m_vk_graphics_queue.submit({ submit_info }, frame.in_flight_fence, m_vk_loader);
    MVE_ASSERT(graphics_submit_result == vk::Result::eSuccess, "[Renderer] Failed to submit to graphics queue")

    vk::SwapchainKHR swapchains[] = { m_vk_swapchain };

    const auto present_info
        = vk::PresentInfoKHR()
              .setWaitSemaphores(signal_semaphores)
              .setSwapchains(swapchains)
              .setPImageIndices(&m_current_draw_state.image_index);

    if (const vk::Result present_result = m_vk_present_queue.presentKHR(present_info, m_vk_loader);
        present_result == vk::Result::eSuboptimalKHR || present_result == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain(window);
    }
    else {
        MVE_ASSERT(present_result == vk::Result::eSuccess, "[Renderer] Failed to present frame")
    }

    m_current_draw_state.frame_index = (m_current_draw_state.frame_index + 1) % c_frames_in_flight;

    m_current_draw_state.is_drawing = false;
}

Vector2i Renderer::extent() const
{
    return { static_cast<int>(m_vk_swapchain_extent.width), static_cast<int>(m_vk_swapchain_extent.height) };
}

void Renderer::wait_ready() const
{
    // ReSharper disable once CppUseStructuredBinding
    const FrameInFlight& frame = m_frames_in_flight[m_current_draw_state.frame_index];

    const vk::Result fence_wait_result
        = m_vk_device.waitForFences(frame.in_flight_fence, true, UINT64_MAX, m_vk_loader);
    MVE_ASSERT(fence_wait_result == vk::Result::eSuccess, "[Renderer] Failed waiting for frame (fences)")
}

void Renderer::update_uniform(
    const size_t handle,
    const UniformLocation location,
    const void* data_ptr,
    const size_t size,
    const uint32_t frame_index) const
{
    // ReSharper disable once CppUseStructuredBinding
    const UniformBufferImpl& buffer = *m_frames_in_flight.at(frame_index).uniform_buffers.at(handle);
    memcpy(&buffer.mapped_ptr[location.value()], data_ptr, size);
}

DescriptorSetLayoutHandleImpl Renderer::create_descriptor_set_layout(
    const vk::DispatchLoaderDynamic& loader,
    const uint32_t set,
    const Shader& vertex_shader,
    const Shader& fragment_shader)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    if (vertex_shader.has_descriptor_set(set)) {
        for (const ShaderDescriptorSet& vertex_set = vertex_shader.descriptor_set(set);
             const auto& [binding_num, shader_binding] : vertex_set.bindings()) {
            auto binding = vk::DescriptorSetLayoutBinding()
                               .setBinding(shader_binding.binding())
                               .setDescriptorCount(1)
                               .setPImmutableSamplers(nullptr);

            switch (shader_binding.type()) {
            case ShaderDescriptorType::uniform_buffer:
                binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
                binding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
                break;
            case ShaderDescriptorType::combined_image_sampler:
                binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                binding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
                break;
            }
            bindings.push_back(binding);
        }
    }
    if (fragment_shader.has_descriptor_set(set)) {
        for (const ShaderDescriptorSet& fragment_set = fragment_shader.descriptor_set(set);
             const auto& [binding_num, fragment_binding] : fragment_set.bindings()) {
            auto binding = vk::DescriptorSetLayoutBinding()
                               .setBinding(fragment_binding.binding())
                               .setDescriptorCount(1)
                               .setPImmutableSamplers(nullptr);

            switch (fragment_binding.type()) {
            case ShaderDescriptorType::uniform_buffer:
                binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
                binding.setStageFlags(vk::ShaderStageFlagBits::eFragment);
                break;
            case ShaderDescriptorType::combined_image_sampler:
                binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                binding.setStageFlags(vk::ShaderStageFlagBits::eFragment);
                break;
            }
            bindings.push_back(binding);
        }
    }

    const auto layout_info = vk::DescriptorSetLayoutCreateInfo().setBindings(bindings);

    const vk::ResultValue<vk::DescriptorSetLayout> descriptor_set_layout_result
        = m_vk_device.createDescriptorSetLayout(layout_info, nullptr, loader);
    MVE_ASSERT(
        descriptor_set_layout_result.result == vk::Result::eSuccess,
        "[Renderer] Failed to create descriptor set layout")
    vk::DescriptorSetLayout vk_layout = descriptor_set_layout_result.value;

    uint64_t handle = m_resource_handle_count;
    m_descriptor_set_layouts.insert({ handle, vk_layout });
    m_resource_handle_count++;

    log().debug("[Renderer] Descriptor set layout created with ID: {}", handle);

    return handle;
}

size_t Renderer::create_graphics_pipeline_layout(
    const vk::DispatchLoaderDynamic& loader, const Shader& vertex_shader, const Shader& fragment_shader)
{
    std::vector<DescriptorSetLayoutHandleImpl> layouts;
    std::unordered_map<uint64_t, DescriptorSetLayoutHandleImpl> descriptor_set_layouts;

    for (uint32_t i = 0; i <= 3; i++) {
        if (vertex_shader.has_descriptor_set(i) || fragment_shader.has_descriptor_set(i)) {
            DescriptorSetLayoutHandleImpl descriptor_set_layout
                = create_descriptor_set_layout(loader, i, vertex_shader, fragment_shader);
            layouts.push_back(descriptor_set_layout);
            descriptor_set_layouts.insert({ i, descriptor_set_layout });
        }
    }

    const vk::PipelineLayout vk_layout = create_vk_pipeline_layout(m_vk_loader, layouts);

    std::optional<size_t> id;
    for (size_t i = 0; i < m_graphics_pipeline_layouts.size(); i++) {
        if (!m_graphics_pipeline_layouts[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = m_graphics_pipeline_layouts.size();
        m_graphics_pipeline_layouts.emplace_back();
    }
    m_graphics_pipeline_layouts[*id]
        = { .vk_handle = vk_layout, .descriptor_set_layouts = std::move(descriptor_set_layouts) };

    log().debug("[Renderer] Graphics pipeline layout created with ID: {}", *id);

    return *id;
}

void Renderer::defer_to_all_frames(std::function<void(uint32_t)> func)
{
    uint32_t id = m_deferred_function_id_count;
    m_deferred_function_id_count++;
    m_deferred_functions.insert({ id, { std::move(func), c_frames_in_flight } });
    // ReSharper disable once CppUseStructuredBinding
    for (FrameInFlight& frame : m_frames_in_flight) {
        frame.funcs.push(id);
    }
}

void Renderer::defer_to_next_frame(std::function<void(uint32_t)> func)
{
    uint32_t id = m_deferred_function_id_count;
    m_deferred_function_id_count++;
    m_deferred_functions.insert({ id, { std::move(func), 1 } });
    m_frames_in_flight.at(m_current_draw_state.frame_index).funcs.push(id);
}

void Renderer::resize(const Window& window)
{
    recreate_swapchain(window);
}

void Renderer::defer_after_all_frames(std::function<void(uint32_t)> func)
{
    uint32_t id = m_deferred_function_id_count;
    m_deferred_function_id_count++;
    m_deferred_functions.insert({ id, { std::move(func), c_frames_in_flight } });
    m_wait_frames_deferred_functions.push(id);
}

void Renderer::defer_to_command_buffer_front(const std::function<void(vk::CommandBuffer)>& func)
{
    m_command_buffer_deferred_functions.push(func);
}

void Renderer::write_descriptor_binding(
    const DescriptorSet& descriptor_set, const ShaderDescriptorBinding& descriptor_binding, const Texture& texture)
{
    const DeferredDescriptorWriteData write_data {
        .counter = c_frames_in_flight,
        .data_type = DescriptorBindingType::texture,
        .data_handle = texture.handle(),
        .descriptor_handle = descriptor_set.handle(),
        .binding = descriptor_binding.binding()
    };
    m_deferred_descriptor_writes.push_back(write_data);
}

VertexBuffer Renderer::create_vertex_buffer(const VertexData& vertex_data)
{
    size_t buffer_size = get_vertex_layout_bytes(vertex_data.layout()) * vertex_data.vertex_count();

    MVE_VAL_ASSERT(buffer_size != 0, "[Renderer] Attempt to allocate empty vertex buffer")

    Buffer staging_buffer = create_buffer(
        m_vma_allocator,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void* data;
    vmaMapMemory(m_vma_allocator, staging_buffer.vma_allocation, &data);
    memcpy(data, vertex_data.data_ptr(), buffer_size);
    vmaUnmapMemory(m_vma_allocator, staging_buffer.vma_allocation);

    Buffer buffer = create_buffer(
        m_vma_allocator,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    defer_to_command_buffer_front([this, staging_buffer, buffer, buffer_size](const vk::CommandBuffer command_buffer) {
        cmd_copy_buffer(m_vk_loader, command_buffer, staging_buffer.vk_handle, buffer.vk_handle, buffer_size);

        const auto barrier
            = vk::BufferMemoryBarrier()
                  .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                  .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
                  .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                  .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                  .setBuffer(buffer.vk_handle)
                  .setOffset(0)
                  .setSize(buffer_size);

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexInput,
            {},
            0,
            nullptr,
            1,
            &barrier,
            0,
            nullptr,
            m_vk_loader);

        defer_to_next_frame([this, staging_buffer](uint32_t) {
            vmaDestroyBuffer(m_vma_allocator, staging_buffer.vk_handle, staging_buffer.vma_allocation);
        });
    });

    std::optional<size_t> id;
    for (size_t i = 0; i < m_vertex_buffers.size(); i++) {
        if (!m_vertex_buffers[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = m_vertex_buffers.size();
        m_vertex_buffers.emplace_back();
    }
    m_vertex_buffers[*id] = { buffer, vertex_data.vertex_count() };

    log().debug("[Renderer] Vertex buffer created with ID: {}", *id);

    return { *this, *id };
}

void Renderer::bind_vertex_buffer(const VertexBuffer& vertex_buffer) const
{
    constexpr vk::DeviceSize offset = 0;
    m_current_draw_state.command_buffer.bindVertexBuffers(
        0, 1, &m_vertex_buffers[vertex_buffer.handle()]->buffer.vk_handle, &offset, m_vk_loader);
}

IndexBuffer Renderer::create_index_buffer(const std::vector<uint32_t>& indices)
{
    size_t buffer_size = sizeof(uint32_t) * indices.size();

    Buffer staging_buffer = create_buffer(
        m_vma_allocator,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void* data;
    vmaMapMemory(m_vma_allocator, staging_buffer.vma_allocation, &data);
    memcpy(data, indices.data(), buffer_size);
    vmaUnmapMemory(m_vma_allocator, staging_buffer.vma_allocation);

    Buffer buffer = create_buffer(
        m_vma_allocator,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        {});

    defer_to_command_buffer_front([this, staging_buffer, buffer, buffer_size](const vk::CommandBuffer command_buffer) {
        cmd_copy_buffer(m_vk_loader, command_buffer, staging_buffer.vk_handle, buffer.vk_handle, buffer_size);

        const auto barrier
            = vk::BufferMemoryBarrier()
                  .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                  .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
                  .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                  .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                  .setBuffer(buffer.vk_handle)
                  .setOffset(0)
                  .setSize(buffer_size);

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexInput,
            {},
            0,
            nullptr,
            1,
            &barrier,
            0,
            nullptr,
            m_vk_loader);

        defer_to_next_frame([this, staging_buffer](uint32_t) {
            vmaDestroyBuffer(m_vma_allocator, staging_buffer.vk_handle, staging_buffer.vma_allocation);
        });
    });

    std::optional<size_t> id;
    for (size_t i = 0; i < m_index_buffers.size(); i++) {
        if (!m_index_buffers[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = m_index_buffers.size();
        m_index_buffers.emplace_back();
    }
    m_index_buffers[*id] = { buffer, indices.size() };

    log().debug("[Renderer] Index buffer created with ID: {}", *id);

    return { *this, *id };
}

void Renderer::draw_index_buffer(const IndexBuffer& index_buffer)
{
    auto& [buffer, index_count] = *m_index_buffers[index_buffer.handle()];
    m_current_draw_state.command_buffer.bindIndexBuffer(buffer.vk_handle, 0, vk::IndexType::eUint32, m_vk_loader);
    m_current_draw_state.command_buffer.drawIndexed(index_count, 1, 0, 0, 0, m_vk_loader);
}

GraphicsPipeline Renderer::create_graphics_pipeline(
    const Shader& vertex_shader,
    const Shader& fragment_shader,
    const VertexLayout& vertex_layout,
    const bool depth_test)
{
    const size_t layout = create_graphics_pipeline_layout(m_vk_loader, vertex_shader, fragment_shader);

    const vk::Pipeline vk_pipeline = create_vk_graphics_pipeline(
        m_vk_loader,
        m_vk_device,
        vertex_shader,
        fragment_shader,
        m_graphics_pipeline_layouts.at(layout)->vk_handle,
        m_vk_render_pass,
        vertex_layout,
        m_msaa_samples,
        depth_test);

    std::optional<size_t> id;
    for (size_t i = 0; i < m_graphics_pipelines.size(); i++) {
        if (!m_graphics_pipelines[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = m_graphics_pipelines.size();
        m_graphics_pipelines.emplace_back();
    }
    m_graphics_pipelines[*id] = { .layout = layout, .pipeline = vk_pipeline };

    log().debug("[Renderer] Graphics pipeline created with ID: {}", *id);

    return { *this, *id };
}

DescriptorSet Renderer::create_descriptor_set(
    const GraphicsPipeline& graphics_pipeline, const ShaderDescriptorSet& descriptor_set)
{
    std::vector<DescriptorSetImpl> descriptor_sets;
    descriptor_sets.reserve(c_frames_in_flight);

    const vk::DescriptorSetLayout layout = m_descriptor_set_layouts.at(
        m_graphics_pipeline_layouts.at(m_graphics_pipelines.at(graphics_pipeline.handle())->layout)
            ->descriptor_set_layouts.at(descriptor_set.set()));

    for (int i = 0; i < c_frames_in_flight; i++) {
        descriptor_sets.push_back(m_descriptor_set_allocator.create(m_vk_loader, m_vk_device, layout));
    }

    // ReSharper disable once CppUseStructuredBinding
    const FrameInFlight& ref_frame = m_frames_in_flight.at(0);
    std::optional<size_t> id;
    for (size_t i = 0; i < ref_frame.descriptor_sets.size(); i++) {
        if (!ref_frame.descriptor_sets[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = ref_frame.descriptor_sets.size();
        // ReSharper disable once CppUseStructuredBinding
        for (FrameInFlight& frame : m_frames_in_flight) {
            frame.descriptor_sets.emplace_back();
        }
    }
    int i = 0;
    // ReSharper disable once CppUseStructuredBinding
    for (FrameInFlight& frame : m_frames_in_flight) {
        frame.descriptor_sets[*id] = descriptor_sets.at(i);
        i++;
    }

    log().debug("[Renderer] Descriptor set created with ID: {}", *id);

    return { *this, *id };
}

void Renderer::bind_graphics_pipeline(const GraphicsPipeline& graphics_pipeline)
{
    if (m_current_draw_state.current_pipeline == graphics_pipeline.handle()) {
        return;
    }
    m_current_draw_state.command_buffer.bindPipeline(
        vk::PipelineBindPoint::eGraphics, m_graphics_pipelines.at(graphics_pipeline.handle())->pipeline, m_vk_loader);
    m_current_draw_state.current_pipeline = graphics_pipeline.handle();
}

void Renderer::write_descriptor_binding(
    const DescriptorSet& descriptor_set,
    const ShaderDescriptorBinding& descriptor_binding,
    const UniformBuffer& uniform_buffer)
{
    const DeferredDescriptorWriteData write_data {
        .counter = c_frames_in_flight,
        .data_type = DescriptorBindingType::uniform_buffer,
        .data_handle = uniform_buffer.handle(),
        .descriptor_handle = descriptor_set.handle(),
        .binding = descriptor_binding.binding()
    };
    m_deferred_descriptor_writes.push_back(write_data);
}

void Renderer::bind_descriptor_set(DescriptorSet& descriptor_set) const
{
    bind_descriptor_sets(1, { &descriptor_set, nullptr, nullptr, nullptr });
}

UniformBuffer Renderer::create_uniform_buffer(const ShaderDescriptorBinding& descriptor_binding)
{
    MVE_VAL_ASSERT(
        descriptor_binding.type() == ShaderDescriptorType::uniform_buffer,
        "[Renderer] Failed to create uniform buffer as binding is not of type uniform buffer")

    const uint32_t struct_size = descriptor_binding.block().size();

    // ReSharper disable once CppUseStructuredBinding
    const FrameInFlight& ref_frame = m_frames_in_flight.at(0);
    std::optional<size_t> id;
    for (size_t i = 0; i < ref_frame.uniform_buffers.size(); i++) {
        if (!ref_frame.uniform_buffers[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = ref_frame.uniform_buffers.size();
        // ReSharper disable once CppUseStructuredBinding
        for (FrameInFlight& frame : m_frames_in_flight) {
            frame.uniform_buffers.emplace_back();
        }
    }
    int i = 0;
    // ReSharper disable once CppUseStructuredBinding
    for (FrameInFlight& frame : m_frames_in_flight) {
        const Buffer buffer = create_buffer(
            m_vma_allocator,
            struct_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        void* ptr;
        vmaMapMemory(m_vma_allocator, buffer.vma_allocation, &ptr);
        frame.uniform_buffers[*id] = { buffer, struct_size, static_cast<std::byte*>(ptr) };
        i++;
    }

    log().debug("[Renderer] Uniform buffer created with ID: {}", *id);

    return { *this, *id };
}

void Renderer::update_uniform(
    UniformBuffer& uniform_buffer, const UniformLocation location, const float value, const bool persist)
{
    update_uniform<float>(uniform_buffer, location, value, persist);
}
void Renderer::update_uniform(
    UniformBuffer& uniform_buffer, const UniformLocation location, const Vector2 value, const bool persist)
{
    update_uniform<Vector2>(uniform_buffer, location, value, persist);
}
void Renderer::update_uniform(
    UniformBuffer& uniform_buffer, const UniformLocation location, const Vector3 value, const bool persist)
{
    update_uniform<Vector3>(uniform_buffer, location, value, persist);
}
void Renderer::update_uniform(
    UniformBuffer& uniform_buffer, const UniformLocation location, const Vector4 value, const bool persist)
{
    update_uniform<Vector4>(uniform_buffer, location, value, persist);
}
void Renderer::update_uniform(
    UniformBuffer& uniform_buffer, const UniformLocation location, const Matrix3& value, const bool persist)
{
    update_uniform<Matrix3>(uniform_buffer, location, value, persist);
}
void Renderer::update_uniform(
    UniformBuffer& uniform_buffer, const UniformLocation location, const Matrix4& value, const bool persist)
{
    update_uniform<Matrix4>(uniform_buffer, location, value, persist);
}

void Renderer::destroy(Texture& texture)
{
    MVE_VAL_ASSERT(texture.is_valid(), "[Renderer] Attempted to destroy invalid texture")
    log().debug("[Renderer] Destroyed texture with ID: {}", texture.handle());
    uint64_t handle = texture.handle();
    texture.invalidate();
    defer_after_all_frames([this, handle](uint32_t) {
        auto& [image, vk_image_view, vk_sampler, mip_levels] = m_textures.at(handle);
        m_vk_device.destroy(vk_sampler, nullptr, m_vk_loader);
        m_vk_device.destroy(vk_image_view, nullptr, m_vk_loader);
        vmaDestroyImage(m_vma_allocator, image.vk_handle, image.vma_allocation);
        m_textures.erase(handle);
    });
}

// TODO: mip-mapping
Texture Renderer::create_texture(const TextureFormat format, uint32_t width, uint32_t height, const std::byte* data)
{
    MVE_VAL_ASSERT(width != 0 && height != 0, "[Renderer] Attempt to create texture with 0 width or height")
    uint32_t mip_levels = 1;

    vk::Format vk_format {};
    size_t size = width * height * sizeof(std::byte);
    switch (format) {
    case TextureFormat::r:
        vk_format = vk::Format::eR8Unorm;
        break;
    case TextureFormat::rg:
        vk_format = vk::Format::eR8G8Unorm;
        size *= 2;
        break;
    case TextureFormat::rgb:
        vk_format = vk::Format::eR8G8B8Unorm;
        size *= 3;
        break;
    case TextureFormat::rgba:
        vk_format = vk::Format::eR8G8B8A8Unorm;
        size *= 4;
        break;
    }

    Buffer staging_buffer = create_buffer(
        m_vma_allocator,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void* data_ptr;
    vmaMapMemory(m_vma_allocator, staging_buffer.vma_allocation, &data_ptr);
    memcpy(data_ptr, data, size);
    vmaUnmapMemory(m_vma_allocator, staging_buffer.vma_allocation);

    Image image = create_image(
        m_vma_allocator,
        width,
        height,
        mip_levels,
        vk::SampleCountFlagBits::e1,
        vk_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        false);

    defer_to_command_buffer_front([this, image, mip_levels, staging_buffer, width, height, vk_format](
                                      const vk::CommandBuffer command_buffer) {
        cmd_transition_image_layout(
            m_vk_loader,
            command_buffer,
            image.vk_handle,
            vk_format,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            mip_levels);

        cmd_copy_buffer_to_image(m_vk_loader, command_buffer, staging_buffer.vk_handle, image.vk_handle, width, height);

        cmd_generate_mipmaps(
            m_vk_loader, m_vk_physical_device, command_buffer, image.vk_handle, vk_format, width, height, mip_levels);

        defer_to_next_frame([this, staging_buffer](uint32_t) {
            vmaDestroyBuffer(m_vma_allocator, staging_buffer.vk_handle, staging_buffer.vma_allocation);
        });
    });

    const vk::ImageView image_view = create_image_view(
        m_vk_loader, m_vk_device, image.vk_handle, vk_format, vk::ImageAspectFlagBits::eColor, mip_levels);

    const vk::Sampler sampler = create_texture_sampler(m_vk_loader, m_vk_physical_device, m_vk_device, mip_levels);

    TextureImpl texture {
        .image = image, .vk_image_view = image_view, .vk_sampler = sampler, .mip_levels = mip_levels
    };

    auto handle = m_resource_handle_count;
    m_resource_handle_count++;
    m_textures.insert({ handle, texture });

    log().debug("[Renderer] Texture created with ID: {}", handle);

    return { *this, handle };
}

Texture Renderer::create_texture(const std::filesystem::path& path)
{
    int width;
    int height;
    int channels;
    const std::string path_string = path.string();
    const stbi_uc* pixels = stbi_load(path_string.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    // vk::DeviceSize size = width * height * 4;
    MVE_ASSERT(pixels != nullptr, "[Renderer] Failed to load texture image")

    return create_texture(
        TextureFormat::rgba,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        reinterpret_cast<const std::byte*>(pixels));
}

Texture Renderer::create_texture(
    const Image& image, const vk::ImageView image_view, const vk::Sampler sampler, const uint32_t mip_levels)
{
    TextureImpl texture {
        .image = image, .vk_image_view = image_view, .vk_sampler = sampler, .mip_levels = mip_levels
    };

    auto handle = m_resource_handle_count;
    m_resource_handle_count++;
    m_textures.insert({ handle, texture });

    log().debug("[Renderer] Texture created with ID: {}", handle);

    return { *this, handle };
}

void Renderer::draw_vertex_buffer(const VertexBuffer& vertex_buffer)
{
    auto& [buffer, vertex_count] = *m_vertex_buffers.at(vertex_buffer.handle());
    m_current_draw_state.command_buffer.bindVertexBuffers(0, buffer.vk_handle, { 0 });
    m_current_draw_state.command_buffer.draw(vertex_count, 1, 0, 0);
}

void Renderer::destroy(DescriptorSet& descriptor_set)
{
    MVE_VAL_ASSERT(descriptor_set.is_valid(), "[Renderer] Attempted to destroy invalid descriptor set")
    log().debug("[Renderer] Destroyed descriptor set with ID: {}", descriptor_set.handle());
    uint64_t handle = descriptor_set.handle();
    descriptor_set.invalidate();
    defer_after_all_frames([this, handle](uint32_t) {
        std::vector<DescriptorSetImpl> sets_to_delete;
        std::ranges::transform(
            std::as_const(m_frames_in_flight), std::back_inserter(sets_to_delete), [&](const FrameInFlight& frame) {
                return *frame.descriptor_sets.at(handle);
            });
        // ReSharper disable once CppUseStructuredBinding
        for (FrameInFlight& frame : m_frames_in_flight) {
            frame.descriptor_sets[handle].reset();
        }
        for (DescriptorSetImpl set : sets_to_delete) {
            m_descriptor_set_allocator.free(m_vk_loader, m_vk_device, set);
        }
    });
}

void Renderer::destroy(GraphicsPipeline& graphics_pipeline)
{
    MVE_VAL_ASSERT(graphics_pipeline.is_valid(), "[Renderer] Attempted to destroy invalid graphics pipeline")
    log().debug("[Renderer] Destroyed graphics pipeline with ID: {}", graphics_pipeline.handle());
    size_t handle = graphics_pipeline.handle();
    graphics_pipeline.invalidate();
    defer_after_all_frames([this, handle](uint32_t) {
        // Descriptor set layouts
        std::vector<DescriptorSetLayoutHandleImpl> deleted_descriptor_set_layout_handles;
        for (auto& [set, set_layout] :
             m_graphics_pipeline_layouts.at(m_graphics_pipelines.at(handle)->layout)->descriptor_set_layouts) {
            m_vk_device.destroy(m_descriptor_set_layouts.at(set_layout), nullptr, m_vk_loader);
            deleted_descriptor_set_layout_handles.push_back(set_layout);
        }
        for (DescriptorSetLayoutHandleImpl descriptor_set_layout_handle : deleted_descriptor_set_layout_handles) {
            m_descriptor_set_layouts.erase(descriptor_set_layout_handle);
        }

        // Pipeline layout
        m_vk_device.destroy(
            m_graphics_pipeline_layouts.at(m_graphics_pipelines.at(handle)->layout)->vk_handle, nullptr, m_vk_loader);
        m_graphics_pipeline_layouts[m_graphics_pipelines.at(handle)->layout].reset();

        // Graphics pipeline
        m_vk_device.destroy(m_graphics_pipelines.at(handle)->pipeline, nullptr, m_vk_loader);
        m_graphics_pipelines[handle].reset();
    });
}

void Renderer::destroy(UniformBuffer& uniform_buffer)
{
    MVE_VAL_ASSERT(uniform_buffer.is_valid(), "[Renderer] Attempted to destroy invalid uniform buffer")
    log().debug("[Renderer] Destroyed uniform buffer with ID: {}", uniform_buffer.handle());
    uniform_buffer.invalidate();
    size_t handle = uniform_buffer.handle();
    defer_after_all_frames([this, handle](uint32_t) {
        // ReSharper disable once CppUseStructuredBinding
        for (const FrameInFlight& frame : m_frames_in_flight) {
            auto [buffer, size, mapped_ptr] = *frame.uniform_buffers.at(handle);
            vmaUnmapMemory(m_vma_allocator, buffer.vma_allocation);
            vmaDestroyBuffer(m_vma_allocator, buffer.vk_handle, buffer.vma_allocation);
        }
        // ReSharper disable once CppUseStructuredBinding
        for (FrameInFlight& frame : m_frames_in_flight) {
            frame.uniform_buffers[handle].reset();
        }
    });
}

void Renderer::destroy(IndexBuffer& index_buffer)
{
    MVE_VAL_ASSERT(index_buffer.is_valid(), "[Renderer] Attempted to destroy invalid index buffer")
    log().debug("[Renderer] Destroyed index buffer with ID: {}", index_buffer.handle());
    uint64_t handle = index_buffer.handle();
    index_buffer.invalidate();
    defer_after_all_frames([this, handle](uint32_t) {
        vmaDestroyBuffer(
            m_vma_allocator,
            m_index_buffers.at(handle)->buffer.vk_handle,
            m_index_buffers.at(handle)->buffer.vma_allocation);
        m_index_buffers[handle].reset();
    });
}

void Renderer::bind_descriptor_sets(const DescriptorSet& descriptor_set_a, const DescriptorSet& descriptor_set_b) const
{
    bind_descriptor_sets(2, { &descriptor_set_a, &descriptor_set_b, nullptr, nullptr });
}

void Renderer::bind_descriptor_sets(
    const uint32_t num, const std::array<const DescriptorSet*, 4>& descriptor_sets) const
{
    std::array<vk::DescriptorSet, 4> sets;
    for (uint32_t i = 0; i < num; i++) {
        sets[i] = m_frames_in_flight[m_current_draw_state.frame_index]
                      .descriptor_sets[descriptor_sets[i]->handle()]
                      ->vk_handle;
    }

    m_current_draw_state.command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        m_graphics_pipeline_layouts[m_graphics_pipelines[m_current_draw_state.current_pipeline]->layout]->vk_handle,
        0,
        num,
        sets.data(),
        0,
        nullptr,
        m_vk_loader);
}

void Renderer::end_render_pass_present() const
{
    m_current_draw_state.command_buffer.endRenderPass(m_vk_loader);
}
Framebuffer Renderer::create_framebuffer(std::function<void()> callback)
{
    std::optional<size_t> id;
    for (size_t i = 0; i < m_framebuffers.size(); i++) {
        if (!m_framebuffers[i].has_value()) {
            id = i;
            break;
        }
    }
    if (!id.has_value()) {
        id = m_framebuffers.size();
        m_framebuffers.emplace_back();
    }
    m_framebuffers[*id] = std::move(create_framebuffer_impl(m_vk_loader, callback));

    log().debug("[Renderer] Framebuffer created with ID: {}", *id);

    return { *this, *id };
}
void Renderer::destroy(Framebuffer& framebuffer)
{
    MVE_VAL_ASSERT(framebuffer.is_valid(), "[Renderer] Attempted to destroy invalid framebuffer")
    log().debug("[Renderer] Destroyed framebuffer with ID: {}", framebuffer.handle());
    size_t handle = framebuffer.handle();
    framebuffer.invalidate();
    defer_after_all_frames([this, handle](uint32_t) {
        auto& [vk_framebuffers, texture, callback, size] = m_framebuffers.at(handle).value();
        m_textures.erase(handle);
        for (const vk::Framebuffer& buffer : vk_framebuffers) {
            m_vk_device.destroy(buffer, nullptr, m_vk_loader);
        }
        m_framebuffers.at(handle).reset();
    });
}
void Renderer::recreate_framebuffers()
{
    std::vector<std::pair<size_t, std::optional<std::function<void()>>>> ids_to_recreate;
    for (size_t i = 0; i < m_framebuffers.size(); i++) {
        if (m_framebuffers[i].has_value()) {
            ids_to_recreate.emplace_back(i, m_framebuffers[i]->callback);
            for (const vk::Framebuffer& buffer : m_framebuffers[i]->vk_framebuffers) {
                m_vk_device.destroy(buffer, nullptr, m_vk_loader);
            }
        }
    }
    for (auto& [id, callback] : ids_to_recreate) {
        m_framebuffers[id] = std::move(create_framebuffer_impl(m_vk_loader, callback));
    }
    for (const std::optional<FramebufferImpl>& framebuffer : m_framebuffers) {
        std::invoke(*framebuffer->callback);
    }
}

FramebufferImpl Renderer::create_framebuffer_impl(
    const vk::DispatchLoaderDynamic& loader, std::optional<std::function<void()>> callback)
{
    auto [image, vk_image_view] = create_color_image(
        m_vk_loader,
        m_vk_device,
        m_vma_allocator,
        m_vk_swapchain_extent,
        m_vk_swapchain_image_format.format,
        vk::SampleCountFlagBits::e1);

    std::vector<vk::Framebuffer> framebuffers;
    framebuffers.reserve(m_vk_swapchain_framebuffers.size());

    for (size_t i = 0; i < m_vk_swapchain_framebuffers.size(); i++) {

        std::array<vk::ImageView, 3> attachments;
        if (m_msaa_samples == vk::SampleCountFlagBits::e1) {
            attachments = { vk_image_view, m_depth_image.vk_image_view };
        }
        else {
            attachments = { m_color_image.vk_image_view, m_depth_image.vk_image_view, vk_image_view };
        }

        auto framebuffer_info
            = vk::FramebufferCreateInfo()
                  .setRenderPass(m_vk_render_pass)
                  .setAttachmentCount(m_msaa_samples == vk::SampleCountFlagBits::e1 ? 2 : 3)
                  .setPAttachments(attachments.data())
                  .setWidth(m_vk_swapchain_extent.width)
                  .setHeight(m_vk_swapchain_extent.height)
                  .setLayers(1);

        vk::ResultValue<vk::Framebuffer> framebuffer_result
            = m_vk_device.createFramebuffer(framebuffer_info, nullptr, loader);
        MVE_ASSERT(framebuffer_result.result == vk::Result::eSuccess, "[Renderer] Failed to create framebuffer")
        framebuffers.push_back(framebuffer_result.value);
    }

    const vk::Sampler sampler = create_texture_sampler(m_vk_loader, m_vk_physical_device, m_vk_device, 1);

    Texture texture = create_texture(image, vk_image_view, sampler, 1);

    FramebufferImpl framebuffer_impl {
        .vk_framebuffers = std::move(framebuffers),
        .texture = std::move(texture),
        .callback = std::move(callback),
        .size = { static_cast<int>(m_vk_swapchain_extent.width), static_cast<int>(m_vk_swapchain_extent.height) }
    };

    return framebuffer_impl;
}

void Renderer::end_render_pass_framebuffer() const
{
    m_current_draw_state.command_buffer.endRenderPass(m_vk_loader);
    //
    //    auto subresource_range
    //        = vk::ImageSubresourceRange()
    //              .setAspectMask(vk::ImageAspectFlagBits::eColor)
    //              .setBaseArrayLayer(0)
    //              .setLayerCount(1)
    //              .setLevelCount(1);
    //
    //    auto barrier
    //        = vk::ImageMemoryBarrier()
    //              .setImage(m_textures[framebuffer.texture().m_handle].image.vk_handle)
    //              .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    //              .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    //              .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
    //              .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
    //              .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
    //              .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
    //              .setSubresourceRange(subresource_range);
    //
    //    m_current_draw_state.command_buffer.pipelineBarrier(
    //        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    //        vk::PipelineStageFlagBits::eFragmentShader,
    //        {},
    //        0,
    //        nullptr,
    //        0,
    //        nullptr,
    //        1,
    //        &barrier);
}
const Texture& Renderer::framebuffer_texture(const Framebuffer& framebuffer)
{
    return m_framebuffers[framebuffer.handle()]->texture;
}
Vector2i Renderer::framebuffer_size(const Framebuffer& framebuffer) const
{
    return m_framebuffers[framebuffer.handle()]->size;
}
std::string Renderer::gpu_name() const
{
    return m_vk_physical_device.getProperties(m_vk_loader).deviceName;
}

Vector2i Renderer::texture_size(const Texture& texture) const
{
    MVE_VAL_ASSERT(texture.m_valid, "[Renderer] Attempt to get size on invalid texture")
    const auto& [image, vk_image_view, vk_sampler, mip_levels] = m_textures.at(texture.handle());
    return { static_cast<int>(image.width), static_cast<int>(image.height) };
}

}