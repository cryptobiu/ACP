
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

#include <log4cpp/Category.hh>

#define LCAT(X)		log4cpp::Category::getInstance(X)

#include "session.h"
#include "listener.h"

listener::listener(boost::asio::io_context& ioc, tcp::endpoint endpoint, const std::string & cat)
: acceptor_(ioc), socket_(ioc), cat_(cat)
{
	boost::system::error_code ec;

	// Open the acceptor
	acceptor_.open(endpoint.protocol(), ec);
	if(ec)
	{
		LCAT(cat_).error("%s: open() failed; error = [%s]", __FUNCTION__, ec.message().c_str());
		return;
	}

	// Allow address reuse
	acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
	if(ec)
	{
		LCAT(cat_).error("%s: set_option() failed; error = [%s]", __FUNCTION__, ec.message().c_str());
		return;
	}

	// Bind to the server address
	acceptor_.bind(endpoint, ec);
	if(ec)
	{
		LCAT(cat_).error("%s: bind() failed; error = [%s]", __FUNCTION__, ec.message().c_str());
		return;
	}

	// Start listening for connections
	acceptor_.listen(
		boost::asio::socket_base::max_listen_connections, ec);
	if(ec)
	{
		LCAT(cat_).error("%s: listen() failed; error = [%s]", __FUNCTION__, ec.message().c_str());
		return;
	}
}

void listener::run()
{
	if(! acceptor_.is_open())
		return;
	do_accept();
}

void listener::do_accept()
{
	acceptor_.async_accept(
		socket_,
		std::bind(
			&listener::on_accept,
			shared_from_this(),
			std::placeholders::_1));
}

void listener::on_accept(boost::system::error_code ec)
{
	if(ec)
	{
		LCAT(cat_).error("%s: accept() failed; error = [%s]", __FUNCTION__, ec.message().c_str());
	}
	else
	{
		// Create the session and run it
		std::make_shared<session>(std::move(socket_), cat_ + ".sess")->run();
	}

	// Accept another connection
	do_accept();
}
