
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>

#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/PatternLayout.hh>
#include <event2/event.h>

#include "comm_client_cb_api.h"
#include "cct_proxy_service.h"

void get_options(int argc, char *argv[], cct_proxy_service::client_t & clnt, cct_proxy_service::service_t & svc, int & log_level);
void show_usage(const char * prog);
void init_log(const char * a_log_file, const char * a_log_dir, const int log_level, const char * logcat);

int main(int argc, char *argv[])
{
	int log_level = 500; //notice
	cct_proxy_service::client_t clnt;
	cct_proxy_service::service_t svc;
	get_options(argc, argv, clnt, svc, log_level);
	char log_file[32];
	snprintf(log_file, 32, "cct_proxy_%u.log", clnt.id);
	init_log(log_file, "./logs", log_level, "cctp");

	cct_proxy_service proxy("cctp");
	proxy.serve(svc, clnt);

	return 0;
}


void get_options(int argc, char *argv[], cct_proxy_service::client_t & clnt, cct_proxy_service::service_t & svc, int & log_level)
{
	if(argc == 1)
	{
		show_usage(argv[0]);
		exit(0);
	}
	int opt;
	while ((opt = getopt(argc, argv, "hi:c:f:a:p:l:")) != -1)
	{
		switch (opt)
		{
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'i':
			clnt.id = (unsigned int)strtol(optarg, NULL, 10);
			break;
		case 'c':
			clnt.count = (unsigned int)strtol(optarg, NULL, 10);
			break;
		case 'f':
			clnt.conf_file = optarg;
			break;
		case 'a':
			svc.ip = optarg;
			break;
		case 'p':
			svc.port = (u_int16_t)strtol(optarg, NULL, 10);
			break;
		case 'l':
			log_level = (int)strtol(optarg, NULL, 10);
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
	std::cout << "-i   client id" << std::endl;
	std::cout << "-c   peer count" << std::endl;
	std::cout << "-f   peer address file" << std::endl;
	std::cout << "-a   service address" << std::endl;
	std::cout << "-p   service port" << std::endl;
	std::cout << "-l   log level" << std::endl;
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

    log4cpp::Category::getInstance(logcat).addAppender(appender);
    log4cpp::Category::getInstance(logcat).setPriority((log4cpp::Priority::PriorityLevel)log_level);
    log4cpp::Category::getInstance(logcat).notice("log start");
}

