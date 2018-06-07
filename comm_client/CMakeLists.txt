cmake_minimum_required(VERSION 2.8)

project(commclient)

set(SOURCE_FILES
		ac_protocol.cpp
		cct_proxy_client.cpp
		comm_client.cpp
		comm_client_factory.cpp
		comm_client_tcp_mesh.cpp
		)

add_library(commclient SHARED ${SOURCE_FILES})

target_link_libraries(commclient pthread event)
