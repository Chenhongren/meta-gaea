PACKAGECONFIG ??= "elf bin dat"

PACKAGECONFIG[elf] = ",,"
PACKAGECONFIG[bin] = ",,"
PACKAGECONFIG[dat] = ",,"

do_deploy:append() {
    # elf files
    if ! ${@bb.utils.contains('PACKAGECONFIG', 'elf', 'true', 'false', d)}; then
        rm -f ${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/*.elf
    fi

    # bin files
    if ! ${@bb.utils.contains('PACKAGECONFIG', 'bin', 'true', 'false', d)}; then
        rm -f ${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/*.bin
    fi

    # dat files
    if ! ${@bb.utils.contains('PACKAGECONFIG', 'dat', 'true', 'false', d)}; then
        rm -f ${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/*.dat
    fi

    stamp=${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/${PN}-${PV}.stamp
    touch ${stamp}
    echo "DEPLOY_ELF=${@bb.utils.contains('PACKAGECONFIG','elf','yes','no',d)}" >> ${stamp}
    echo "DEPLOY_BIN=${@bb.utils.contains('PACKAGECONFIG','bin','yes','no',d)}" >> ${stamp}
    echo "DEPLOY_DAT=${@bb.utils.contains('PACKAGECONFIG','dat','yes','no',d)}" >> ${stamp}
}
