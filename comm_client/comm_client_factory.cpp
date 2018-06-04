
#include <stdlib.h>

#include <string>
#include <vector>

#include "comm_client_factory.h"
#include "comm_client.h"
#include "comm_client_tcp_mesh.h"
#include "cct_proxy_client.h"

comm_client * comm_client_factory::create_comm_client(const comm_client_factory::client_type_t type, comm_client::cc_args_t * args)
{
	switch(type)
	{
	case cc_tcp_mesh: return new comm_client_tcp_mesh(args);
	case cc_tcp_proxy: return new cct_proxy_client(args);
	default: return NULL;
	}
}
