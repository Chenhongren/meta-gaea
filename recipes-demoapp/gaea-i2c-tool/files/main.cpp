#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <variant>

using variant = std::variant<int, std::string>;

static constexpr auto gataI2CToolServiceName = "xyz.gaea.i2c_tool";
static constexpr auto gataI2CToolObjName = "/xyz/gaea/i2c_tool";
static constexpr auto gataI2CToolLoopbackIntf =
	"xyz.gaea.i2c_tool.loopback_test";
static constexpr auto gataI2CToolMethodIntf = "xyz.gaea.i2c_tool.methods";
static constexpr auto gataI2CToolDebugIntf = "xyz.gaea.i2c_tool.debug";

const std::string configFile = "/var/lib/gaea-i2c-tool/gaea-i2c-loopback.json";
static constexpr auto i2cBusProperty = "i2c_bus";
static constexpr auto chipAddrProperty = "chip_addr";
static constexpr auto startValueProperty = "start_value";
static constexpr auto endValueProperty = "end_value";

static bool debugMode = false;

struct loopback_i2c_info_t
{
	int bus;
	uint8_t addr;
	uint8_t start;
	uint8_t end;
	int max_retry = 3;
} loopback_i2c_info;

static int i2c_transfer_internal(int fd, uint8_t chip_addr, uint8_t reg,
								 uint8_t* val, size_t len)
{
	struct i2c_msg msgs[2]{};
	struct i2c_rdwr_ioctl_data data{};

	msgs[0].addr = chip_addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	msgs[1].addr = chip_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = val;

	data.msgs = msgs;
	data.nmsgs = 2;

	int ret = ioctl(fd, I2C_RDWR, &data);
	if (ret < 0)
	{
		perror("failed to xfer i2c data");
		return ret;
	}

	return 0;
}

static int i2c_read_internal(int fd, uint8_t chip_addr, uint8_t* val,
							 size_t len)
{
	struct i2c_msg msg{};
	struct i2c_rdwr_ioctl_data data{};

	msg.addr = chip_addr;
	msg.flags = I2C_M_RD;
	msg.len = static_cast<__u16>(len);
	msg.buf = val;

	data.msgs = &msg;
	data.nmsgs = 1;

	int ret = ioctl(fd, I2C_RDWR, &data);
	if (ret < 0)
	{
		perror("failed to read i2c data");
		return ret;
	}

	return 0;
}

static int i2c_write_internal(int fd, uint8_t chip_addr, uint8_t* data,
							  size_t len)
{
	if (len == 0 || !data)
	{
		return -EINVAL;
	}

	struct i2c_msg msg{};
	struct i2c_rdwr_ioctl_data ioctl_data{};

	msg.addr = chip_addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = const_cast<uint8_t*>(data);

	ioctl_data.msgs = &msg;
	ioctl_data.nmsgs = 1;

	int ret = ioctl(fd, I2C_RDWR, &ioctl_data);
	if (ret < 0)
	{
		perror("failed to write i2c data");
		return ret;
	}

	return 0;
}

static auto openI2CDevice(int bus)
{
	char dev[32];
	std::snprintf(dev, sizeof(dev), "/dev/i2c-%d", bus);
	int fd = open(dev, O_RDWR);
	if (fd < 0)
	{
		perror("failed to open i2c device");
	}

	return fd;
}

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
	return ret;
}

int perform_loop_test(boost::asio::yield_context)
{
	if (debugMode)
	{
		std::cout << "<dbg> loopback: start loopback test..." << std::endl;
	}

	int fd = openI2CDevice(loopback_i2c_info.bus);
	if (fd < 0)
	{
		return -ENODEV;
	}

	for (uint16_t val = loopback_i2c_info.start; val <= loopback_i2c_info.end;
		 ++val)
	{
		std::vector<uint8_t> tx(16, 0), rx(16, 0);
		int retry = 0;

		while (retry < loopback_i2c_info.max_retry)
		{
			std::iota(tx.begin(), tx.begin() + tx.size(),
					  static_cast<uint8_t>(val));
			if (i2c_write_internal(fd, loopback_i2c_info.addr, tx.data(),
								   tx.size()))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				retry++;
				continue;
			}
			break;
		}

		if (retry == loopback_i2c_info.max_retry)
		{
			std::cerr << "loopback: failed to write i2c device" << std::endl;
			close(fd);
			return -ETIMEDOUT;
		}

		retry = 0;
		while (retry < loopback_i2c_info.max_retry)
		{
			if (i2c_read_internal(fd, loopback_i2c_info.addr, rx.data(),
								  rx.size()))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				retry++;
				continue;
			}
			break;
		}

		if (retry == loopback_i2c_info.max_retry)
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
			return -EILSEQ;
		}
	}

	close(fd);
	return 0;
}

static auto parseConfigFile(void)
{
	std::ifstream jsonFile(configFile);
	if (!jsonFile.is_open())
	{
		std::cerr << "missing gaea-i2c-tool config" << std::endl;
		return -EINVAL;
	}

	try
	{
		auto jsonConfig = nlohmann::json::parse(jsonFile, nullptr, true);
		jsonConfig.at(i2cBusProperty).get_to(loopback_i2c_info.bus);
		jsonConfig.at(chipAddrProperty).get_to(loopback_i2c_info.addr);
		jsonConfig.at(startValueProperty).get_to(loopback_i2c_info.start);
		jsonConfig.at(endValueProperty).get_to(loopback_i2c_info.end);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::cerr << "failed to parse gaea-i2c-tool config" << std::endl;
		return -EINVAL;
	}

	return 0;
}

