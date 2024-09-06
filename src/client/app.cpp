#include "app.hpp"

#include <fstream>

#include "options.hpp"
#include <ThreadedLoggerForCPP/LoggerThread.hpp>
#include <game_performance_profiler.hpp>

#include <ThreadedLoggerForCPP/LoggerFileSystem.hpp>
#include <ThreadedLoggerForCPP/LoggerGlobals.hpp>
std::string _exe_game = "VoxelVerse";

void InitThreadedLoggerForCPP(std::string& ProjectDirectory, std::string& LogFileName, std::string& GameSaveFolder)
{
#pragma warning(push)
#pragma warning(disable : 4996) // Disable warning for getenv
#ifdef _WIN32
    LoggerGlobals::UsernameDirectory = std::getenv("USERNAME");
#else
    LoggerGlobals::UsernameDirectory = std::getenv("USER");
#endif
#pragma warning(pop)

    // this is the folder that contains your src files like main.cpp
    LoggerGlobals::SrcProjectDirectory = ProjectDirectory;
    // Create Log File and folder
    LoggerGlobals::LogFolderPath
        = "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." + GameSaveFolder + "\\logging\\";
    LoggerGlobals::LogFilePath = "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." + GameSaveFolder
        + "\\logging\\" + LogFileName + ".log";
    LoggerGlobals::LogFolderBackupPath
        = "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." + GameSaveFolder + "\\logging\\LogBackup";
    LoggerGlobals::LogFileBackupPath = "C:\\Users\\" + LoggerGlobals::UsernameDirectory + "\\." + GameSaveFolder
        + "\\logging\\LogBackup\\" + LogFileName + "-";

    LoggerThread::GetLoggerThread().StartLoggerThread(
        LoggerGlobals::LogFolderPath,
        LoggerGlobals::LogFilePath,
        LoggerGlobals::LogFolderBackupPath,
        LoggerGlobals::LogFileBackupPath);
}
namespace app {

App::App()
    : m_window("Voxelverse", nnm::Vector2i(800, 600))
    , m_renderer(m_window, "Voxelverse", 0, 1, 0)
    , m_server(false)
    , m_client(enet_host_create(
          nullptr, // client
          1, // outgoing connections
          2, // channels to be used
          0, // incoming bandwith
          0)) // outgoing bandwith
    , m_ui_pipeline(m_renderer)
    , m_text_pipeline(m_renderer, 36)
    , m_world(m_renderer, m_ui_pipeline, m_text_pipeline, 32)
    , m_world_framebuffer(m_renderer.create_framebuffer([this] {
        m_ui_pipeline.update_framebuffer_texture(
            m_world_framebuffer.texture(), m_renderer.framebuffer_size(m_world_framebuffer));
    }))
    , m_fixed_loop(60.0f)
    , m_begin_time(std::chrono::high_resolution_clock::now())
{
    InitThreadedLoggerForCPP(_exe_game, _exe_game, _exe_game);
    m_window.set_min_size({ 800, 600 });
    m_window.disable_cursor();

    auto resize_func = [&](nnm::Vector2i) {
        m_renderer.resize(m_window);
        m_world.resize(m_renderer.extent());
        m_ui_pipeline.resize();
        m_text_pipeline.resize();
        draw();
    };

    m_window.set_resize_callback(resize_func);

    std::invoke(resize_func, m_window.size());

    auto [fullscreen, msaa] = load_options();
    if (fullscreen) {
        m_window.fullscreen(true);
    }
    else {
        m_window.windowed();
    }
    m_renderer.set_msaa_samples(m_window, msaa);

    ENetAddress address;
    enet_address_set_host_ip(&address, "127.0.0.1");
    address.port = c_server_port;
    ENetPeer* peer = enet_host_connect(m_client, &address, 2, 0);
    VV_REL_ASSERT(peer != nullptr, "[App] No available peers for initiating an ENet connection");
    ENetEvent event;
    if (enet_host_service(m_client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        LOGGER_THREAD(LogLevel::INFO, "[App] Connected to server")
    }
    else {
        enet_peer_reset(peer);
        LOGGER_THREAD(LogLevel::ERRORING, "[App] Failed to connect to server;")
    }
    enet_host_flush(m_client);
}

App::~App()
{
    enet_host_flush(m_client);
    enet_host_destroy(m_client);
    enet_deinitialize();
    gamePerformanceProfiler.print();
}

void App::draw()
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    m_renderer.begin_frame(m_window);

    m_renderer.begin_render_pass_framebuffer(m_world_framebuffer);

    m_world.draw();

    m_renderer.end_render_pass();

    m_renderer.begin_render_pass_present();

    m_ui_pipeline.draw_world();

    m_renderer.end_render_pass();

    m_renderer.end_frame(m_window);
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void App::main_loop()
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    while (!m_window.should_close() && !m_world.should_exit()) {
        handle_networking();

        m_window.poll_events();

        m_fixed_loop.update(5, [&] { m_world.fixed_update(m_window); });

        m_world.update(m_window, m_fixed_loop.blend(), m_renderer);

        if (m_window.is_key_pressed(mve::Key::enter) && m_window.is_key_down(mve::Key::left_alt)) {
            if (!m_window.is_fullscreen()) {
                m_window.fullscreen(true);
            }
            else {
                m_window.windowed();
            }
        }

        m_world.update_debug_fps(m_frame_count);

        draw();

        if (std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - m_begin_time).count() >= 1000000) {
            m_begin_time = std::chrono::high_resolution_clock::now();
            m_frame_count = m_current_frame_count;
            m_current_frame_count = 0;
        }
        m_current_frame_count++;
    }
    LoggerThread::GetLoggerThread().ExitLoggerThread();
    const Options options { .fullscreen = m_window.is_fullscreen(), .msaa = m_renderer.current_msaa_samples() };
    set_options(options);
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void App::handle_networking() const
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    ENetEvent event;
    std::string buffer;
    while (enet_host_service(m_client, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_DISCONNECT:
            LOGGER_THREAD(LogLevel::INFO, "[App] Disconnected from server")
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            buffer = std::string(event.packet->data, event.packet->data + event.packet->dataLength);
            LOGGER_THREAD(LogLevel::INFO, "[App] Receieved packet: {}" + buffer)
            enet_packet_destroy(event.packet);
            break;
        default:
            break;
        }
    }
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

}
