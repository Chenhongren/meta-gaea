# Machine name and firmware version for tag
MACHINE_NAME = "Gaea5"
FW_VERSION = "1.0-init"
CODENAME = "styhead"

PRETTY_NAME = "${MACHINE_NAME} ${FW_VERSION} (Base: ${DISTRO_NAME} ${VERSION})"
VERSION_ID = "${FW_VERSION}"
VERSION = "${FW_VERSION}"
ID = "${MACHINE_NAME}"
DISTRO_CODENAME = "${CODENAME}"

def run_git(d, cmd):
    try:
        path = d.getVar('GAEA_COREBASE', True)
        return bb.process.run(("export PSEUDO_DISABLED=1; " + "git -C %s %s")
                                % (path, cmd))[0].strip('\n')
    except Exception as e:
        bb.warn("Unexpected exception from 'git' call: %s" % e)
        pass

CODE_ID = "${@run_git(d, 'describe --tags --dirty --always')}"

OS_RELEASE_FIELDS = "\
    ID BUILD_ID NAME VERSION VERSION_ID VERSION_CODENAME PRETTY_NAME \
    CODE_ID \
"

BB_DONT_CACHE = "1"
