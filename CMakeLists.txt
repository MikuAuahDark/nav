cmake_minimum_required(VERSION 3.21)
project(nav LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(NAV_PUBLIC_INCLUDE
	include/nav/audioformat.h
	include/nav/input.h
	include/nav/nav.h
	include/nav/types.h
)

add_library(nav
	${NAV_PUBLIC_INCLUDE}
	src/nav.cpp
	src/nav_backend.cpp
	src/nav_backend.hpp
	src/nav_backend_ffmpeg.cpp
	src/nav_backend_ffmpeg.hpp
	src/nav_backend_ffmpeg_funcptr.h
	src/nav_backend_ffmpeg_internal.hpp
	src/nav_backend_mediafoundation.cpp
	src/nav_backend_mediafoundation.hpp
	src/nav_backend_mediafoundation_internal.hpp
	src/nav_common.cpp
	src/nav_common.hpp
	src/nav_config.hpp
	src/nav_dynlib.cpp
	src/nav_dynlib.hpp
	src/nav_error.cpp
	src/nav_error.hpp
	src/nav_input_file.cpp
	src/nav_input_file.hpp
	src/nav_input_memory.cpp
	src/nav_input_memory.hpp
	src/nav_internal.hpp
	src/nav_internal.cpp
)
target_include_directories(nav PUBLIC include)
target_compile_features(nav PUBLIC c_std_99)
target_compile_features(nav PRIVATE cxx_std_17)

# Get build type
get_target_property(NAV_TARGET_TYPE nav TYPE)
if(NAV_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
	target_compile_definitions(nav PUBLIC NAV_STATIC)
endif()

# Find FFmpeg
if(FFMPEG_DIR)
	# Ok I trusted you
	set(FFMPEG_INCLUDE_DIRS ${FFMPEG_DIR}/include)
else()
	find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
endif()

if(FFMPEG_FOUND OR FFMPEG_INCLUDE_DIRS)
	target_include_directories(nav PRIVATE ${FFMPEG_INCLUDE_DIRS})
endif()

# Install files
if(PROJECT_IS_TOP_LEVEL)
	include(GNUInstallDirs)
	install(TARGETS nav)
	install(FILES ${NAV_PUBLIC_INCLUDE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nav)
	if(WIN32 AND NAV_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
		install(FILES $<TARGET_PDB_FILE:nav> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
	endif()
endif()
