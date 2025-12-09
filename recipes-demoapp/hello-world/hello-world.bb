SUMMARY = "Hello World C++ Example using Meson build system"
DESCRIPTION = "Simple C++ Hello World application built using Meson"
SECTION = "examples"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit meson pkgconfig

S = "${WORKDIR}/sources-unpack"

SRC_URI = "file://main.cpp \
           file://meson.build \
          "
