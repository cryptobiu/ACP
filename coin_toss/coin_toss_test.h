
#pragma once

void test_tcp_mesh_coin_toss(const unsigned int id, const unsigned int count, const char * party_file, const size_t rounds, const char * logcat);

void test_tcp_proxy_coin_toss(const char * proxy_ip, const u_int16_t proxy_port,
							  const unsigned int id, const unsigned int count, const char * party_file, const size_t rounds, const char * logcat);

void test_tcp_proxy_server(const char * proxy_ip, const u_int16_t proxy_port,
						   const unsigned int id, const unsigned int count, const char * party_file, const int log_level);
