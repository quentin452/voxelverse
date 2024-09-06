cmake_minimum_required(VERSION 3.5)

project(voxelverse)

set(CMAKE_CXX_STANDARD 20)

if(PERFORMANCE_MONITOR)
    add_definitions(-DPERFORMANCE_MONITOR)
endif()

function(add_shaders TARGET)
    find_program(GLSLANGVALIDATOR glslangValidator)
    foreach (SHADER ${ARGN})
        set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/src/client/shader/${SHADER})
        set(current-output-path ${CMAKE_CURRENT_SOURCE_DIR}/res/bin/shader/${SHADER}.spv)

        # Add a custom command to compile GLSL to SPIR-V using glslangvalidator.
        get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
        file(MAKE_DIRECTORY ${current-output-dir})

        add_custom_command(
                OUTPUT ${current-output-path}
                COMMAND ${GLSLANGVALIDATOR} -V -o ${current-output-path} ${current-shader-path}
                DEPENDS ${current-shader-path}
                IMPLICIT_DEPENDS CXX ${current-shader-path}
                VERBATIM)

        # Make sure our build depends on this output.
        set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
        target_sources(${TARGET} PRIVATE ${current-output-path})
    endforeach ()
endfunction()

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LEVELDB_INSTALL OFF CACHE BOOL "" FORCE)


add_subdirectory(external/freetype-2.13.2 SYSTEM)
add_subdirectory(external/leveldb-1.23 SYSTEM)
add_subdirectory(external/enet-1.3.18 SYSTEM)

set(LIB_SOURCE_FILES
        external/whereami-ba364cd/src/whereami.c
        external/lz4-1.9.4/src/lz4.c
        external/lz4-1.9.4/src/lz4hc.c)

add_subdirectory(lib/mve)

file(GLOB_RECURSE SOURCE_FILES "src/**/*.cpp"
"${CMAKE_SOURCE_DIR}/external/ThreadedLoggerForCPPV0.5.2/src/LoggerThread.cpp")

set(LIBS
        enet
        freetype
        leveldb
        mve)
if (WIN32)
    list(APPEND LIBS winmm.lib Ws2_32.lib)
endif ()

set(LIB_INCLUDES
"${CMAKE_SOURCE_DIR}/external/game_performance_profiler"
"${CMAKE_SOURCE_DIR}/external/spdlog-1.14.1/include"
"${CMAKE_SOURCE_DIR}/external/whereami-ba364cd/include"
"${CMAKE_SOURCE_DIR}/external/tinyobjloader-45f683c/include"
"${CMAKE_SOURCE_DIR}/external/fast-noise-lite-1.1.1/include"
"${CMAKE_SOURCE_DIR}/external/cereal-1.3.2/include"
"${CMAKE_SOURCE_DIR}/external/lz4-1.9.4/include"
"${CMAKE_SOURCE_DIR}/external/thread-pool-4.1.0/include"
"${CMAKE_SOURCE_DIR}/external/nlohmann-json-3.11.3/include"
"${CMAKE_SOURCE_DIR}/external/enet-1.3.18/include"
"${CMAKE_SOURCE_DIR}/external/ThreadedLoggerForCPPV0.5.2/libs/include")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
add_executable(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PUBLIC RES_PATH="./res")

if (WIN32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -stdlib=libc++ -lc++abi")
    endif ()
endif ()

target_sources(${PROJECT_NAME} PRIVATE
        ${LIB_SOURCE_FILES}
        ${SOURCE_FILES}
        src/client/main.cpp)

target_link_libraries(${PROJECT_NAME} ${LIBS})

target_include_directories(${PROJECT_NAME} PRIVATE ${LIB_INCLUDES})

add_shaders(voxelverse
        simple.frag
        simple.vert
        color.frag
        color.vert
        ui.frag
        ui.vert
        text.vert
        text.frag)

#set(TEST_LIB_SOURCE_FILES
#        external/catch2-3.3.2/src/catch_amalgamated.cpp)
#
#set(TEST_SOURCE_FILES
#        tests/vertex_data.cpp)
#
#set(TEST_LIB_INCLUDES
#        external/catch2-3.3.2/include)
#
#add_executable(tests)
#
#target_compile_definitions(tests PUBLIC RES_PATH="../res")
#
#target_sources(tests PRIVATE
#        ${LIB_SOURCE_FILES}
#        ${SOURCE_FILES}
#        ${TEST_LIB_SOURCE_FILES}
#        ${TEST_SOURCE_FILES})
#
#target_link_libraries(tests ${LIBS})
#
#target_include_directories(tests PRIVATE
#        ${LIB_INCLUDES}
#        ${TEST_LIB_INCLUDES})

# Ajouter une cible personnalisée pour copier le dossier après la construction
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
    COMMENT "Copying resource directory to build directory"
)

