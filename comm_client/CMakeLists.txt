cmake_minimum_required(VERSION 2.8)
if(${ARCH} MATCHES "Android")

	set( CMAKE_SYSTEM_VERSION 21 ) # API level
	set( CMAKE_C_COMPILER $ENV{HOME}/Android/Sdk/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang )
	set( CMAKE_CXX_COMPILER $ENV{HOME}/Android/Sdk/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -stdlib=libc++ --target=aarch64-none-linux-android ")
	INCLUDE_DIRECTORIES( $ENV{HOME}/Android/Sdk/ndk-bundle/sysroot/usr/include/ )

else()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

endif()

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
