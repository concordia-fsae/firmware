FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

FILES:${PN}:append = "${systemd_unitdir}/network/75-wired-static.network"

SRC_URI:append = " \
    file://wired-static.network \
"

do_install:append() {
    # The wired static network priority needs to be higher (lower) than the wired ethernet priority (80) or it won't get applied.
    install -m 0644 ${WORKDIR}/wired-static.network ${D}${systemd_unitdir}/network/75-wired-static.network
}
