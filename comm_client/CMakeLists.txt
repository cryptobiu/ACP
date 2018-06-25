cmake_minimum_required(VERSION 2.8)
project(commclient)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

set(SOURCE_FILES
	ac_protocol.cpp
	cct_proxy_client.cpp
	comm_client.cpp
	comm_client_factory.cpp
	comm_client_tcp_mesh.cpp
	comm_client_udp.cpp
	subs_for_macosx.cpp
)

add_library(commclient SHARED ${SOURCE_FILES})

target_link_libraries(
	commclient
	event
	pthread
)
