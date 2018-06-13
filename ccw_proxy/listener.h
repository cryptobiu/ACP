
#pragma once

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::string cat_;

public:
    listener(boost::asio::io_context& ioc, tcp::endpoint endpoint, const std::string & cat);

    // Start accepting incoming connections
    void
    run()
    {
        if(! acceptor_.is_open())
            return;
        do_accept();
    }

    void
    do_accept()
    {
        acceptor_.async_accept(
            socket_,
            std::bind(
                &listener::on_accept,
                shared_from_this(),
                std::placeholders::_1));
    }

    void
    on_accept(boost::system::error_code ec)
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
};
