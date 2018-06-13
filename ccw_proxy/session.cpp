
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

#include "comm_client.h"

#define LCAT(X)		log4cpp::Category::getInstance(X)

//------------------------------------------------------------------------------

#include "session.h"

// Take ownership of the socket
session::session(tcp::socket socket, const std::string & cat)
: ws_(std::move(socket)), strand_(ws_.get_executor()), cat_(cat), cc_(NULL)
{
}

void session::run()
{
	// Accept the websocket handshake
	ws_.async_accept(
			boost::asio::bind_executor(
			strand_,
			std::bind(
				&session::on_accept,
				shared_from_this(),
				std::placeholders::_1)));
}

void session::on_accept(boost::system::error_code ec)
{
	if(ec)
		LCAT(cat_).error("%s: accept() failed; error = [%s]", __FUNCTION__, ec.message().c_str());

	// Read a message
	do_read();
}

void session::do_read()
{
	// Read a message into our buffer
	ws_.async_read(
		buffer_,
		boost::asio::bind_executor(
			strand_,
			std::bind(
				&session::on_read,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2)));
}

void session::on_read(boost::system::error_code ec, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);

	// This indicates that the session was closed
	if(ec == websocket::error::closed)
		return;

	if(ec)
		LCAT(cat_).error("%s: read() failed; error = [%s]", __FUNCTION__, ec.message().c_str());

	// Echo the message
	ws_.text(ws_.got_text());
	ws_.async_write(
		buffer_.data(),
		boost::asio::bind_executor(
			strand_,
			std::bind(
				&session::on_write,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2)));
}

void session::on_write(boost::system::error_code ec, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);

	if(ec)
	{
		LCAT(cat_).error("%s: write() failed; error = [%s]", __FUNCTION__, ec.message().c_str());
		return;
	}

	// Clear the buffer
	buffer_.consume(buffer_.size());

	// Do another read
	do_read();
}
