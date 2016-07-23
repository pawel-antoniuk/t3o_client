#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <iterator>
#include <cstdio>
#include <thread>
#include <chrono>
#include <sstream>
#include <thread>

#include "game_client.hpp"
#include "async_stdin_line_reader.hpp"

void handle_read_line(t3o::game_client& client,
		const std::string& line)
{
	std::istringstream iss(line);
	std::string command;
	iss >> command;
	if(command == "connect")
	{
		std::string addr, port, name;
		iss >> addr >> port >> name;
		client.async_connect(addr, port, [&client, name]
		{
			std::cout << "connected" << std::endl;
			client.async_sign_in(0, name, []
			{
				std::cout << "logged in" << std::endl;
			});
		});		
	}
	if(command == "set")
	{
		unsigned x = 0, y = 0;
		if(iss >> x >> y)
		{
			client.async_set_field(x, y);
		}
	}
}

/**
 * ... text ...
 */
int main()
{
	using namespace std::placeholders;
	boost::asio::io_service service;
	t3o::game_client client(service);
	t3o::async_stdin_line_reader stdin_reader(service);
	stdin_reader.begin_read();
	stdin_reader.event_read() += std::bind(handle_read_line,
			std::ref(client), _1);
	client.event_disconnected() += [&client, &service]
	{
		std::cout << "disconnected" << std::endl;
		service.stop();
	};
	client.event_game_started() += [&client]
	{
		std::cout << "game started\nsymbol: " << client.symbol()
			<< "\nmap: " << client.width() << 'x' 
			<< client.height() << std::endl;
	};
	client.event_game_ended() += [](auto result)
	{
		std::cout << "game endded; result: " 
			<< result << std::endl;
	};
	client.event_field_set() += [](auto field, auto x, auto y)
	{
		std::cout << '[' << x << "][" << y << "]=" << field 
		<< std::endl;
	};

	service.run();
}
