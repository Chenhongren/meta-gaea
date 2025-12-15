SUMMARY = "I2C Loop Test Example"
SECTION = "examples"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = " \
    sdbusplus \
    nlohmann-json \
    "

inherit meson pkgconfig systemd

S = "${WORKDIR}/sources-unpack"

SRC_URI = "file://main.cpp \
           file://dbus-gaea_i2c_tool.conf \
           file://gaea-i2c-tool.service \
           file://meson.build \
          "

SYSTEMD_SERVICE:${PN} = "gaea-i2c-tool.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
