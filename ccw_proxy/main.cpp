//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: WebSocket server, asynchronous
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/PatternLayout.hh>

#define LCAT(X)		log4cpp::Category::getInstance(X)

void get_options(int argc, char *argv[], std::string & address, u_int16_t & port, unsigned int & threads_num);
void show_usage(const char * prog);
void init_log(const char * a_log_file, const char * a_log_dir, const int log_level, const char * logcat);

#include "listener.h"

static const std::string master_cat = "ccwp";

int main(int argc, char* argv[])
{
	std::string service_address;
	u_int16_t port = 0;
	unsigned int threads_num = 1;

	get_options(argc, argv, service_address, port, threads_num);

    auto const address = boost::asio::ip::make_address(service_address);

    // The io_context is required for all I/O
    boost::asio::io_context ioc{(int)threads_num};

    // Create and launch a listening port
    std::make_shared<listener>(ioc, tcp::endpoint{address, port}, master_cat + ".lsnr")->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve((int)threads_num - 1);
    for(auto i = (int)threads_num - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    return EXIT_SUCCESS;
}

void get_options(int argc, char *argv[], std::string & address, u_int16_t & port, unsigned int & threads_num)
{
	if(argc == 1)
	{
		show_usage(argv[0]);
		exit(0);
	}
	int opt;
	while ((opt = getopt(argc, argv, "ha:p:n:")) != -1)
	{
		switch (opt)
		{
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'a':
			address = optarg;
			break;
		case 'p':
			port = (u_int16_t)strtol(optarg, NULL, 10);
			break;
		case 'n':
			threads_num = (unsigned int)strtol(optarg, NULL, 10);
			break;
		default:
			std::cerr << "Invalid program arguments." << std::endl;
			show_usage(argv[0]);
			exit(__LINE__);
		}
	}
}

void show_usage(const char * prog)
{
	std::cout << "Usage:" << std::endl;
	std::cout << prog << "   [ OPTIONS ]" << std::endl;
	std::cout << "-a   web socket service address" << std::endl;
	std::cout << "-p   web socket service port" << std::endl;
	std::cout << "-n   number of threads" << std::endl;
}

void init_log(const char * a_log_file, const char * a_log_dir, const int log_level, const char * logcat)
{
	static const char the_layout[] = "%d{%y-%m-%d %H:%M:%S.%l}| %-6p | %-15c | %m%n";

	std::string log_file = a_log_file;
	log_file.insert(0, "/");
	log_file.insert(0, a_log_dir);

    log4cpp::Layout * log_layout = NULL;
    log4cpp::Appender * appender = new log4cpp::RollingFileAppender("rlf.appender", log_file.c_str(), 10*1024*1024, 5);

    bool pattern_layout = false;
    try
    {
        log_layout = new log4cpp::PatternLayout();
        ((log4cpp::PatternLayout *)log_layout)->setConversionPattern(the_layout);
        appender->setLayout(log_layout);
        pattern_layout = true;
    }
    catch(...)
    {
        pattern_layout = false;
    }

    if(!pattern_layout)
    {
        log_layout = new log4cpp::BasicLayout();
        appender->setLayout(log_layout);
    }

    LCAT(logcat).addAppender(appender);
    LCAT(logcat).setPriority((log4cpp::Priority::PriorityLevel)log_level);
    LCAT(logcat).notice("log start");
}
