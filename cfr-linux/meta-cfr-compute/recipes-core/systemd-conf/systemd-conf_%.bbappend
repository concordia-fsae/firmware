FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

PACKAGECONFIG = "${@bb.utils.contains('DISTRO_FEATURES', 'static-ip-address', 'static-ethernet', 'dhcp-ethernet', d)}"
PACKAGECONFIG[static-ethernet] = ""

SRC_URI:append = " \
    file://wired-static.network \
"

do_install:append() {
    if ${@bb.utils.contains('PACKAGECONFIG', 'static-ethernet', 'true', 'false', d)}; then
        # The wired static network priority needs to be higher (lower) than the wired ethernet priority (80) or it won't get applied.
        install -d ${D}${sysconfdir}/systemd/network
        install -m 0644 ${WORKDIR}/wired-static.network ${D}${sysconfdir}/systemd/network/75-wired-static.network
    fi
}
