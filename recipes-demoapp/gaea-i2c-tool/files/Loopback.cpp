#include "Loopback.hpp"

#include "Util.hpp"

#include <boost/container/flat_map.hpp>
#include <sdbusplus/bus/match.hpp>

#include <iostream>
#include <numeric>

static constexpr auto i2cToolLoopbackIntf = "xyz.gaea.i2c_tool.loopback";

Loopback::Loopback(const std::string& name,
				   sdbusplus::asio::object_server& server, int bus,
				   uint8_t addr, uint8_t start, uint8_t end,
				   std::shared_ptr<sdbusplus::asio::connection>& conn) :
	objServer(server), lp_name(name), bus(bus), addr(addr), start(start),
	end(end), max_retry(3), debugMode(false), dbusConnection(conn)
{
	std::string path = "/xyz/gaea/i2c_tool/" + lp_name;

	loopbackInterface = objServer.add_interface(path, i2cToolLoopbackIntf);

	loopbackInterface->register_property("maxRetry", max_retry);
	loopbackInterface->register_property(
		"i2cBus", bus, [this](const int& req, int& propertyValue) {
			if (req != propertyValue)
			{
				int fd = openI2CDevice(req);
				if (fd < 0)
				{
					return false;
				}
				close(fd);
				if (updateLoopbackConfig(lp_name, i2cBusProperty, req) != 0)
				{
					std::cerr
						<< "<err> (lp) " << lp_name << " : failed to update \""
						<< i2cBusProperty << " config" << std::endl;
					return false;
				}
				propertyValue = req;
				this->bus = req;
				if (debugMode)
				{
					std::cout
						<< "<dbg> (lp) " << lp_name << " : i2c bus is set as "
						<< std::dec << static_cast<int>(this->bus) << std::endl;
				}
			}
			return true;
		});
	loopbackInterface->register_property(
		"chipAddress", addr,
		[this](const uint8_t& req, uint8_t& propertyValue) {
			if (req != propertyValue)
			{
				if (updateLoopbackConfig(lp_name, chipAddrProperty, req) != 0)
				{
					std::cerr
						<< "<err> (lp) " << lp_name << " : failed to update \""
						<< chipAddrProperty << " config" << std::endl;
					return false;
				}
				propertyValue = req;
				this->addr = req;
				if (debugMode)
				{
					std::cout << "<dbg> (lp) " << lp_name
							  << " : chip address is set as 0x" << std::hex
							  << static_cast<int>(this->addr) << std::dec
							  << std::endl;
				}
			}
			return true;
		});
	loopbackInterface->register_property(
		"startValue", start,
		[this](const uint8_t& req, uint8_t& propertyValue) {
			if (req != propertyValue)
			{
				if (req >= this->end)
				{
					std::cerr
						<< "<err> (lp) " << lp_name
						<< " : start should be larger than end(" << std::dec
						<< static_cast<int>(this->end) << ")" << std::endl;
					return false;
				}
				if (updateLoopbackConfig(lp_name, startValueProperty, req) != 0)
				{
					std::cerr
						<< "<err> (lp) " << lp_name << " : failed to update \""
						<< startValueProperty << " config" << std::endl;
					return false;
				}
				propertyValue = req;
				this->start = req;
				if (debugMode)
				{
					std::cout << "<dbg> (lp) " << lp_name
							  << " : start value is set as " << std::dec
							  << static_cast<int>(this->start) << std::endl;
				}
			}
			return true;
		});
	loopbackInterface->register_property(
		"endValue", end, [this](const uint8_t& req, uint8_t& propertyValue) {
			if (req != propertyValue)
			{
				if (req <= this->start)
				{
					std::cerr
						<< "<err> (lp) " << lp_name
						<< " : end should be less than start(" << std::dec
						<< static_cast<int>(this->start) << ")" << std::endl;
					return false;
				}
				if (updateLoopbackConfig(lp_name, endValueProperty, req) != 0)
				{
					std::cerr
						<< "<err> (lp) " << lp_name << " : failed to update \""
						<< endValueProperty << " config" << std::endl;
					return false;
				}
				propertyValue = req;
				this->end = req;
				if (debugMode)
				{
					std::cout
						<< "<dbg> (lp) " << lp_name << " : end value is set as "
						<< std::dec << static_cast<int>(this->end) << std::endl;
				}
			}
			return true;
		});
	loopbackInterface->register_method(
		"perform", [this](boost::asio::yield_context yield) -> int {
			return this->performLoopbackTest(yield);
		});

	loopbackInterface->initialize();

	if (debugModeChangeMatch)
	{
		std::cout << "<warning> property match of debugMode has been registered"
				  << std::endl;
	}
	else
	{
		using namespace sdbusplus::bus::match::rules;
		debugModeChangeMatch = std::make_unique<sdbusplus::bus::match_t>(
			static_cast<sdbusplus::bus_t&>(*dbusConnection),
			path_namespace("/xyz/gaea/i2c_tool") + type::signal() +
				member("PropertiesChanged") +
				interface("org.freedesktop.DBus.Properties"),
			[this](sdbusplus::message_t& m) {
				std::string interfaceName;
				boost::container::flat_map<std::string, std::variant<bool>>
					propertiesChanged;

				m.read(interfaceName, propertiesChanged);
				if (interfaceName != "xyz.gaea.i2c_tool.debug")
				{
					return;
				}
				auto itr = propertiesChanged.find("debugMode");
				if (itr == propertiesChanged.end())
				{
					return;
				}
				if (auto* v = std::get_if<bool>(&itr->second))
				{
					debugMode = *v;
					std::cout
						<< "<info> (lp) " << lp_name
						<< " : monitor debugMode is "
						<< (debugMode ? "enable" : "disabled") << std::endl;
				}
			});
	}

	std::cout << "<info> loopback device " << lp_name << " is created"
			  << std::endl;
}

