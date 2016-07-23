#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <functional>
#include "protocol/detail/protocol_detail.hpp"
#include "protocol/detail/basic_async_reader.hpp"
#include "protocol/detail/basic_async_writer.hpp"
#include "protocol/detail/serializers/text_iserializer.hpp"
#include "protocol/detail/serializers/text_oserializer.hpp"
#include "event/event.hpp"

namespace t3o
{
/**
 * The main class that controlling clinet
 */
class game_client
{
public:
	explicit game_client(boost::asio::io_service& service) :
		_socket(service),
		_resolver(service),
		_reader(_socket),
		_writer(_socket),
		_is_connected(false),
		_is_logged_in(false)
	{
		using namespace std::placeholders;
		_reader.event_disconnected() += 
			std::bind(&game_client::_handle_disconnect, this);
		_writer.event_disconnected() += 
			std::bind(&game_client::_handle_disconnect, this);
	}

	void async_connect(const std::string& addr, const std::string& port,
			std::function<void()> handler = nullptr)
	{
		using namespace boost::asio::ip;
		using namespace std::placeholders;
		tcp::resolver::query query(addr, port);
		_resolver.async_resolve(query,
				std::bind(&game_client::_handle_resolve, this, _1, _2, handler));
	}

	void async_sign_in(unsigned mode, const std::string& name, 
			std::function<void()> handler = nullptr)
	{
		_name = name;
		detail::protocol::client_handshake packet;
		packet.mode = mode;
		packet.name.fill(0);
		name.copy(packet.name.data(), packet.name.size());
		_writer.async_write(packet, std::bind(&game_client::_handle_log_in, 
					this, handler));
	}

	void async_set_field(unsigned x, unsigned y,
			std::function<void()> handler = nullptr)
	{
		t3o::detail::protocol::field_set_packet packet;
		packet.x = x;
		packet.y = y;
		packet.field = _symbol;
		_writer.async_write(packet, handler);
	}

	//events
	auto& event_game_started() { return _game_started_event; }
	auto& event_game_ended() { return _game_ended_event; }
	auto& event_field_set() { return _field_set_event; }
	auto& event_disconnected() { return _disconnected_event; }

	auto symbol() const { return _symbol; }
	auto width() const { return _width; }
	auto height() const { return _height; }

private:
	void _handle_resolve(const boost::system::error_code& er,
			boost::asio::ip::tcp::resolver::iterator it,
			std::function<void()> handler)
	{
		if(er) return;
		using namespace std::placeholders;
		_socket.async_connect(*it, std::bind(&game_client::_handel_connect,
					this, _1, handler));
	}

	void _handel_connect(const boost::system::error_code& er, 
			std::function<void()> handler)
	{
		if(er) return;
		_is_connected = true;
		if(handler) handler();
	}

	void _handle_disconnect()
	{
		_is_connected = false;
		_is_logged_in = false;
		_disconnected_event();
	}

	void _handle_log_in(std::function<void()> handler)
	{
		_async_recv_feedback([this,handler](auto p)
		{
			_is_logged_in = true;
			if(handler) handler();
			_listen_for_data();
		});
	}

	void _handle_keepalive(const detail::protocol::keepalive& packet)
	{
		_writer.async_write(packet);
		_listen_for_data();
	}

	void _handle_server_handshake(
			const detail::protocol::server_handshake& packet)
	{
		_symbol = static_cast<unsigned>(packet.symbol);
		_width = static_cast<unsigned>(packet.width);
		_height = static_cast<unsigned>(packet.height);
		_game_started_event();
		_listen_for_data();
	}

	void _handle_game_end(const detail::protocol::game_end& packet)
	{
		_game_ended_event(static_cast<unsigned>(packet.result));
		_listen_for_data();
	}

	void _handle_field_set(const detail::protocol::field_set_packet&
			packet)
	{
		_field_set_event(packet.field, packet.x, packet.y);
		_listen_for_data();
	}

	void _async_recv_feedback(
			std::function<void(detail::protocol::feedback)> handler)
	{
		_reader.async_read<detail::protocol::feedback>(handler);
	}

	void _listen_for_data()
	{
		using namespace std::placeholders;
		_reader.async_read<
			detail::protocol::keepalive,
			detail::protocol::server_handshake,
			detail::protocol::game_end,
			detail::protocol::field_set_packet
		>(
			std::bind(&game_client::_handle_keepalive, this, _1),
			std::bind(&game_client::_handle_server_handshake,
				this, _1),
			std::bind(&game_client::_handle_game_end, this, _1),
			std::bind(&game_client::_handle_field_set, this, _1));
	}

	boost::asio::ip::tcp::socket _socket;
	std::string _name;
	boost::asio::ip::tcp::resolver _resolver;
	unsigned _symbol;
	unsigned _width;
	unsigned _height;

	detail::basic_async_reader<detail::text_iserializer> _reader;
	detail::basic_async_writer<detail::text_oserializer> _writer;

	//flags
	bool _is_connected;
	bool _is_logged_in;

	//events
	event<void()> _game_started_event;
	event<void(unsigned)> _game_ended_event;
	event<void(unsigned, unsigned, unsigned)> _field_set_event;
	event<void()> _disconnected_event;
};

}
