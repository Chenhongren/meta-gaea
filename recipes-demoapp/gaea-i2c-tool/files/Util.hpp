#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

int i2c_transfer_internal(int fd, uint8_t chip_addr, uint8_t reg, uint8_t* val, size_t len);
int i2c_read_internal(int fd, uint8_t chip_addr, uint8_t* val, size_t len);
int i2c_write_internal(int fd, uint8_t chip_addr, uint8_t* data, size_t len);
int openI2CDevice(int bus);

int updateLoopbackConfig(const std::string& name, const std::string& property, const nlohmann::json& value);

static constexpr auto nameProperty = "name";
static constexpr auto i2cBusProperty = "i2c_bus";
static constexpr auto chipAddrProperty = "chip_addr";
static constexpr auto startValueProperty = "start_value";
static constexpr auto endValueProperty = "end_value";

const std::string configFile = "/var/lib/gaea-i2c-tool/gaea-i2c-loopback.json";
