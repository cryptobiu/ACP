
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>

#include <string>
#include <vector>
#include <map>
#include <deque>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <log4cpp/Category.hh>

#define lc_fatal(...) log4cpp::Category::getInstance(m_logcat).error(__VA_ARGS__)
#define lc_error(...) log4cpp::Category::getInstance(m_logcat).error(__VA_ARGS__)
#define lc_warn(...) log4cpp::Category::getInstance(m_logcat).warn(__VA_ARGS__)
#define lc_notice(...) log4cpp::Category::getInstance(m_logcat).notice(__VA_ARGS__)
#define lc_info(...) log4cpp::Category::getInstance(m_logcat).info(__VA_ARGS__)
#define lc_debug(...) log4cpp::Category::getInstance(m_logcat).debug(__VA_ARGS__)

#include <event2/event.h>

#include "mutex_timed_lock_for_macs.h"
#include "comm_client.h"
#include "comm_client_udp.h"
#include "comm_client_cb_api.h"

/*"Data can be written to the file descriptor fildes[1] and read from the file descriptor fildes[0]."*/
#define PIPE_READ_FD	0
#define PIPE_WRITE_FD	1

static const size_t max_msg_size = 64*1024;

comm_client_udp::comm_client_udp(comm_client::cc_args_t * cc_args)
: comm_client(cc_args), m_sock_fd(-1), m_send_msg_vec(NULL), m_recv_msg_vec(NULL)
, the_base(NULL), the_timer(NULL), the_recvr(NULL), the_sendr(NULL)
{
 	int errcode = 0;
 	if(0 != (errcode = pthread_mutex_init(&m_send_lock, NULL)))
 	{
         char errmsg[256];
         lc_error("%s: pthread_mutex_init() failed with error %d : [%s].",
         		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
         exit(__LINE__);
 	}
}

comm_client_udp::~comm_client_udp()
{
 	int errcode = 0;
 	if(0 != (errcode = pthread_mutex_destroy(&m_send_lock)))
 	{
         char errmsg[256];
         lc_error("%s: pthread_cond_destroy() failed with error %d : [%s].",
         		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
         exit(__LINE__);
 	}
}

int comm_client_udp::start(const unsigned int id, const unsigned int peer_count, const char * comm_conf_file, comm_client_cb_api * sink)
{
	lc_debug("%s: id=%u; count=%u; file=%s; ", __FUNCTION__, id, peer_count, comm_conf_file);

	if(id >= peer_count)
	{
		lc_error("%s: invalid id/parties values %u/%u", __FUNCTION__, id, peer_count);
		return -1;
	}

	if(get_run_flag())
	{
		lc_error("%s: this comm client is already started", __FUNCTION__);
		return -1;
	}
	set_run_flag(true);

	m_id = id;
	m_peer_count = peer_count;
	m_comm_conf_file = comm_conf_file;
	m_sink = sink;

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	lc_notice("%s: started %lu.%03lu", __FUNCTION__, ts.tv_sec, ts.tv_nsec/1000000);

	if(0 != load_peers())
	{
		lc_error("%s: parties load failure.", __FUNCTION__);
		return -1;
	}

	if(0 != start_socket())
	{
		lc_error("%s: service start failure.", __FUNCTION__);
		return -1;
	}

	if(0 != pipe(m_send_pipe))
	{
		int errcode = errno;
		char errmsg[512];
		lc_error("%s: pipe() failed with error %d : %s", __FUNCTION__, errcode, strerror_r(errcode, errmsg, 512));
		return -1;
	}

	comm_client_udp::init_msg_vec(m_send_msg_vec, m_peer_count);
	comm_client_udp::init_msg_vec(m_recv_msg_vec, m_peer_count);
}

void comm_client_udp::stop()
{
	comm_client::stop();
	comm_client_udp::delete_msg_vec(m_send_msg_vec, m_peer_count);
	comm_client_udp::delete_msg_vec(m_recv_msg_vec, m_peer_count);

	close(m_send_pipe[PIPE_READ_FD]);
	close(m_send_pipe[PIPE_WRITE_FD]);
	close(m_sock_fd);

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	lc_notice("%s: stopped %lu.%03lu", __FUNCTION__, ts.tv_sec, ts.tv_nsec/1000000);
}

int comm_client_udp::send(const unsigned int dst_id, const unsigned char * msg, const size_t size)
{
	if(max_msg_size < size)
	{
		lc_error("%s: max msg size %lu exceeded: %lu.", __FUNCTION__, max_msg_size, size);
		return -1;
	}

	msg_t * pmsg = new msg_t;
	pmsg->id = dst_id;
	pmsg->data.assign(msg, msg + size);

	static const u_int8_t b = 1;
	if(1 != write(m_send_pipe[PIPE_WRITE_FD], &b, 1))
	{
        int errcode = errno;
        char errmsg[256];
        lc_error("%s: write() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}

	struct timespec to;
	clock_gettime(CLOCK_REALTIME, &to);
	to.tv_sec += 2;

	int errcode;
	if(0 == (errcode = pthread_mutex_timedlock(&m_send_lock, &to)))
	{
		m_msg_q.push_back(pmsg);

		if(0 != (errcode = pthread_mutex_unlock(&m_send_lock)))
		{
	        int errcode = errno;
	        char errmsg[256];
	        lc_error("%s: pthread_mutex_unlock() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
	        exit(__LINE__);
		}
	}
	else
	{
        int errcode = errno;
        char errmsg[256];
        lc_error("%s: pthread_mutex_timedlock() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        return -1;
	}
	return 0;
}

void comm_client_udp::run()
{
	the_base = event_base_new();
	if(NULL != the_base)
	{
		the_timer = event_new(the_base, -1, EV_TIMEOUT|EV_PERSIST, comm_client_udp::time_cb, this);
		if(NULL != the_timer)
		{
			static const struct timeval sec_timeout = {1,0};
			if(0 == event_add(the_timer, &sec_timeout))
			{
				the_recvr = event_new(the_base, m_sock_fd, EV_READ|EV_PERSIST, comm_client_udp::recv_cb, this);
				if(NULL != the_recvr)
				{
					if(0 == event_add(the_recvr, NULL))
					{
						the_sendr = event_new(the_base, m_send_pipe[PIPE_READ_FD], EV_READ, comm_client_udp::send_cb, this);
						if(NULL != the_sendr)
						{
							if(0 == event_add(the_sendr, NULL))
							{
								lc_notice("%s: event loop started.", __FUNCTION__);
								event_base_dispatch(the_base);
								lc_notice("%s: event loop stopped.", __FUNCTION__);
								event_del(the_sendr);
							}
							else
								lc_error("%s: sendr event addition failed.", __FUNCTION__);
							event_free(the_sendr);
						}
						else
							lc_error("%s: sendr event creation failed.", __FUNCTION__);
						event_del(the_recvr);
					}
					else
						lc_error("%s: recvr event addition failed.", __FUNCTION__);
					event_free(the_recvr);
				}
				else
					lc_error("%s: recvr event creation failed.", __FUNCTION__);
				event_del(the_timer);
			}
			else
				lc_error("%s: timer event addition failed.", __FUNCTION__);
			event_free(the_timer);
		}
		else
			lc_error("%s: timer event creation failed.", __FUNCTION__);
		event_base_free(the_base);
	}
	else
		lc_error("%s: event_base creation failed.", __FUNCTION__);
}

int comm_client_udp::load_peers()
{
	lc_debug("%s: count=%u;", __FUNCTION__, m_peer_count);

	unsigned int n = 0;
	FILE * pf = fopen(m_comm_conf_file.c_str(), "r");
	if(NULL != pf)
	{
		char buffer[128];
		for(n = 0; n < m_peer_count; ++n)
		{
			bool peer_n_added = false;
			if(NULL != fgets(buffer, 128, pf))
			{
				peer_t peer;
				if(0 == parse_address(buffer, peer.ip, peer.port, peer.inet_addr))
				{
					peer.id = n;
					u_int64_t x = comm_client_udp::tokenize_inetaddr(peer.inet_addr);
					peer_n_added = m_peers.insert(std::pair< size_t , peer_t >(n, peer)).second &&
								   m_reeps.insert(std::pair< u_int64_t , size_t >(x, peer.id)).second;
				}
			}
			if(!peer_n_added)
				break;
		}
		fclose(pf);
	}
	return (m_peer_count == n)? 0: -1;
}

int comm_client_udp::start_socket()
{
	lc_debug("%s: ", __FUNCTION__);

	peer_t & self(m_peers[m_id]);
	if (0 > (m_sock_fd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        int errcode = errno;
        char errmsg[256];
        lc_error("%s: socket() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        return -1;
    }
	lc_debug("%s: self service socket created; fd = %d.", __FUNCTION__, m_sock_fd);

	if (0 != bind(m_sock_fd, (const sockaddr *)&self.inet_addr, (socklen_t)sizeof(struct sockaddr_in)))
	{
        int errcode = errno;
        char errmsg[256];
        lc_error("%s: bind() to [%s:%hu] failed with error %d : [%s].",
        		__FUNCTION__, self.ip.c_str(), self.port, errcode, strerror_r(errcode, errmsg, 256));
        close(m_sock_fd);
        return (m_sock_fd = -1);
	}
	lc_debug("%s: socket bound to address %s:%hu.", __FUNCTION__, self.ip.c_str(), self.port);

	return 0;
}

void comm_client_udp::init_msg_vec(struct mmsghdr * & msg_vec, size_t count)
{
	msg_vec = new struct mmsghdr[count];
	memset(msg_vec, 0, sizeof(struct mmsghdr) * count);
	for(size_t i = 0; i < count; ++i)
	{
		msg_vec[i].msg_len = 0;

		msg_vec[i].msg_hdr.msg_name = new struct sockaddr_in;
		msg_vec[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);

		msg_vec[i].msg_hdr.msg_iov = new struct iovec;
		msg_vec[i].msg_hdr.msg_iov[0].iov_base = new u_int8_t[max_msg_size];
		msg_vec[i].msg_hdr.msg_iov[0].iov_len = max_msg_size;
		msg_vec[i].msg_hdr.msg_iovlen = 1;

		msg_vec[i].msg_hdr.msg_control = NULL;
		msg_vec[i].msg_hdr.msg_controllen = 0;
	}
}

void comm_client_udp::clear_msg_vec(struct mmsghdr * & msg_vec, size_t count)
{
	for(size_t i = 0; i < count; ++i)
	{
		msg_vec[i].msg_len = 0;

		memset(msg_vec[i].msg_hdr.msg_name, 0, msg_vec[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in));

		memset(msg_vec[i].msg_hdr.msg_iov[0].iov_base, 0, max_msg_size);
		msg_vec[i].msg_hdr.msg_iov[0].iov_len = max_msg_size;
		msg_vec[i].msg_hdr.msg_iovlen = 1;

		msg_vec[i].msg_hdr.msg_control = NULL;
		msg_vec[i].msg_hdr.msg_controllen = 0;
		msg_vec[i].msg_hdr.msg_flags = 0;
	}
}

void comm_client_udp::delete_msg_vec(struct mmsghdr * & msg_vec, size_t count)
{
	for(size_t i = 0; i < count; ++i)
	{
		delete (u_int8_t *)msg_vec[i].msg_hdr.msg_iov[0].iov_base;
		delete msg_vec[i].msg_hdr.msg_iov;
		delete (struct sockaddr_in *)msg_vec[i].msg_hdr.msg_name;
	}
	delete msg_vec;
	msg_vec = NULL;
}

u_int64_t comm_client_udp::tokenize_inetaddr(const struct sockaddr_in & addr)
{
	return ((u_int64_t)addr.sin_addr.s_addr) << 32 | (u_int64_t)addr.sin_port;
}

void comm_client_udp::time_cb(evutil_socket_t fd, short what, void * arg)
{
	((comm_client_udp *)arg)->on_timer(fd, what);
}

void comm_client_udp::recv_cb(evutil_socket_t fd, short what, void * arg)
{
	((comm_client_udp *)arg)->on_recvr(fd, what);
}

void comm_client_udp::send_cb(evutil_socket_t fd, short what, void * arg)
{
	((comm_client_udp *)arg)->on_sendr(fd, what);
}

void comm_client_udp::on_timer(evutil_socket_t fd, short what)
{
	if(!get_run_flag())
	{
		lc_debug("%s: run flag down; breaking event loop.", __FUNCTION__);
		event_base_loopbreak(the_base);
	}
}

void comm_client_udp::on_recvr(evutil_socket_t fd, short what)
{
	int result = recvmmsg(fd, m_recv_msg_vec, m_peer_count, 0, NULL);
	if(0 == result)
	{
		for(size_t i = 0; i < m_peer_count; ++i)
		{
			if(0 > m_recv_msg_vec[i].msg_len)
			{
				lc_warn("%s: msg[%lu] has a negative msg-len result.", __FUNCTION__, i);
				continue;
			}
			if(0 < m_recv_msg_vec[i].msg_len)
			{
				process_recvd_msg(m_recv_msg_vec[i].msg_len, m_recv_msg_vec[i].msg_hdr);
				continue;
			}
			if(0 == m_recv_msg_vec[i].msg_len)
			{
				lc_warn("%s: msg[%lu] has 0 len; breaking.", __FUNCTION__);
				break;
			}
		}
	}
	else
	{
        int errcode = errno;
        char errmsg[256];
        lc_error("%s: recvmmsg() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
	}
	comm_client_udp::clear_msg_vec(m_recv_msg_vec, m_peer_count);
}

void comm_client_udp::on_sendr(evutil_socket_t fd, short what)
{
	event_free(the_sendr);

	if(fd == m_send_pipe[PIPE_READ_FD] && (what & EV_READ))
	{
		the_sendr = event_new(the_base, m_sock_fd, EV_WRITE, comm_client_udp::send_cb, this);
		if(NULL != the_sendr)
		{
			if(0 == event_add(the_sendr, NULL))
			{
				lc_debug("%s: sendr event set for write on sockfd %d.", __FUNCTION__, m_sock_fd);
				return;
			}
			else
				lc_error("%s: sendr event addition failed.", __FUNCTION__);
			event_free(the_sendr);
		}
		else
			lc_error("%s: sendr event creation failed.", __FUNCTION__);
	}
	else if(fd == m_sock_fd && (what & EV_WRITE))
	{
		perform_msgs_send();
	}
	else
	{
		lc_warn("%s: unidentified event state: fd=%d; what=%hu;", __FUNCTION__, fd, what);
	}

	the_sendr = event_new(the_base, m_send_pipe[PIPE_READ_FD], EV_READ, comm_client_udp::send_cb, this);
	if(NULL != the_sendr)
	{
		if(0 == event_add(the_sendr, NULL))
		{
			lc_debug("%s: sendr event set for read on send-pipe-read %d.", __FUNCTION__, m_send_pipe[PIPE_READ_FD]);
			return;
		}
		else
			lc_error("%s: sendr event addition failed.", __FUNCTION__);
		event_free(the_sendr);
	}
	else
		lc_error("%s: sendr event creation failed.", __FUNCTION__);
	exit(__LINE__);
}

void comm_client_udp::process_recvd_msg(const size_t msg_len, const struct msghdr & msg_hdr)
{
	size_t id((size_t)-1);
	if(msg_hdr.msg_namelen == sizeof(struct sockaddr_in))
	{
		u_int64_t token = comm_client_udp::tokenize_inetaddr(*((struct sockaddr_in *)msg_hdr.msg_name));
		std::map< u_int64_t , size_t >::const_iterator i = m_reeps.find(token);
		if(i != m_reeps.end())
		{
			id = i->second;
			lc_debug("%s: token %lu reverse lookup yielded id %lu", __FUNCTION__, token, id);
		}
		else
			lc_warn("%s: token %lu reverse lookup yielded no id.", __FUNCTION__, token);
	}
	m_sink->on_comm_message(id, (const u_int8_t *)msg_hdr.msg_iov[0].iov_base, msg_len);
}

void comm_client_udp::perform_msgs_send()
{
	std::deque< msg_t * > qd_msgs;
	pop_queued_msgs(qd_msgs);
	if(0 < qd_msgs.size())
	{
		size_t msg_offset = 0;
		while(!qd_msgs.empty())
		{
			msg_t * pmsg = *qd_msgs.begin();
			qd_msgs.pop_front();

			std::map< size_t , peer_t >::const_iterator i = m_peers.find(pmsg->id);
			if(m_peers.end() != i)
			{
				//populate a message slot
				*((struct sockaddr_in *)m_send_msg_vec[msg_offset].msg_hdr.msg_name) = i->second.inet_addr;
				m_send_msg_vec[msg_offset].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);

				memcpy(m_send_msg_vec[msg_offset].msg_hdr.msg_iov[0].iov_base, pmsg->data.data(),
						m_send_msg_vec[msg_offset].msg_len = pmsg->data.size());

				msg_offset++;
			}
			else
				lc_error("%s: failed to find peer details for party id %lu; msg dropped.", __FUNCTION__, pmsg->id);

			delete pmsg;

			if(msg_offset == m_peer_count || qd_msgs.empty())
			{
				if(0 != sendmmsg(m_sock_fd, m_send_msg_vec, msg_offset, 0))
				{
			        int errcode = errno;
			        char errmsg[256];
			        lc_error("%s: sendmmsg() failed with error %d : [%s].",
			        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
				}
				clear_msg_vec(m_send_msg_vec, m_peer_count);
			}
		}
	}
}

void comm_client_udp::pop_queued_msgs(std::deque< msg_t * > & qd_msgs)
{
	static u_int8_t buffer[max_msg_size];
	qd_msgs.clear();

	struct timespec to;
	clock_gettime(CLOCK_REALTIME, &to);
	to.tv_sec += 2;

	int errcode;
	if(0 == (errcode = pthread_mutex_timedlock(&m_send_lock, &to)))
	{
		qd_msgs.swap(m_msg_q);

		if(0 != (errcode = pthread_mutex_unlock(&m_send_lock)))
		{
	        int errcode = errno;
	        char errmsg[256];
	        lc_error("%s: pthread_mutex_unlock() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
	        exit(__LINE__);
		}

		if(0 > read(m_send_pipe[PIPE_READ_FD], buffer, qd_msgs.size()))
		{
	        int errcode = errno;
	        char errmsg[256];
	        lc_error("%s: read() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
		}
	}
	else
	{
        int errcode = errno;
        char errmsg[256];
        lc_error("%s: pthread_mutex_timedlock() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
	}
	return;
}
