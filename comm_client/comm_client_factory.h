
#pragma once

class comm_client;

namespace comm_client_factory
{
	typedef enum
	{
		cc_nil = 0,
		cc_tcp_mesh = 1,
		cc_tcp_proxy = 2,
	}client_type_t;

	comm_client * create_comm_client(const client_type_t type, const char * log_category);
};
