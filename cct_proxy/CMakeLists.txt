cmake_minimum_required(VERSION 2.8)
project(cct_proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

INCLUDE_DIRECTORIES(
	$ENV{HOME}/ACP/comm_client
)

set(SOURCE_FILES
	cct_proxy_service.cpp
	main.cpp
)

link_directories(
	$ENV{HOME}/ACP/comm_client )

add_executable(cct_proxy ${SOURCE_FILES})

target_link_libraries( cct_proxy
	commclient
	event
	log4cpp
)
