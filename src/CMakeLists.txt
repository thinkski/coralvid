configure_file(config.hpp.in config.hpp)

add_executable(coralvid main.cpp ${CMAKE_CURRENT_BINARY_DIR}/shader.cpp converter.cpp)

find_library(EGL_LIB NAMES EGL)
find_library(GBM_LIB NAMES gbm)
find_library(GLESv2_LIB NAMES GLESv2)
find_library(X264_LIB NAMES x264)

if (NOT EGL_LIB)
	message(FATAL_ERROR "libEGL required but not found.")
endif()

if (NOT GBM_LIB)
	message(FATAL_ERROR "libgbm required but not found.")
endif()

if (NOT GLESv2_LIB)
	message(FATAL_ERROR "libGLESv2 required but not found.")
endif()

if (NOT X264_LIB)
	message(FATAL_ERROR "libx264 required but not found.")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(coralvid ${EGL_LIB} ${GBM_LIB} ${GLESv2_LIB} ${X264_LIB})

add_custom_command(
	OUTPUT shader.cpp
	COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/glsl2str.py ${CMAKE_CURRENT_SOURCE_DIR}/yuyv_to_yuv420p.comp ${CMAKE_CURRENT_BINARY_DIR}/shader.cpp
	DEPENDS ${CMAKE_SOURCE_DIR}/tools/glsl2str.py ${CMAKE_CURRENT_SOURCE_DIR}/yuyv_to_yuv420p.comp
	COMMENT "Generating file containing compute shader source as string."
)
