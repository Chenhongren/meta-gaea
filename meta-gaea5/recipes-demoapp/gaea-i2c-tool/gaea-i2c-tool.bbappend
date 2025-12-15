FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
    file://gaea-i2c-loopback.json \
    "

do_install:append() {
    install -d ${D}/var/lib/gaea-i2c-tool
    install -m 0755 ${UNPACKDIR}/gaea-i2c-loopback.json ${D}/var/lib/gaea-i2c-tool/gaea-i2c-loopback.json
}
