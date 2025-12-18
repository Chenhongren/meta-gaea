#pragma once

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/object_server.hpp>

struct LoopbackI2CInfo
{
	std::string name;
	int bus;
	int addr;
	int start;
	int end;
};

class Loopback
{
	public:
		Loopback(const std::string& name, sdbusplus::asio::object_server& server, int bus, uint8_t addr, uint8_t start, uint8_t end, std::shared_ptr<sdbusplus::asio::connection>& conn);
		~Loopback();

	private:
		sdbusplus::asio::object_server& objServer;
		std::shared_ptr<sdbusplus::asio::dbus_interface> loopbackInterface;
		std::shared_ptr<sdbusplus::asio::connection> dbusConnection;
		std::unique_ptr<sdbusplus::bus::match_t> debugModeChangeMatch;
		std::string lp_name;
		int bus;
		uint8_t addr;
		uint8_t start;
		uint8_t end;
		int max_retry;
		bool debugMode;
		int performLoopbackTest(boost::asio::yield_context);
};
