cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- MSVC runtime flags ---
if (MSVC)
  # В точности как просили:
  set(CMAKE_CXX_FLAGS_RELEASE "/MT")
  set(CMAKE_CXX_FLAGS_DEBUG "/MD")
endif ()

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

get_property(importTargets DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY IMPORTED_TARGETS)
message(STATUS "${importTargets}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(MyLib STATIC 
	src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
	src/player.h
	src/player.cpp
	src/collision_detector.cpp
	src/collision_detector.h
)

target_link_libraries(MyLib PUBLIC CONAN_PKG::boost CONAN_PKG::libpq CONAN_PKG::libpqxx)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
	src/logger.h
	src/logger.cpp
	src/ticker.h
	src/extra_data.h
	src/infrastructure.h
	src/infrastructure.cpp
	src/retire_repository.h
	src/retire_repositoryImpl.h
)

target_link_libraries(game_server MyLib)

add_executable(game_server_tests
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
	tests/collision-detector-tests.cpp
)

target_include_directories(game_server_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(game_server_tests
    PRIVATE
        CONAN_PKG::catch2
        MyLib
)

# Определение макроса через CMake:
if (WIN32)
  target_compile_definitions(game_server PRIVATE _WIN32_WINNT=0x0601)
  # <sdkddkver.h> подключайте в исходниках (это нельзя сделать через CMake).
endif ()
