cmake_minimum_required(VERSION 3.10)
project(audio_visualizer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Og -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
# -ftime-report to profile the compilation process
# -fno-stack-protector to allow compatibility with precompiled headers (maybe?)
set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME} main.cpp Presenter.cpp miniaudio_implementation.c Recorder.cpp)


# CMake function for shaders
# https://github.com/ARM-software/vulkan-sdk/blob/master/Sample.cmake
function(add_shader TARGET SHADER)

	# Find glslc shader compiler.
	# On Android, the NDK includes the binary, so no external dependency.
	if(ANDROID)
		file(GLOB glslc-folders ${ANDROID_NDK}/shader-tools/*)
		find_program(GLSLC glslc HINTS ${glslc-folders})
	else()
		find_program(GLSLC glslc)
	endif()

	# All shaders for a sample are found here.
	set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER})

	# For Android, write SPIR-V files to app/assets which is then packaged into the APK.
	# Otherwise, output in the binary directory.
	if(ANDROID)
		set(current-output-path ${CMAKE_CURRENT_SOURCE_DIR}/app/assets/shaders/${SHADER}.spv)
	else(ANDROID)
		set(current-output-path ${CMAKE_BINARY_DIR}/shaders/${SHADER}.spv)
	endif(ANDROID)

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(current-output-dir ${current-output-path} DIRECTORY)
	file(MAKE_DIRECTORY ${current-output-dir})
	add_custom_command(
		OUTPUT ${current-output-path}
		COMMAND ${GLSLC} -o ${current-output-path} ${current-shader-path}
		DEPENDS ${current-shader-path}
		IMPLICIT_DEPENDS CXX ${current-shader-path}
		VERBATIM)

	# Make sure our native build depends on this output.
	set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

add_shader(${PROJECT_NAME} shader.vert)
add_shader(${PROJECT_NAME} shader.frag)



# Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})

# GLFW
# https://www.glfw.org/docs/3.3/build_guide.html#build_link_cmake_source
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/glfw-3.3.7)
target_link_libraries(${PROJECT_NAME} glfw)

# miniaudio and vkfw and vulkan-hpp and ... ?
target_include_directories(${PROJECT_NAME} PUBLIC lib/include)


#target_precompile_headers(${PROJECT_NAME} PRIVATE
#	${Vulkan_INCLUDE_DIRS}/vulkan/vulkan.h
#)
#target_precompile_headers(${PROJECT_NAME} PRIVATE precompile_vulkan.h)
# todo figure out precompiled headers; apparently address randomization interferes, and maybe it's MinGW

# todo github fetching?
