
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include <iostream>
#include <vector>
#include <set>

#include <event2/event.h>

#include "comm_client_cb_api.h"

#include "coin_toss_test.h"
#include "cct_proxy_service.h"

void run_comm_client_test_thread();
void run_comm_tcp_mesh_client_test_fork();
void run_comm_tcp_proxy_client_test_fork();
void run_comm_client_test_single();
void run_comm_client_tcp_proxy_svc_test_single();

int main() {
	std::cout << "!!!Hello World!!!" << std::endl; // prints !!!Hello World!!!

	run_comm_tcp_mesh_client_test_fork();
	//run_comm_tcp_proxy_client_test_fork();
	//run_comm_client_test_thread();
	//run_comm_client_test_single();
	//run_comm_client_tcp_proxy_svc_test_single();

	return 0;
}

//******************************************************************************************//

void run_comm_tcp_mesh_client_test_fork()
{
	static const size_t count = 25;
	static const char * party_file = "/home/ranp/workspace_other/parties_500.txt";
	std::vector<u_int8_t> random;
	pid_t cpid;

	std::set<pid_t> children_of_the_revolution;

	for(size_t i = 0; i < count; i++)
	{
		switch(cpid = fork())
		{
		case 0:
			test_tcp_mesh_coin_toss(i, count, party_file, random);
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

void run_comm_tcp_proxy_client_test_fork()
{
	static const size_t count = 20;
	static const char * party_file = "/home/ranp/workspace_other/parties_500.txt";
	std::vector<u_int8_t> random;
	pid_t cpid;
	u_int16_t proxy_base_port = 42000;

	std::set<pid_t> proxy_srvrs, proxy_clnts;

	for(size_t i = 0; i < count; i++)
	{
		switch(cpid = fork())
		{
		case 0:
			test_tcp_proxy_server("127.0.0.1", proxy_base_port + i, i, count, party_file);
			exit(0);
		case -1:
			{
				int errcode = errno;
				char errmsg[512];
				std::cerr << "fork() failed with error " << errcode << " : [" << strerror_r(errcode, errmsg, 256) << "]" << std::endl;
				for(std::set<pid_t>::iterator i = proxy_srvrs.begin(); i != proxy_srvrs.end(); ++i)
					kill(*i, 9);
			}
			exit(-1);
		default:
			std::cout << "proxy server cpid: " << cpid << std::endl;
			proxy_srvrs.insert(cpid);
			continue;
		}
	}

	for(size_t i = 0; i < count; i++)
	{
		switch(cpid = fork())
		{
		case 0:
			test_tcp_proxy_coin_toss("127.0.0.1", proxy_base_port + i, i, count, party_file, random);
			exit(0);
		case -1:
			{
				int errcode = errno;
				char errmsg[512];
				std::cerr << "fork() failed with error " << errcode << " : [" << strerror_r(errcode, errmsg, 256) << "]" << std::endl;
				for(std::set<pid_t>::iterator i = proxy_clnts.begin(); i != proxy_clnts.end(); ++i)
					kill(*i, 9);
				for(std::set<pid_t>::iterator i = proxy_srvrs.begin(); i != proxy_srvrs.end(); ++i)
					kill(*i, 9);
			}
			exit(-1);
		default:
			std::cout << "proxy client cpid: " << cpid << std::endl;
			proxy_clnts.insert(cpid);
			continue;
		}
	}

	for(int i = 0; i < 120; ++i)
	{
		size_t clnt_count = 0;
		for(std::set<pid_t>::iterator i = proxy_clnts.begin(); i != proxy_clnts.end(); ++i)
			if(0 == kill(*i, 0))
				clnt_count++;

		if(0 == clnt_count)
			break;
		else
			sleep(1);
	}
	for(std::set<pid_t>::iterator i = proxy_srvrs.begin(); i != proxy_srvrs.end(); ++i)
		kill(*i, 9);
}

//******************************************************************************************//

typedef struct
{
	size_t count, id;
	std::string party_file;
	std::vector<u_int8_t> random;
}param_t;

void * test_proc(void *);

#define THDCNT	2

void run_comm_client_test_thread()
{
	std::cout << "start of " << __FUNCTION__ << std::endl;

	int retcode;
	param_t params[THDCNT];
	pthread_t threads[THDCNT];

	for(size_t i = 0; i < THDCNT; i++)
	{
		params[i].count = THDCNT;
		params[i].id = i;
		params[i].party_file = "/home/ranp/workspace_other/parties_3.txt";
		params[i].random.clear();

		if(0 == (retcode = pthread_create((threads + i), NULL, test_proc, (params + i))))
			std::cout << "Test thread " << i << " launched." << std::endl;
		else
		{
			char errmsg[256];
			std::cout << "Test thread " << i << " launch failed with error " << retcode << " : " << strerror_r(retcode, errmsg, 256) << std::endl;
			exit(__LINE__);
		}
	}

	for(size_t i = 0; i < THDCNT; i++)
	{
		struct timespec abstime;
		clock_gettime(CLOCK_REALTIME, &abstime);
		abstime.tv_sec += 60;

		void * retval;
		if(0 != (retcode = pthread_timedjoin_np(threads[i], &retval, &abstime)))
		{
			char errmsg[256];
			std::cout << "Test thread " << i << " join failed with error " << retcode << " : " << strerror_r(retcode, errmsg, 256) << std::endl;

			if(0 != (retcode = pthread_cancel(threads[i])))
			{
				char errmsg[256];
				std::cout << "Test thread " << i << " cancel failed with error " << retcode << " : " << strerror_r(retcode, errmsg, 256) << std::endl;
				exit(__LINE__);
			}
			else
				std::cout << "Test thread " << i << " cancelled." << std::endl;

			exit(__LINE__);
		}
		else
			std::cout << "Test thread " << i << " joined." << std::endl;
	}

	std::cout << "end of " << __FUNCTION__ << std::endl;
}

void * test_proc(void * arg)
{
	param_t * prm = (param_t *)arg;
	test_tcp_mesh_coin_toss(prm->id, prm->count, prm->party_file.c_str(), prm->random);
	return NULL;
}

//******************************************************************************************//

void run_comm_client_test_single()
{
	std::vector<u_int8_t> random;
	test_tcp_mesh_coin_toss(0, 3, "/home/ranp/workspace_other/parties_3.txt", random);
}

//******************************************************************************************//

void run_comm_client_tcp_proxy_svc_test_single()
{
	cct_proxy_service::client_t clnt;
	clnt.conf_file = "/home/ranp/workspace_other/parties_3.txt";
	clnt.id = 0;
	clnt.count = 2;

	cct_proxy_service::service_t svc;
	svc.ip = "127.0.0.1";
	svc.port = 41100;

	cct_proxy_service proxy;
	proxy.serve(svc, clnt);

}

//******************************************************************************************//

