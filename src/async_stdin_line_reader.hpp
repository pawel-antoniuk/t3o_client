#include <boost/asio.hpp>
#include <string>
#include <functional>
#include "event/event.hpp"

namespace t3o
{

class async_stdin_line_reader
{
public:
	async_stdin_line_reader(boost::asio::io_service& service) :
		_input_strm(service, ::dup(STDIN_FILENO)),
		_input_buf(512)
	{
	}

	void begin_read()
	{
		using namespace std::placeholders;
		boost::asio::async_read_until(_input_strm, _input_buf, '\n',
				std::bind(&async_stdin_line_reader::_handle_read,
					this, _1, _2));
	}

	auto& event_read() { return _read_event; }

private:
	void _handle_read(const boost::system::error_code&, std::size_t size)
	{
		std::string msg(size - 1, 0);
		_input_buf.sgetn(&msg[0], size - 1);
		_input_buf.consume(1);
		_read_event(msg);
		begin_read();
	}

	boost::asio::posix::stream_descriptor _input_strm;
	boost::asio::streambuf _input_buf;

	//events
	event<void(const std::string&)> _read_event;
};

}
