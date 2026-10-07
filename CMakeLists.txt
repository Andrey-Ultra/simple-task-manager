cmake_minimum_required(VERSION 3.24)

project(simple_task_manager)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(simple_task_manager main.cpp
        TaskQueue.cpp
        TaskQueue.h)

include(FetchContent)
FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.18.7)
FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(httplib json)

target_link_libraries(simple_task_manager PRIVATE httplib::httplib nlohmann_json::nlohmann_json)