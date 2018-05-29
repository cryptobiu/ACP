cmake_minimum_required(VERSION 2.8)
project(ccw_proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

INCLUDE_DIRECTORIES(
	$ENV{HOME}/ACP/comm_client
)

set(SOURCE_FILES
	main.cpp
)

link_directories(
	$ENV{HOME}/ACP/comm_client )

add_executable(ccw_proxy ${SOURCE_FILES})

target_link_libraries( ccw_proxy
	commclient
	log4cpp
)
