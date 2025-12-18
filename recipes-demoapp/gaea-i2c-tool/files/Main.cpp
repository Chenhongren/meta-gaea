#include "Loopback.hpp"
#include "Util.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

static constexpr auto i2cToolServiceName = "xyz.gaea.i2c_tool";
static constexpr auto i2cToolObjName = "/xyz/gaea/i2c_tool";
static constexpr auto i2cToolMethodIntf = "xyz.gaea.i2c_tool.methods";
static constexpr auto i2cToolDebugIntf = "xyz.gaea.i2c_tool.debug";

static bool debugMode = false;

auto perform_i2c_transfer(boost::asio::yield_context, int bus, uint8_t addr,
						  uint8_t reg, size_t len)
{
	if (debugMode)
	{
		std::cout << "<dbg> i2c_xfer: start i2c transfer..." << std::endl;
	}

	int ret = -ENODEV;
	int fd = openI2CDevice(bus);
	if (fd < 0)
	{
		return std::make_tuple(ret, std::vector<uint8_t>{});
	}

	std::vector<uint8_t> rx(len);
	ret = i2c_transfer_internal(fd, addr, reg, rx.data(), rx.size());

	close(fd);

	if (!ret && debugMode)
	{
		std::cout << "<dbg> bus/addr: " << std::dec << bus << "/0x" << std::hex
				  << static_cast<int>(addr) << std::dec << std::endl;
		std::cout << "<dbg> tx: 0x" << std::hex << static_cast<int>(reg)
				  << std::endl;
		std::cout << "<dbg> rx:";
		for (uint8_t v : rx)
		{
			std::cout << " 0x" << std::hex << static_cast<int>(v);
		}
		std::cout << std::dec << std::endl;
	}

	return std::make_tuple(ret, ret ? std::vector<uint8_t>{} : rx);
}

auto perform_i2c_read(boost::asio::yield_context, int bus, uint8_t addr,
					  size_t len)
{
	if (debugMode)
	{
		std::cout << "<dbg> i2c_read: start i2c read..." << std::endl;
	}

	int ret = -ENODEV;
	int fd = openI2CDevice(bus);
	if (fd < 0)
	{
		return std::make_tuple(ret, std::vector<uint8_t>{});
	}

	std::vector<uint8_t> rx(len);
	ret = i2c_read_internal(fd, addr, rx.data(), rx.size());

	close(fd);

	if (!ret && debugMode)
	{
		std::cout << "<dbg> bus/addr: " << std::dec << bus << "/0x" << std::hex
				  << static_cast<int>(addr) << std::dec << std::endl;
		std::cout << "<dbg> rx:";
		for (uint8_t v : rx)
		{
			std::cout << " 0x" << std::hex << static_cast<int>(v);
		}
		std::cout << std::dec << std::endl;
	}

	return std::make_tuple(ret, ret ? std::vector<uint8_t>{} : rx);
}

int perform_i2c_write(boost::asio::yield_context, int bus, uint8_t addr,
					  std::vector<uint8_t>& data)
{
	if (debugMode)
	{
		std::cout << "<dbg> i2c_write: start i2c write..." << std::endl;
	}

	int fd = openI2CDevice(bus);
	if (fd < 0)
	{
		return -ENODEV;
	}

	int ret = i2c_write_internal(fd, addr, data.data(), data.size());

	close(fd);

	if (!ret && debugMode)
	{
		std::cout << "<dbg> bus/addr: " << std::dec << bus << "/0x" << std::hex
				  << static_cast<int>(addr) << std::dec << std::endl;
		std::cout << "<dbg> tx:";
		for (uint8_t v : data)
		{
			std::cout << " 0x" << std::hex << static_cast<int>(v);
		}
		std::cout << std::dec << std::endl;
	}

	return ret;
}

static void registerMethodInterface(sdbusplus::asio::object_server& server)
{
	std::shared_ptr<sdbusplus::asio::dbus_interface> method_iface =
		server.add_interface(i2cToolObjName, i2cToolMethodIntf);
	method_iface->register_method("write", perform_i2c_write);
	method_iface->register_method("read", perform_i2c_read);
	method_iface->register_method("transfer", perform_i2c_transfer);
	method_iface->initialize();
}

static void registerDebugInterface(sdbusplus::asio::object_server& server)
{
	std::shared_ptr<sdbusplus::asio::dbus_interface> debug_iface =
		server.add_interface(i2cToolObjName, i2cToolDebugIntf);
	debug_iface->register_property(
		"debugMode", debugMode, [&](const bool& req, bool& propertyValue) {
			if (propertyValue != req)
			{
				propertyValue = req;
				debugMode = req;
			}
			return true;
		});
	debug_iface->initialize();
}

static void createLoopbackDevice(
	const nlohmann::json& jsonConfig, sdbusplus::asio::object_server& server,
	std::shared_ptr<sdbusplus::asio::connection>& conn,
	boost::container::flat_map<std::string, std::shared_ptr<Loopback>>&
		loopbacks)
{
	const auto& tests = jsonConfig.at("loopback_device");
	for (const auto& item : tests)
	{
		LoopbackI2CInfo info{};
		item.at(nameProperty).get_to(info.name);
		item.at(i2cBusProperty).get_to(info.bus);
		item.at(chipAddrProperty).get_to(info.addr);
		item.at(startValueProperty).get_to(info.start);
		item.at(endValueProperty).get_to(info.end);

		try
		{
			auto lpdev = std::make_shared<Loopback>(
				info.name, server, info.bus, info.addr, info.start, info.end,
				conn);
			loopbacks[info.name] = lpdev;
		}
		catch (const std::exception& e)
		{
			std::cerr << "<err> failed to create " << info.name << ": "
					  << "bus/addr: " << std::dec << info.bus << "/0x"
					  << std::hex << info.addr << std::endl
					  << "      " << e.what() << std::endl;
			continue;
		}
	}
}

int main(void)
{
	boost::container::flat_map<std::string, std::shared_ptr<Loopback>>
		loopbacks;
	boost::asio::io_context io;
	auto conn = std::make_shared<sdbusplus::asio::connection>(io);
	conn->request_name(i2cToolServiceName);
	auto server = sdbusplus::asio::object_server(conn);

	std::ifstream jsonFile(configFile);
	if (!jsonFile.is_open())
	{
		std::cerr << "missing gaea-i2c-tool config" << std::endl;
		return -EINVAL;
	}

	registerMethodInterface(server);
	registerDebugInterface(server);

	try
	{
		auto jsonConfig = nlohmann::json::parse(jsonFile);
		createLoopbackDevice(jsonConfig, server, conn, loopbacks);
	}
	catch (const nlohmann::json::exception& e)
	{
		std::cerr << "failed to parse gaea-i2c-tool config: " << e.what()
				  << std::endl;
		jsonFile.close();
		return -EINVAL;
	}

	jsonFile.close();
	io.run();

	return 0;
}