int updateJsonConfig(auto propertyName, auto value)
{
	nlohmann::json cfg;

	std::ifstream ifs(configFile);
	if (ifs.is_open())
	{
		try
		{
			ifs >> cfg;
		}
		catch (...)
		{
			std::cerr << "failed to parse json, using empty object"
					  << std::endl;
			return -errno;
		}
		ifs.close();
	}
	else
	{
		cfg = nlohmann::json::object();
	}

	cfg[propertyName] = value;

	std::ofstream ofs(configFile);
	if (!ofs.is_open())
	{
		std::cerr << "failed to open config for writing" << std::endl;
		return -errno;
	}
	ofs << cfg.dump(4) << std::endl;
	ofs.close();
	return 0;
}

static void registerLoopbackInterface(sdbusplus::asio::object_server& server)
{
	std::shared_ptr<sdbusplus::asio::dbus_interface> loopback_iface =
		server.add_interface(gataI2CToolObjName, gataI2CToolLoopbackIntf);
	loopback_iface->register_property("max_retry", loopback_i2c_info.max_retry);

	loopback_iface->register_property("i2c_bus", loopback_i2c_info.bus,
									  [&](const int& req, int& propertyValue) {
		if (req != propertyValue)
		{
			int fd = openI2CDevice(req);
			if (fd < 0)
			{
				return false;
			}
			close(fd);
			if (updateJsonConfig(i2cBusProperty, loopback_i2c_info.bus))
			{
				return false;
			}
			propertyValue = req;
			loopback_i2c_info.bus = req;
			if (debugMode)
			{
				std::cout << "<dbg> loopback: i2c bus is set as " << std::hex
						  << static_cast<int>(loopback_i2c_info.bus) << std::dec
						  << std::endl;
			}
		}
		return true;
	});
	loopback_iface->register_property(
		"chip_addr", loopback_i2c_info.addr,
		[&](const uint8_t& req, uint8_t& propertyValue) {
		if (req != propertyValue)
		{
			if (updateJsonConfig(chipAddrProperty, loopback_i2c_info.addr))
			{
				return false;
			}
			propertyValue = req;
			loopback_i2c_info.addr = req;
			if (debugMode)
			{
				std::cout << "<dbg> loopback: chip address is set as "
						  << std::hex
						  << static_cast<int>(loopback_i2c_info.addr)
						  << std::dec << std::endl;
			}
		}
		return true;
	});
	loopback_iface->register_property(
		"start_value", loopback_i2c_info.start,
		[&](const uint8_t& req, uint8_t& propertyValue) {
		if (req != propertyValue)
		{
			if (req >= loopback_i2c_info.end)
			{
				std::cerr << "start should be larger than end("
						  << static_cast<int>(loopback_i2c_info.end) << ")"
						  << std::endl;
				return false;
			}
			if (updateJsonConfig(startValueProperty, loopback_i2c_info.start))
			{
				return false;
			}
			propertyValue = req;
			loopback_i2c_info.start = req;
			if (debugMode)
			{
				std::cout << "<dbg> loopback: start value is set as "
						  << static_cast<int>(loopback_i2c_info.start)
						  << std::endl;
			}
		}
		return true;
	});
	loopback_iface->register_property(
		"end_value", loopback_i2c_info.end,
		[&](const uint8_t& req, uint8_t& propertyValue) {
		if (req != propertyValue)
		{
			if (req <= loopback_i2c_info.start)
			{
				std::cerr << "end should be less than start("
						  << static_cast<int>(loopback_i2c_info.start) << ")"
						  << std::endl;
				return false;
			}
			if (updateJsonConfig(endValueProperty, loopback_i2c_info.end))
			{
				return false;
			}
			propertyValue = req;
			loopback_i2c_info.end = req;
			if (debugMode)
			{
				std::cout << "<dbg> loopback: end value is set as "
						  << static_cast<int>(loopback_i2c_info.end)
						  << std::endl;
			}
		}
		return true;
	});
	loopback_iface->register_method("loopback_test", perform_loop_test);
	loopback_iface->initialize();
}

static void registerMethodInterface(sdbusplus::asio::object_server& server)
{
	std::shared_ptr<sdbusplus::asio::dbus_interface> method_iface =
		server.add_interface(gataI2CToolObjName, gataI2CToolMethodIntf);
	method_iface->register_method("write", perform_i2c_write);
	method_iface->register_method("read", perform_i2c_read);
	method_iface->register_method("transfer", perform_i2c_transfer);
	method_iface->initialize();
}

static void registerDebugInterface(sdbusplus::asio::object_server& server)
{
	std::shared_ptr<sdbusplus::asio::dbus_interface> debug_iface =
		server.add_interface(gataI2CToolObjName, gataI2CToolDebugIntf);
	debug_iface->register_property("debug_mode", debugMode,
								   [&](const bool& req, bool& propertyValue) {
		if (propertyValue != req)
		{
			std::cout << "<dbg> debug mode is " << (req ? "enable" : "disabled")
					  << std::endl;
			propertyValue = req;
			debugMode = req;
		}
		return true;
	});
	debug_iface->initialize();
}

int main(void)
{
	int ret = parseConfigFile();
	if (ret)
	{
		return ret;
	}

	boost::asio::io_context io;
	auto conn = std::make_shared<sdbusplus::asio::connection>(io);
	conn->request_name(gataI2CToolServiceName);
	auto server = sdbusplus::asio::object_server(conn);

	registerLoopbackInterface(server);
	registerMethodInterface(server);
	registerDebugInterface(server);

	io.run();

	return 0;
}
