cmake_minimum_required(VERSION 3.10)
project(audio_visualizer)

# todo change optimization level in release mode
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Og")
set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME} main.cpp audio.cpp graphics.cpp)



# Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/glfw-3.3.7)
target_link_libraries(${PROJECT_NAME} glfw)

# miniaudio and vkfw and vulkan-hpp and ... ?
target_include_directories(${PROJECT_NAME} PUBLIC lib/include)
# todo make this better??? vkfw weird
# todo github fetch stuff?
