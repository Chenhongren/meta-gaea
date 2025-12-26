#include "Util.hpp"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>

int i2c_transfer_internal(int fd, uint8_t chip_addr, uint8_t reg, uint8_t* val,
						  size_t len)
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

int i2c_read_internal(int fd, uint8_t chip_addr, uint8_t* val, size_t len)
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

int i2c_write_internal(int fd, uint8_t chip_addr, uint8_t* data, size_t len)
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

int openI2CDevice(int bus)
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

int updateLoopbackConfig(const std::string& name, const std::string& property,
						 const nlohmann::json& value)
{
	nlohmann::json cfg;

	/* read */
	std::ifstream ifs(configFile);
	if (!ifs)
	{
		std::cerr << "failed to open config for reading\n";
		return -EIO;
	}

	try
	{
		ifs >> cfg;
	}
	catch (const std::exception& e)
	{
		std::cerr << "failed to parse json: " << e.what() << '\n';
		return -EINVAL;
	}

	/* validate structure */
	if (!cfg.contains("loopback_device") || !cfg["loopback_device"].is_array())
	{
		std::cerr << "invalid config: loopback_device missing or not array\n";
		return -EINVAL;
	}

	bool found = false;
	for (auto& item : cfg["loopback_device"])
	{
		if (item.contains("name") && item["name"] == name)
		{
			item[property] = value;
			found = true;
			break;
		}
	}

	if (!found)
	{
		std::cerr << "loopback device not found: " << name << '\n';
		return -ENOENT;
	}

	/* atomic write */
	const std::string tmp = configFile + ".tmp";
	std::ofstream ofs(tmp);
	if (!ofs)
	{
		std::cerr << "failed to open temp config for writing\n";
		return -EIO;
	}

	ofs << cfg.dump(4) << std::endl;
	ofs.close();

	if (std::rename(tmp.c_str(), configFile.c_str()) != 0)
	{
		std::cerr << "failed to replace config file\n";
		return -EIO;
	}

	return 0;
}
