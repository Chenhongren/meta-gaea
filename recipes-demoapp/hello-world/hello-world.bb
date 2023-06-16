SUMMARY = "Hello World Demo"
SECTION = "apps"
LICENSE = "CLOSED"

APP_NAME = "hello-world"
localdir = "/usr/local"
bindir = "${localdir}/bin"

TARGET_CC_ARCH += "${LDFLAGS}"

SRC_URI = "file://main.c \
           file://Makefile \
	   "

S = "${WORKDIR}"

do_compile() {
    make -f Makefile

}

do_install () {
    install -m 0755 -d ${D}${localdir}
    install -m 0755 -d ${D}${bindir}
    cd ${S}
    install -m 0755 ${APP_NAME} ${D}${bindir}
}

FILES_${PN}-dev = ""
FILES_${PN} = "${bindir}/*"

