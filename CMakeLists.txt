cmake_minimum_required(VERSION 3.21)
project(nav LANGUAGES C CXX)

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

if(PROJECT_IS_TOP_LEVEL)
	include(GNUInstallDirs)
	install(TARGETS nav)
	install(FILES ${NAV_PUBLIC_INCLUDE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nav)
	install(FILES $<TARGET_PDB_FILE:nav> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()
