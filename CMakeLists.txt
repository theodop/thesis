#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(TheoThesis CXX)

set(DEPENDENCIES realsense2)
find_package(Curses REQUIRED)
find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)
find_package( OpenCV REQUIRED )
include_directories(${CURSES_INCLUDE_DIR})
include_directories(${ALSA_INCLUDE_DIR})
include_directories(${THREADS_INCLUDE_DIR})

add_executable(theo-thesis thesis.cpp)
target_link_libraries(theo-thesis ${DEPENDENCIES})
target_link_libraries(theo-thesis ${CURSES_LIBRARIES})
target_link_libraries(theo-thesis ${ALSA_LIBRARIES})
target_link_libraries(theo-thesis ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(theo-thesis ${OpenCV_LIBS})
set_property(TARGET theo-thesis PROPERTY CXX_STANDARD 11)

set_target_properties (theo-thesis PROPERTIES
	FOLDER "Examples/OpenCV"
)

install(
	TARGETS

	theo-thesis

	RUNTIME DESTINATION
	${CMAKE_INSTALL_PREFIX}/bin
)
