#include "server.hpp"

#include <sstream>

#include "../common/assert.hpp"

#include <ThreadedLoggerForCPP/LoggerThread.hpp>

#include <ThreadedLoggerForCPP/LoggerFileSystem.hpp>
#include <ThreadedLoggerForCPP/LoggerGlobals.hpp>

#include <game_performance_profiler.hpp>

Server::Server(const bool cleanup_enet)
    : m_cleanup_enet(cleanup_enet)
    , m_exit(false)
    , m_server(nullptr)
{
    VV_REL_ASSERT(enet_initialize() == 0, "[Server] Failed to initialize ENet");

    constexpr ENetAddress address { .host = ENET_HOST_ANY, .port = c_server_port };
    m_server = enet_host_create(
        &address,
        7, // clients and/or outgoing connections
        2, // max channels to be used
        0, // incoming bandwith
        0); // outoing bandwith
    VV_REL_ASSERT(m_server != nullptr, "[Server] Unable to create ENetHost");
    m_thread = std::thread([this] { this->start(); });
    LOGGER_THREAD(LogLevel::INFO, "[Server] Started")
}

Server::~Server()
{
    m_exit = true;
    m_thread.join();
    enet_host_destroy(m_server);
    if (m_cleanup_enet) {
        enet_deinitialize();
    }
}

static std::string host_ip_to_string(uint32_t host)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    uint32_t ip[4];
    ip[0] = host & 0xff;
    host >>= 8;
    ip[1] = host & 0xff;
    host >>= 8;
    ip[2] = host & 0xff;
    host >>= 8;
    ip[3] = host & 0xff;
    std::stringstream ss;
    ss << static_cast<int>(ip[0]) << "." << static_cast<int>(ip[1]) << "." << static_cast<int>(ip[2]) << "."
       << static_cast<int>(ip[3]);
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    return ss.str();
}

static void send_hello_packet(ENetPeer* peer)
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    const std::string msg = "Hello World!";
    ENetPacket* packet = enet_packet_create(msg.data(), msg.length(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}

void Server::start() const
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    ENetEvent event;
    while (!m_exit) {
        while (enet_host_service(m_server, &event, 1000) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                std::string clientInfo
                    = host_ip_to_string(event.peer->address.host) + ":" + std::to_string(event.peer->address.port);
                LOGGER_THREAD(LogLevel::INFO, "[Server] Client connected from " + clientInfo)
            }
                send_hello_packet(event.peer);
                break;
            case ENET_EVENT_TYPE_DISCONNECT: {
                std::string clientInfo
                    = host_ip_to_string(event.peer->address.host) + ":" + std::to_string(event.peer->address.port);
                LOGGER_THREAD(LogLevel::INFO, "[Server] Client disconnected from " + clientInfo)
            } break;
            case ENET_EVENT_TYPE_RECEIVE:
            default:
                break;
            }
        }
    }
    LOGGER_THREAD(LogLevel::INFO, "[Server] Stopping")
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
}
