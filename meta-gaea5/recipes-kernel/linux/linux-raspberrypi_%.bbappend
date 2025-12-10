FILESEXTRAPATHS:prepend := "${THISDIR}/linux-raspberrypi:"

SRC_URI:append:gaea5 = " \
    file://gaea5.config \
    file://gaea5.dts \
"

KBUILD_DEFCONFIG:gaea5 = "gaea5.config"

do_kernel_metadata:prepend:gaea5() {
    echo "[${MACHINE}] install defconfig before kernel metadata"
    install -m 0644 ${WORKDIR}/sources-unpack/gaea5.config \
        ${STAGING_KERNEL_DIR}/arch/${ARCH}/configs/gaea5.config
}

do_patch:append:gaea5() {
    for DTB in ${KERNEL_DEVICETREE}; do
        DT=$(basename "${DTB}" .dtb)
        if [ -r "${UNPACKDIR}/${DT}.dts" ]; then
            cp ${UNPACKDIR}/${DT}.dts \
                ${STAGING_KERNEL_DIR}/arch/${ARCH}/boot/dts/broadcom/
        fi
    done
}
