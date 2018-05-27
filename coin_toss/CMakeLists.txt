cmake_minimum_required(VERSION 2.8)
project(cointoss)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x")

INCLUDE_DIRECTORIES(
	$ENV{HOME}/ACP/comm_client
)

set(SOURCE_FILES
	ac_protocol.cpp
	cc_coin_toss.cpp
	coin_toss_test.cpp
	coin_toss_tester.cpp
)

link_directories(
	$ENV{HOME}/ACP/comm_client
)

add_executable(cointoss ${SOURCE_FILES})

TARGET_LINK_LIBRARIES( cointoss
	commclient
	log4cpp
	event
	crypto
	pthread
)


