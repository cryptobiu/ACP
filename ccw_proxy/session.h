
#pragma once

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
    websocket::stream<tcp::socket> ws_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::beast::multi_buffer buffer_;

    std::string cat_;

public:
    explicit session(tcp::socket socket, const std::string & cat);

    // Start the asynchronous operation
    void run();

    void on_accept(boost::system::error_code ec);

    void do_read();

    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);

    void on_write(boost::system::error_code ec, std::size_t bytes_transferred);
};