Loopback::~Loopback()
{
	debugModeChangeMatch.reset();
	objServer.remove_interface(loopbackInterface);
	std::cout << "<info> loopback device " << lp_name << " is removed"
			  << std::endl;
}

int Loopback::performLoopbackTest(boost::asio::yield_context)
{
	if (debugMode)
	{
		std::cout << "<dbg> " << lp_name << " : start loopback test..."
				  << std::endl;
	}

	int fd = openI2CDevice(bus);
	if (fd < 0)
	{
		return -ENODEV;
	}

	for (uint16_t val = start; val <= end; ++val)
	{
		std::vector<uint8_t> tx(16, 0), rx(16, 0);
		int retry = 0;

		while (retry < max_retry)
		{
			std::iota(tx.begin(), tx.begin() + tx.size(),
					  static_cast<uint8_t>(val));
			if (i2c_write_internal(fd, addr, tx.data(), tx.size()))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				retry++;
				continue;
			}
			break;
		}

		if (retry == max_retry)
		{
			std::cerr << "loopback: failed to write i2c device" << std::endl;
			close(fd);
			return -ETIMEDOUT;
		}

		retry = 0;
		while (retry < max_retry)
		{
			if (i2c_read_internal(fd, addr, rx.data(), rx.size()))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				retry++;
				continue;
			}
			break;
		}

		if (retry == max_retry)
		{
			std::cerr << "loopback: failed to read i2c device" << std::endl;
			close(fd);
			return -ETIMEDOUT;
		}

		if (rx != tx)
		{
			std::cerr << "loopback: mismatch at value " << std::dec << val;
			std::cerr << "\ntx: ";
			for (auto i : tx)
			{
				std::cerr << std::hex << static_cast<int>(i) << " ";
			}
			std::cerr << "\nrx: ";
			for (auto i : rx)
			{
				std::cerr << std::hex << static_cast<int>(i) << " ";
			}
			std::cerr << std::dec << std::endl;
			close(fd);
			return -EILSEQ;
		}
	}

	close(fd);
	return 0;
}
