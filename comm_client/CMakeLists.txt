cmake_minimum_required(VERSION 2.8)

if(${ARCH} MATCHES "Android")
	set(CMAKE_C_COMPILER /home/liork/Android/Sdk/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang)
	set(CMAKE_CXX_COMPILER /home/liork/Android/Sdk/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++)
endif()

project(commclient)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

INCLUDE_DIRECTORIES(
)

set(SOURCE_FILES
	ac_protocol.cpp
	cct_proxy_client.cpp
	comm_client.cpp
	comm_client_factory.cpp
	comm_client_tcp_mesh.cpp
)

add_library(commclient SHARED ${SOURCE_FILES})

target_link_libraries( commclient
	event
)