set(SOURCES
    ${SOURCE_FILES}
    ${LIB_SOURCE_FILES}
    ${CMAKE_SOURCE_DIR}/src/client/main.cpp
)

# Common compilation options
set(ALL_INCLUDE_DIR
    ${LIB_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src
)
set(common_compile_options
   -std=c++${CMAKE_CXX_STANDARD}
   -Wall 
   -compilerandversion=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}
   -Wmicrosoft 
   -Wno-invalid-token-paste 
   -Wno-unknown-pragmas 
   -Wno-unused-value 
   -fsyntax-only 
   "\\\"-D_MT\\\"" 
   "\\\"-D_DLL\\\"" 
   "\\\"-DWIN32\\\"" 
   "\\\"-D_WINDOWS\\\"" 
   "\\\"-DCMAKE_INTDIR=/\\\"Debug/\\\"\\\"" 
   "\\\"-D_DEBUG_FUNCTIONAL_MACHINERY\\\""   
)

# Add each include directory to the compilation options
foreach(INCLUDE_DIR ${ALL_INCLUDE_DIR})
    list(APPEND common_compile_options -I\\\"${INCLUDE_DIR}\\\")
endforeach()

# Add an extra quote to the last element
list(LENGTH ALL_INCLUDE_DIR INCLUDE_DIR_COUNT)
math(EXPR LAST_INDEX "${INCLUDE_DIR_COUNT} - 1")
list(GET ALL_INCLUDE_DIR ${LAST_INDEX} LAST_INCLUDE_DIR)
string(CONCAT LAST_INCLUDE_DIR_WITH_QUOTE ${LAST_INCLUDE_DIR} "\"")
list(POP_BACK common_compile_options)
list(APPEND common_compile_options ${LAST_INCLUDE_DIR_WITH_QUOTE})

# Convert the list to a string with spaces between the options
string(REPLACE ";" " " common_compile_options_string "${common_compile_options}")

# Filter the source files to keep only .cpp and .hpp files
list(FILTER SOURCES INCLUDE REGEX "\\.(cpp|hpp|h|c)$")

set(compile_commands "")
foreach(SOURCE_FILE ${SOURCES})
    # Create a JSON string for each .cpp or .hpp source file
    string(APPEND compile_commands
        "    {\n"
        "        \"directory\": \"${CMAKE_CURRENT_BINARY_DIR}\",\n"
        "        \"command\": \"\\\"${CMAKE_CXX_COMPILER}\\\" -x c++ \\\"${SOURCE_FILE}\\\""
    )

    # Add the common compile options
    foreach(OPTION ${common_compile_options})
        string(APPEND compile_commands " ${OPTION}") 
    endforeach()

    # Add the rest of the command and JSON metadata
    string(APPEND compile_commands
        ",\n"
        "        \"file\": \"${SOURCE_FILE}\"\n"
        "    },\n"
    )
endforeach()

# Remove the final comma
string(REGEX REPLACE ",\n$" "" compile_commands "${compile_commands}")

# Add brackets to form a JSON list
set(compile_commands "[\n${compile_commands}\n]")

# Write the entries to a JSON file
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json "${compile_commands}")

# Add the custom target to generate compile_commands.json
add_custom_target(generate_compile_commands
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
    COMMENT "Generating compile_commands.json"
)