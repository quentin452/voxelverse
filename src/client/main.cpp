#include <filesystem>

#include "app.hpp"
#include <game_performance_profiler.hpp>

int main()
{
    PROFILE_START(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    if (!std::filesystem::exists("save")) {
        const bool result = std::filesystem::create_directory("save");
        VV_REL_ASSERT(result, "[Main] Failed to create save dir")
    }

    //    try {
    app::App instance;
    instance.main_loop();
    //    }
    //    catch (const std::exception& e) {
    //         LOGGER_THREAD(LogLevel::ERROR,string(e.what()))
    //          PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    //        return EXIT_FAILURE;
    //    }
    PROFILE_STOP(std::string("VOXELVERSE:") + ":" + __FUNCTION__)
    return EXIT_SUCCESS;
}