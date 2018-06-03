
#include <stdlib.h>

#include <string>
#include <vector>

#include "comm_client_factory.h"
#include "comm_client.h"
#include "comm_client_tcp_mesh.h"
#include "cct_proxy_client.h"

comm_client * comm_client_factory::create_comm_client(const comm_client_factory::client_type_t type, const char * log_category)
{
	switch(type)
	{
	case cc_tcp_mesh: return new comm_client_tcp_mesh(log_category);
	case cc_tcp_proxy: return new cct_proxy_client(log_category);
	default: return NULL;
	}
}
