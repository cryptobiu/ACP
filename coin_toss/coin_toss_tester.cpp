
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include <iostream>
#include <vector>
#include <set>

#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/SimpleLayout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/PatternLayout.hh>
#include <event2/event.h>

#include "comm_client_cb_api.h"
#include "coin_toss_test.h"

void get_options(int argc, char *argv[], size_t & parties, std::string & conf_file, size_t & rounds, int & log_level);
void show_usage(const char * prog);
void init_log(const char * a_log_file, const char * a_log_dir, const int log_level, const char * logcat);

void run_comm_tcp_mesh_client_test_fork(const size_t parties, const std::string & conf_file, const size_t rounds, const int log_level);
void run_comm_tcp_proxy_client_test_fork(const size_t parties, const std::string & conf_file, const size_t rounds, const int log_level);

int main(int argc, char *argv[]) {

	size_t parties, rounds;
	std::string conf_file;
	int log_level = 500;//default = notice

	get_options(argc, argv, parties, conf_file, rounds, log_level);

	std::cout << "!!!Hello World!!!" << std::endl; // prints !!!Hello World!!!

	//run_comm_tcp_mesh_client_test_fork(parties, conf_file, rounds, log_level);
	run_comm_tcp_proxy_client_test_fork(parties, conf_file, rounds, log_level);

	return 0;
}

//******************************************************************************************//

void get_options(int argc, char *argv[], size_t & parties, std::string & conf_file, size_t & rounds, int & log_level)
{
	if(argc == 1)
	{
		show_usage(argv[0]);
		exit(0);
	}
	int opt;
	while ((opt = getopt(argc, argv, "hn:f:r:l:")) != -1)
	{
		switch (opt)
		{
		case 'h':
			show_usage(argv[0]);
			exit(0);
		case 'n':
			parties = (size_t)strtol(optarg, NULL, 10);
			break;
		case 'f':
			conf_file = optarg;
			break;
		case 'r':
			rounds = (size_t)strtol(optarg, NULL, 10);
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

//******************************************************************************************//

void show_usage(const char * prog)
{
	std::cout << "Usage:" << std::endl;
	std::cout << prog << "   [ OPTIONS ]" << std::endl;
	std::cout << "-n   number of parties" << std::endl;
	std::cout << "-f   peer address file" << std::endl;
	std::cout << "-r   number of rounds" << std::endl;
	std::cout << "-l   log level [fatal=0,alert=100,critical=200,error=300,warning=400,notice=500(default),info=600,debug=700]" << std::endl;
}

//******************************************************************************************//

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

//******************************************************************************************//

void run_comm_tcp_mesh_client_test_fork(const size_t parties, const std::string & conf_file, const size_t rounds, const int log_level)
{
	pid_t cpid;
	std::set<pid_t> children_of_the_revolution;
	char log_category[32];
	char log_file[32];

	for(size_t i = 0; i < parties; i++)
	{
		switch(cpid = fork())
		{
		case 0:
			snprintf(log_category, 32, "ct.tmt.%03lu", i);
			snprintf(log_file, 32, "coin_toss_test.%03lu.log", i);
			init_log(log_file, "./logs", log_level, log_category);
			test_tcp_mesh_coin_toss(i, parties, conf_file.c_str(), rounds, log_category);
			exit(0);
		case -1:
			{
				int errcode = errno;
				char errmsg[512];
				std::cerr << "fork() failed with error " << errcode << " : [" << strerror_r(errcode, errmsg, 256) << "]" << std::endl;
				for(std::set<pid_t>::iterator i = children_of_the_revolution.begin(); i != children_of_the_revolution.end(); ++i)
					kill(*i, 9);
			}
			exit(-1);
		default:
			std::cout << "cpid: " << cpid << std::endl;
			children_of_the_revolution.insert(cpid);
			continue;
		}
	}
}

//******************************************************************************************//
void run_comm_tcp_proxy_client_test_fork(const size_t parties, const std::string & conf_file, const size_t rounds, const int log_level)
{
	pid_t cpid;
	std::set<pid_t> children_of_the_revolution;
	char log_category[32];
	char log_file[32];

	//launch proxies
	for(size_t i = 0; i < parties; i++)
	{
		switch(cpid = fork())
		{
		case 0:
			test_tcp_proxy_server("127.0.0.1", 9000 + i, i, parties, conf_file.c_str(), log_level);
			exit(0);
		case -1:
			{
				int errcode = errno;
				char errmsg[512];
				std::cerr << "fork() failed with error " << errcode << " : [" << strerror_r(errcode, errmsg, 256) << "]" << std::endl;
				for(std::set<pid_t>::iterator i = children_of_the_revolution.begin(); i != children_of_the_revolution.end(); ++i)
					kill(*i, 9);
			}
			exit(-1);
		default:
			std::cout << "cct proxy cpid: " << cpid << std::endl;
			children_of_the_revolution.insert(cpid);
			continue;
		}
	}

	//launch clients
	for(size_t i = 0; i < parties; i++)
	{
		switch(cpid = fork())
		{
		case 0:
			snprintf(log_category, 32, "ct.tpc.%03lu", i);
			snprintf(log_file, 32, "coin_toss_test.%03lu.log", i);
			init_log(log_file, "./logs", log_level, log_category);
			test_tcp_proxy_coin_toss("127.0.0.1", 9000 + i, i, parties, conf_file.c_str(), rounds, log_category);
			exit(0);
		case -1:
			{
				int errcode = errno;
				char errmsg[512];
				std::cerr << "fork() failed with error " << errcode << " : [" << strerror_r(errcode, errmsg, 256) << "]" << std::endl;
				for(std::set<pid_t>::iterator i = children_of_the_revolution.begin(); i != children_of_the_revolution.end(); ++i)
					kill(*i, 9);
			}
			exit(-1);
		default:
			std::cout << "cct client cpid: " << cpid << std::endl;
			children_of_the_revolution.insert(cpid);
			continue;
		}
	}
}
//******************************************************************************************//
