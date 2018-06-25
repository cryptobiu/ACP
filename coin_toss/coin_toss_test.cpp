
#include <unistd.h>
#include <semaphore.h>
#include <memory.h>

#include <string>
#include <vector>
#include <list>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <log4cpp/Category.hh>
#include <event2/event.h>

#include "comm_client_cb_api.h"
#include "comm_client_factory.h"
#include "ac_protocol.h"

#include "coin_toss_test.h"
#include "cc_coin_toss.h"

void test_tcp_mesh_coin_toss(const unsigned int id, const unsigned int count, const char * party_file, const size_t rounds, const char * logcat)
{
	comm_client::cc_args_t cc_args;
	cc_args.logcat = std::string(logcat) + ".ctm";
	cc_coin_toss cct(comm_client_factory::cc_tcp_mesh, &cc_args);
	if(0 != cct.run(id, count, party_file, rounds, 60))
		log4cpp::Category::getInstance(logcat).error("%s: %lu round tcp mesh coin toss test failed.", __FUNCTION__, rounds);
	else
		log4cpp::Category::getInstance(logcat).notice("%s: %lu round tcp mesh coin toss test succeeded.", __FUNCTION__, rounds);
}

void test_tcp_proxy_coin_toss(const char * proxy_ip, const u_int16_t proxy_port,
		  	  	  	  	  	  const unsigned int id, const unsigned int count, const char * party_file, const size_t rounds, const char * logcat)
{
	log4cpp::Category::getInstance(logcat).notice("%s: started.", __FUNCTION__, rounds);
	comm_client::cc_args_t cc_args;
	cc_args.logcat = (std::string(logcat) + ".ctp");
	cc_args.proxy_addr = proxy_ip;
	cc_args.proxy_port = proxy_port;
	cc_coin_toss cct(comm_client_factory::cc_tcp_proxy, &cc_args);
	log4cpp::Category::getInstance(logcat).notice("%s: running cct.", __FUNCTION__, rounds);
	if(0 != cct.run(id, count, party_file, rounds, 60))
		log4cpp::Category::getInstance(logcat).error("%s: %lu round tcp proxy coin toss test failed.", __FUNCTION__, rounds);
	else
		log4cpp::Category::getInstance(logcat).notice("%s: %lu round tcp proxy coin toss test succeeded.", __FUNCTION__, rounds);
}

void test_tcp_proxy_server(const char * proxy_ip, const u_int16_t proxy_port,
						   const unsigned int id, const unsigned int count, const char * party_file, const int log_level)
{
	char * args[14];

	args[0] = strdup("/home/ranp/ACP/cct_proxy/cct_proxy");

	args[1] = strdup("-i");
	char pid[16];
	snprintf(pid, 16, "%u", id);
	args[2] = strdup(pid);

	args[3] = strdup("-c");
	char prts[16];
	snprintf(prts, 16, "%u", count);
	args[4] = strdup(prts);

	args[5] = strdup("-f");
	args[6] = strdup(party_file);

	args[7] = strdup("-a");
	args[8] = strdup(proxy_ip);

	args[9] = strdup("-p");
	char port[16];
	snprintf(port, 16, "%hu", proxy_port);
	args[10] = strdup(port);

	args[11] = strdup("-l");
	char level[16];
	snprintf(level, 16, "%d", log_level);
	args[12] = strdup(level);

	args[13] = NULL;

	execvp("/home/ranp/ACP/cct_proxy/cct_proxy", args);
}
