cmake_minimum_required(VERSION 2.8)
project(ccw_proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

INCLUDE_DIRECTORIES(
	$ENV{HOME}/ACP/comm_client
	$ENV{HOME}/boost_1_67_0
)

set(SOURCE_FILES
	ccw_proxy_service.cpp
	main.cpp
)

link_directories(
	$ENV{HOME}/ACP/comm_client
	$ENV{HOME}/boost_1_67_0/stage/lib
)

add_executable(ccw_proxy ${SOURCE_FILES})

target_link_libraries( ccw_proxy
	libboost_system.a
	commclient
	log4cpp
	pthread
)
