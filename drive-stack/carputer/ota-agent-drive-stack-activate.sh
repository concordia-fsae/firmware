#!/usr/bin/env bash
set -euo pipefail

STATE_ROOT="/var/lib/ota-agent/local-deploy"
if [ -n "${1:-}" ]; then
    RELEASE_ROOT="$1"
elif [ -d "${STATE_ROOT}/active/payload" ]; then
    RELEASE_ROOT="${STATE_ROOT}/active"
else
    RELEASE_ROOT="${STATE_ROOT}/current"
fi
PAYLOAD_DIR="${RELEASE_ROOT}/payload"
RUNTIME_ROOT="${STATE_ROOT}/runtime"
BASE_BIN_ROOT="${STATE_ROOT}/base/bin-cfr"
UPPER_BIN_ROOT="${RUNTIME_ROOT}/bin-cfr/upper"
WORK_BIN_ROOT="${RUNTIME_ROOT}/bin-cfr/work"
MERGED_BIN_ROOT="/run/ota-agent-drive-stack/bin-cfr"

log() {
    echo "[ota-agent-activate] $*"
}

try_umount() {
    local target="$1"
    if mountpoint -q "${target}"; then
        log "Unmounting ${target}"
        if ! umount "${target}" 2>/dev/null; then
            log "umount failed for ${target}, attempting lazy unmount"
            umount -l "${target}" 2>/dev/null || true
        fi
    fi
}

log "Release root: ${RELEASE_ROOT}"
log "Payload dir: ${PAYLOAD_DIR}"
if [ ! -d "${PAYLOAD_DIR}" ]; then
    log "missing payload directory: ${PAYLOAD_DIR}"
    exit 1
fi

log "Creating directories for overlay"
mkdir -p "${STATE_ROOT}/base"
mkdir -p "${RUNTIME_ROOT}/bin-cfr"
mkdir -p "${MERGED_BIN_ROOT}"
mkdir -p /bin/cfr
mkdir -p /application

if [ ! -f "${BASE_BIN_ROOT}/.base-seeded" ]; then
    log "Seeding base bin from /bin/cfr"
    mkdir -p "${BASE_BIN_ROOT}"
    if [ -d /bin/cfr ]; then
        cp -a /bin/cfr/. "${BASE_BIN_ROOT}/" 2>/dev/null || true
    fi
    touch "${BASE_BIN_ROOT}/.base-seeded"
fi

rm -rf "${UPPER_BIN_ROOT}" "${WORK_BIN_ROOT}"
mkdir -p "${UPPER_BIN_ROOT}" "${WORK_BIN_ROOT}"
if [ -d "${PAYLOAD_DIR}/bin/cfr" ]; then
    log "Staging /bin/cfr payload"
    cp -a "${PAYLOAD_DIR}/bin/cfr/." "${UPPER_BIN_ROOT}/"
fi
if [ -d "${PAYLOAD_DIR}/local" ]; then
    for local_bin in "${PAYLOAD_DIR}"/local/*/bin/cfr; do
        [ -d "${local_bin}" ] || continue
        log "Staging local /bin/cfr payload from ${local_bin} (legacy)"
        cp -a "${local_bin}/." "${UPPER_BIN_ROOT}/"
    done
fi

try_umount "${MERGED_BIN_ROOT}"
log "Mounting overlay at ${MERGED_BIN_ROOT}"
mount -t overlay overlay -o "lowerdir=${BASE_BIN_ROOT},upperdir=${UPPER_BIN_ROOT},workdir=${WORK_BIN_ROOT}" "${MERGED_BIN_ROOT}"

if [ -d /usr/bin/cfr ] || mountpoint -q /usr/bin/cfr; then
    try_umount /usr/bin/cfr
fi
try_umount /bin/cfr
log "Bind mounting ${MERGED_BIN_ROOT} to /bin/cfr"
mount --bind "${MERGED_BIN_ROOT}" /bin/cfr

log "Creating /usr/local/bin symlinks for /bin/cfr binaries"
mkdir -p /usr/local/bin
for link_path in /usr/local/bin/*; do
    [ -L "${link_path}" ] || continue
    target="$(readlink "${link_path}" || true)"
    if [[ "${target}" == /bin/cfr/* ]]; then
        rm -f "${link_path}"
    fi
done
for bin_path in /bin/cfr/*; do
    [ -f "${bin_path}" ] || continue
    bin_name="$(basename "${bin_path}")"
    ln -sf "${bin_path}" "/usr/local/bin/${bin_name}"
done

log "Copying payload to /application and other roots"
allowed_roots=("application" "etc" "usr" "var" "opt" "lib")
is_allowed_root() {
    local root="$1"
    for allowed in "${allowed_roots[@]}"; do
        if [ "${root}" = "${allowed}" ]; then
            return 0
        fi
    done
    return 1
}
for entry in "${PAYLOAD_DIR}"/*; do
    [ -e "${entry}" ] || continue
    top_level="$(basename "${entry}")"
    if [ "${top_level}" = "bin" ] || [ "${top_level}" = "bootstrap" ]; then
        continue
    fi
    if ! is_allowed_root "${top_level}"; then
        log "Skipping unsupported top-level payload entry: ${top_level}"
        continue
    fi

    dest="/${top_level}"
    mkdir -p "${dest}"
    if [ -d "${entry}" ]; then
        cp -a "${entry}/." "${dest}/"
    else
        cp -a "${entry}" "${dest}"
    fi
done

log "Installing systemd units from payload"
if [ -d "${PAYLOAD_DIR}/etc/systemd/system" ]; then
    cp -a "${PAYLOAD_DIR}/etc/systemd/system/." /etc/systemd/system/
fi
if [ -d "${PAYLOAD_DIR}/local" ]; then
    for unit_dir in "${PAYLOAD_DIR}"/local/*/etc/systemd/system; do
        [ -d "${unit_dir}" ] || continue
        log "Installing local systemd units from ${unit_dir} (legacy)"
        cp -a "${unit_dir}/." /etc/systemd/system/
    done
fi
