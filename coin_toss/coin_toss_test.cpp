
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

#include <event2/event.h>

#include "coin_toss_test.h"

#include "comm_client_cb_api.h"
#include "ac_protocol.h"

//#include "comm_client.h"
//#include "comm_client_tcp_mesh.h"
//#include "cct_proxy_client.h"
//#include "cct_proxy_service.h"

#include "cc_coin_toss.h"

void test_tcp_mesh_coin_toss(const unsigned int id, const unsigned int count, const char * party_file, const size_t rounds)
{
	cc_coin_toss cct;
	if(0 != cct.run(id, count, party_file, rounds, 60))
		syslog(LOG_ERR, "%s: %lu round tcp mesh coin toss test failed.", __FUNCTION__, rounds);
	else
		syslog(LOG_NOTICE, "%s: %lu round tcp mesh coin toss test succeeded.", __FUNCTION__, rounds);
}

/*
void test_tcp_proxy_coin_toss(const char * proxy_ip, const u_int16_t proxy_port, const unsigned int id,
							  const unsigned int count, const char * party_file, std::vector<u_int8_t> & random)
{
	//comm_client_coin_toss tosser;
	//tosser.run_tcp_proxy_clent_coin_toss(proxy_ip, proxy_port, id, count, party_file, random);
}

void test_tcp_proxy_server(const char * proxy_ip, const u_int16_t proxy_port, const unsigned int id, const unsigned int count, const char * party_file)
{
	cct_proxy_service::client_t clnt;
	clnt.conf_file = party_file;
	clnt.count = count;
	clnt.id = id;

	cct_proxy_service::service_t svc;
	svc.ip = proxy_ip;
	svc.port = proxy_port;

	cct_proxy_service srvr;
	srvr.serve(svc, clnt);
}
*/
