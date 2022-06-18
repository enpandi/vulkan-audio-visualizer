cmake_minimum_required(VERSION 3.12)
project(audio_visualizer)

add_executable(${PROJECT_NAME}
	Frame.cpp
	Framebuffer.cpp
	GraphicsDevice.cpp
	GraphicsState.cpp
	main.cpp
	miniaudio_implementation.c
	Renderer.cpp
	SoundRecorder.cpp
	SurfaceInfo.cpp
	VertexBuffer.cpp
	vma_implementation.cpp
	Window.cpp
	Allocator.cpp Allocator.hpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<CONFIG:DEBUG>:-O0 -g -DDEBUG>)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<CONFIG:RELEASE>:-O2>)
# -ftime-report to profile the compilation process

# CMake function for shaders
# https://github.com/ARM-software/vulkan-sdk/blob/master/Sample.cmake
function(add_shader TARGET SHADER)

	# Find glslc shader compiler.
	# On Android, the NDK includes the binary, so no external dependency.
	if (ANDROID)
		file(GLOB glslc-folders ${ANDROID_NDK}/shader-tools/*)
		find_program(GLSLC glslc REQUIRED HINTS ${glslc-folders})
	else ()
		find_program(GLSLC glslc REQUIRED)
	endif ()

	# All shaders for a sample are found here.
	set(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER})

	# For Android, write SPIR-V files to app/assets which is then packaged into the APK.
	# Otherwise, output in the binary directory.
	if (ANDROID)
		set(current-output-path ${CMAKE_CURRENT_SOURCE_DIR}/app/assets/shaders/${SHADER}.spv)
	else (ANDROID)
		set(current-output-path ${CMAKE_BINARY_DIR}/shaders/${SHADER}.spv)
	endif (ANDROID)

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
#set(Vulkan_LIBRARY $ENV{VULKAN_SDK}/Lib/vulkan-1.lib) # this should not be necessary in a good CMake
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
message(NOTICE ${Vulkan_LIBRARY})
message(NOTICE ${Vulkan_INCLUDE_DIRS})
message(NOTICE ${Vulkan_LIBRARIES})

# GLFW
# https://www.glfw.org/docs/3.3/build_guide.html#build_link_cmake_source
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/glfw-3.3.7)
target_link_libraries(${PROJECT_NAME} glfw)

# miniaudio and vkfw and vulkan-hpp and ... ?
target_include_directories(${PROJECT_NAME} PUBLIC lib/include)

# todo github fetching?


# precompiled headers suck (ASLR bugs, undocumented size limit for GCH file, etc)
#target_precompile_headers(${PROJECT_NAME} PUBLIC
#	$<$<COMPILE_LANGUAGE:CXX>:"${CMAKE_CURRENT_SOURCE_DIR}/vkpch.h">
#	$<$<COMPILE_LANGUAGE:C>:<vulkan/vulkan.h$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:C>:<GLFW/glfw3.h$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:C>:<miniaudio/miniaudio.h$<ANGLE-R>>
#)
#target_precompile_headers(${PROJECT_NAME} PUBLIC
#	$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<array$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<chrono$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<cmath$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<exception$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<ios$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<iostream$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<limits$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<optional$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<ranges$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<span$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<stdexcept$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<system_error$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<tuple$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<utility$<ANGLE-R>>
#	$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>
#)
