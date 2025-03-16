cmake_minimum_required(VERSION 3.21)
project(nav LANGUAGES C CXX)

if(POLICY CMP0077)
	cmake_policy(SET CMP0077 NEW)
endif()

if(POLICY CMP0144)
	cmake_policy(SET CMP0144 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Read version
file(READ include/nav/nav.h NAV_INCLUDE_H LIMIT 1024)
string(REGEX MATCH "#define NAV_VERSION_MAJOR ([0-9+])" _ "${NAV_INCLUDE_H}")
set(NAV_VERSION_MAJOR ${CMAKE_MATCH_1})
string(REGEX MATCH "#define NAV_VERSION_MINOR ([0-9+])" _ "${NAV_INCLUDE_H}")
set(NAV_VERSION_MINOR ${CMAKE_MATCH_1})
string(REGEX MATCH "#define NAV_VERSION_PATCH ([0-9+])" _ "${NAV_INCLUDE_H}")
set(NAV_VERSION_PATCH ${CMAKE_MATCH_1})

# Public include files
set(NAV_PUBLIC_INCLUDE
	include/nav/audioformat.h
	include/nav/input.h
	include/nav/nav.h
	include/nav/types.h
)

# Shared/static library
if(NOT DEFINED NAV_SHARED)
	if(BUILD_SHARED_LIBS)
		set(NAV_SHARED_DEFAULT 1)
	else()
		set(NAV_SHARED_DEFAULT 0)
	endif()
endif()
option(NAV_SHARED "NAV: Build as shared library" ${NAV_SHARED_DEFAULT})

if(NAV_SHARED)
	set(NAV_SHARED_VALUE SHARED)
else()
	set(NAV_SHARED_VALUE STATIC)
endif()

add_library(nav ${NAV_SHARED_VALUE}
	${NAV_PUBLIC_INCLUDE}
	src/NAV.cpp
	src/Backend.cpp
	src/Backend.hpp
	src/androidndk/AndroidNDKBackend.cpp
	src/androidndk/AndroidNDKBackend.hpp
	src/androidndk/AndroidNDKPointers.h
	src/androidndk/AndroidNDKInternal.hpp
	src/ffmpeg_common/FFmpegBackend.cpp
	src/ffmpeg_common/FFmpegBackend.hpp
	src/ffmpeg_common/FFmpegCommon.hpp
	src/ffmpeg_common/FFmpegInternal.hpp
	src/ffmpeg_common/FFmpegPointers.h
	src/ffmpeg_common/FFmpegSetup.h
	src/ffmpeg4/FFmpeg4Backend.cpp
	src/ffmpeg4/FFmpeg4Backend.hpp
	src/ffmpeg5/FFmpeg5Backend.cpp
	src/ffmpeg5/FFmpeg5Backend.hpp
	src/ffmpeg6/FFmpeg6Backend.cpp
	src/ffmpeg6/FFmpeg6Backend.hpp
	src/ffmpeg7/FFmpeg7Backend.cpp
	src/ffmpeg7/FFmpeg7Backend.hpp
	src/gstreamer/GStreamerBackend.cpp
	src/gstreamer/GStreamerBackend.hpp
	src/gstreamer/GStreamerPointers.h
	src/gstreamer/GStreamerInternal.hpp
	src/mediafoundation/MediaFoundationBackend.cpp
	src/mediafoundation/MediaFoundationBackend.hpp
	src/mediafoundation/MediaFoundationInternal.hpp
	src/mediafoundation/MediaFoundationPointers.h
	src/Common.cpp
	src/Common.hpp
	src/NAVConfig.hpp
	src/DynLib.cpp
	src/DynLib.hpp
	src/Error.cpp
	src/Error.hpp
	src/InputFile.cpp
	src/InputFile.hpp
	src/InputFileAndroid.cpp
	src/InputMemory.cpp
	src/InputMemory.hpp
	src/Internal.hpp
	src/Internal.cpp
)
target_include_directories(nav PUBLIC include)
target_include_directories(nav PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_features(nav PUBLIC c_std_99)
target_compile_features(nav PRIVATE cxx_std_17)

if(NAV_SHARED)
	target_compile_definitions(nav PUBLIC NAV_SHARED)

	if(ANDROID)
		# https://developer.android.com/guide/practices/page-sizes
		target_link_options(nav PRIVATE "-Wl,-z,max-page-size=16384")
	else()
		set_target_properties(nav PROPERTIES
			VERSION ${NAV_VERSION_MAJOR}.${NAV_VERSION_MINOR}.${NAV_VERSION_PATCH}
			SOVERSION ${NAV_VERSION_MAJOR}
		)
	endif()

	if(UNIX)
		target_link_options(nav PRIVATE "-Wl,--no-undefined")
	endif()
endif()

# Silence C4996
if(MSVC)
	target_compile_definitions(nav PRIVATE _CRT_SECURE_NO_WARNINGS)
	target_compile_options(nav PRIVATE /Zc:__cplusplus)
endif()

# Find FFmpeg
if(DEFINED FFMPEG4_DIR OR DEFINED FFMPEG5_DIR OR DEFINED FFMPEG6_DIR OR DEFINED FFMPEG7_DIR)
	# Multi FFmpeg mode
	if(DEFINED FFMPEG_DIR)
		message(FATAL_ERROR "Cannot combine multi-FFmpeg include directories with single FFmpeg")
	endif()

	# Ok I trust you
	message(STATUS "Multi-FFmpeg mode activates:")
	foreach(i RANGE 4 7)
		if(FFMPEG${i}_DIR)
			set_source_files_properties(
				src/ffmpeg${i}/FFmpeg${i}Backend.cpp
				src/ffmpeg${i}/FFmpeg${i}Backend.hpp
				PROPERTIES
				INCLUDE_DIRECTORIES ${FFMPEG${i}_DIR}/include
			)
			set(NAV_BACKEND_FFMPEG_${i} ON)
			message(STATUS "- FFmpeg${i}")
		endif()
	endforeach()
	configure_file(src/multiffmpegconfig.h.in multiffmpegconfig.h)
	target_compile_definitions(nav PRIVATE HAVE_MULTIFFMPEGCONFIG_H)
elseif(FFMPEG_DIR)
	# Anyway I'll still trust you
	set(FFMPEG_INCLUDE_DIRS ${FFMPEG_DIR}/include)
elseif(ENV{FFMPEG_DIR})
	set(FFMPEG_INCLUDE_DIRS $ENV{FFMPEG_DIR}/include)
else()
	find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
endif()

if(FFMPEG_FOUND OR FFMPEG_INCLUDE_DIRS)
	message(STATUS "Using FFmpeg include directory (Single FFmpeg mode): ${FFMPEG_INCLUDE_DIRS}")
	target_include_directories(nav PRIVATE ${FFMPEG_INCLUDE_DIRS})
endif()

# Find GStreamer
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(GST gstreamer-1.0)
	pkg_check_modules(GST_BASE gstreamer-plugins-base-1.0)
	if(GST_FOUND AND GST_BASE_FOUND)
		message(STATUS "Using GStreamer include directory: ${GST_INCLUDE_DIRS} ${GST_BASE_INCLUDE_DIRS}")
		target_include_directories(nav PRIVATE ${GST_INCLUDE_DIRS} ${GST_BASE_INCLUDE_DIRS})
	endif()
endif()

# libdl?
if(CMAKE_DL_LIBS)
	target_link_libraries(nav PRIVATE ${CMAKE_DL_LIBS})
endif()

# Install files
if(PROJECT_IS_TOP_LEVEL)
	include(GNUInstallDirs)
	install(TARGETS nav)
	install(FILES ${NAV_PUBLIC_INCLUDE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nav)
	if(WIN32 AND NAV_SHARED)
		install(FILES $<TARGET_PDB_FILE:nav> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
	endif()
endif()
