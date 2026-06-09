#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
Usage: tools/bootstrap-carputer.sh [options] <host>

Bootstrap the carcomputer OTA bundle onto a target host over SSH.

Options:
  --local                Run on the carputer using the current payload
  -u, --user USER         SSH user (default: current user)
  -p, --platform PLATFORM Platform name (required)
  -b, --bundle PATH       Use an existing bundle tarball
  -r, --remote-tmp DIR    Remote temp dir for upload (default: /tmp)
  -h, --help              Show this help
EOF
}

HOST=""
USER_NAME=""
PLATFORM=""
BUNDLE=""
REMOTE_TMP="/tmp"
LOCAL_MODE=0

log() {
	echo "[bootstrap] $*"
}

validate_payload_binaries() {
	local release_root="$1"
	local bundle="${2:-}"
	local bin_dir="${release_root}/payload/bin/cfr"
	local bin_path
	local rel_path
	local expected
	local actual
	local readelf_out
	local checked=0

	if [[ ! -d "${bin_dir}" ]]; then
		echo "Missing payload binary directory: ${bin_dir}" >&2
		exit 1
	fi
	if ! command -v file >/dev/null 2>&1 || ! command -v readelf >/dev/null 2>&1; then
		echo "Payload validation requires 'file' and 'readelf'" >&2
		exit 1
	fi

	for bin_path in "${bin_dir}"/*; do
		[[ -f "${bin_path}" ]] || continue
		checked=$((checked + 1))
		actual="$(stat -c %s "${bin_path}")"
		if [[ "${actual}" -le 0 ]]; then
			echo "Invalid zero-length payload binary: ${bin_path}" >&2
			exit 1
		fi
		if [[ -n "${bundle}" ]]; then
			rel_path="./${bin_path#"${release_root}/"}"
			expected="$(tar -tzvf "${bundle}" "${rel_path}" | awk '{print $3}' | head -n1)"
			if [[ -z "${expected}" || "${expected}" -le 0 ]]; then
				echo "Missing archive entry size for ${rel_path}" >&2
				exit 1
			fi
			if [[ "${actual}" != "${expected}" ]]; then
				echo "Size mismatch for ${rel_path}: expected ${expected}, got ${actual}" >&2
				exit 1
			fi
		fi
		if ! file "${bin_path}" | grep -q "ELF"; then
			echo "Payload binary is not an ELF executable: ${bin_path}" >&2
			exit 1
		fi
		readelf_out="$(readelf -h "${bin_path}" 2>&1 || true)"
		if printf '%s\n' "${readelf_out}" | grep -q "Error:"; then
			printf '%s\n' "${readelf_out}" >&2
			echo "Invalid ELF metadata in payload binary: ${bin_path}" >&2
			exit 1
		fi
	done

	if [[ "${checked}" -eq 0 ]]; then
		echo "No payload binaries found in ${bin_dir}" >&2
		exit 1
	fi
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	--local)
		LOCAL_MODE=1
		shift
		;;
	-u | --user)
		USER_NAME="${2:-}"
		shift 2
		;;
	-p | --platform)
		PLATFORM="${2:-}"
		shift 2
		;;
	-b | --bundle)
		BUNDLE="${2:-}"
		shift 2
		;;
	-r | --remote-tmp)
		REMOTE_TMP="${2:-}"
		shift 2
		;;
	-h | --help)
		usage
		exit 0
		;;
	--)
		shift
		break
		;;
	-*)
		echo "Unknown option: $1" >&2
		usage
		exit 2
		;;
	*)
		if [[ -z "${HOST}" ]]; then
			HOST="$1"
			shift
		else
			echo "Unexpected argument: $1" >&2
			usage
			exit 2
		fi
		;;
	esac
done

if [[ "${LOCAL_MODE}" -eq 0 && -z "${HOST}" ]]; then
	usage
	exit 2
fi
if [[ "${LOCAL_MODE}" -eq 0 && -z "${PLATFORM}" ]]; then
	echo "Missing required --platform." >&2
	usage
	exit 2
fi

if [[ "${LOCAL_MODE}" -eq 1 ]]; then
	log "Running local bootstrap"
	state_root="/var/lib/ota-agent/local-deploy"
	release_root="${state_root}/current"
	if [[ ! -d "${release_root}/payload" ]]; then
		echo "Missing payload in ${release_root}" >&2
		exit 1
	fi
	validate_payload_binaries "${release_root}"

	echo "[bootstrap] Updating active payload"
	if [[ -d "${state_root}/active" && ! -L "${state_root}/active" ]]; then
		sudo rm -rf "${state_root}/active"
	fi
	sudo ln -sfn "${release_root}" "${state_root}/active"

	echo "[bootstrap] Seeding base /bin/cfr from payload"
	if [[ -d "${release_root}/payload/bin/cfr" ]]; then
		sudo rm -rf "${state_root}/base/bin-cfr"
		sudo mkdir -p "${state_root}/base/bin-cfr"
		sudo cp -a "${release_root}/payload/bin/cfr/." "${state_root}/base/bin-cfr/"
	fi

	echo "[bootstrap] Installing activation scripts"
	sudo mkdir -p /usr/local/libexec/ota-agent
	sudo cp "${release_root}/bootstrap/ota-agent-drive-stack-activate.sh" \
		/usr/local/libexec/ota-agent/drive-stack-activate.sh
	if [ -f "${release_root}/bootstrap/bootstrap-carputer.sh" ]; then
		sudo install -m 0755 "${release_root}/bootstrap/bootstrap-carputer.sh" \
			/usr/local/bin/bootstrap-carputer
	fi
	if [ -f "${release_root}/bootstrap/bootstrap-startup.sh" ]; then
		sudo cp "${release_root}/bootstrap/bootstrap-startup.sh" \
			/usr/local/libexec/ota-agent/bootstrap-startup.sh
	fi
	sudo cp "${release_root}/bootstrap/ota-agent-drive-stack.service" \
		/etc/systemd/system/ota-agent-drive-stack.service
	sudo chmod 0755 /usr/local/libexec/ota-agent/drive-stack-activate.sh
	if [ -f /usr/local/libexec/ota-agent/bootstrap-startup.sh ]; then
		sudo chmod 0755 /usr/local/libexec/ota-agent/bootstrap-startup.sh
	fi

	echo "[bootstrap] Starting activation service"
	sudo systemctl daemon-reload
	sudo systemctl enable ota-agent-drive-stack.service
	sudo systemctl restart --no-block ota-agent-drive-stack.service ||
		sudo systemctl start --no-block ota-agent-drive-stack.service

	if [ -x /usr/local/libexec/ota-agent/bootstrap-startup.sh ]; then
		echo "[bootstrap] Running bootstrap startup script"
		sudo /usr/local/libexec/ota-agent/bootstrap-startup.sh
	fi

	ota_service_src="${release_root}/payload/etc/systemd/system/ota-agent.service"
	if [ -f "${ota_service_src}" ]; then
		echo "[bootstrap] Installing ota-agent.service from payload"
		sudo cp "${ota_service_src}" /etc/systemd/system/ota-agent.service
		sudo systemctl daemon-reload
	else
		ota_service_legacy="${release_root}/payload/local/ota-agent/etc/systemd/system/ota-agent.service"
		if [ -f "${ota_service_legacy}" ]; then
			echo "[bootstrap] Installing ota-agent.service from legacy payload"
			sudo cp "${ota_service_legacy}" /etc/systemd/system/ota-agent.service
			sudo systemctl daemon-reload
		fi
	fi
	if systemctl list-unit-files --no-legend ota-agent.service | grep -q '^ota-agent.service'; then
		echo "[bootstrap] Restarting ota-agent.service"
		sudo systemctl restart ota-agent.service
	fi
	if systemctl list-unit-files --no-legend bootstrap-carputer.service | grep -q '^bootstrap-carputer.service'; then
		echo "[bootstrap] Disabling bootstrap-carputer.service"
		sudo systemctl disable --now bootstrap-carputer.service || true
	fi
	log "Local bootstrap complete"
	exit 0
fi

if [[ -z "${BUNDLE}" ]]; then
	REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
	BUILD_DIR="$(mktemp -d)"
	trap 'rm -rf "${BUILD_DIR}"' EXIT
	BUNDLE="${BUILD_DIR}/carputer-${PLATFORM}.tgz"
	log "Building bundle ${BUNDLE}"
	(cd "${REPO_ROOT}" && buckle build "//drive-stack/carputer:carputer-package-${PLATFORM}" --out "${BUNDLE}")
fi

if [[ ! -f "${BUNDLE}" ]]; then
	echo "Bundle not found: ${BUNDLE}" >&2
	exit 1
fi

SSH_HOST="${HOST}"
if [[ -n "${USER_NAME}" ]]; then
	SSH_HOST="${USER_NAME}@${HOST}"
fi

REMOTE_BUNDLE="${REMOTE_TMP}/carputer-${PLATFORM}-$(date +%Y%m%d-%H%M%S).tgz"
log "Uploading bundle to ${SSH_HOST}:${REMOTE_BUNDLE}"
scp -q "${BUNDLE}" "${SSH_HOST}:${REMOTE_BUNDLE}"

log "Running remote bootstrap on ${SSH_HOST}"
ssh "${SSH_HOST}" bash -s -- "${REMOTE_BUNDLE}" <<'EOF'
set -euo pipefail

bundle="$1"
state_root="/var/lib/ota-agent/local-deploy"
ts="$(date +%Y%m%d-%H%M%S)"
release_root="${state_root}/releases/boot-${ts}"

validate_payload_binaries() {
    local release_root="$1"
    local bundle="${2:-}"
    local bin_dir="${release_root}/payload/bin/cfr"
    local bin_path
    local rel_path
    local expected
    local actual
    local readelf_out
    local checked=0

    if [ ! -d "${bin_dir}" ]; then
        echo "Missing payload binary directory: ${bin_dir}" >&2
        exit 1
    fi
    if ! command -v file >/dev/null 2>&1 || ! command -v readelf >/dev/null 2>&1; then
        echo "Payload validation requires 'file' and 'readelf'" >&2
        exit 1
    fi

    for bin_path in "${bin_dir}"/*; do
        [ -f "${bin_path}" ] || continue
        checked=$((checked + 1))
        actual="$(stat -c %s "${bin_path}")"
        if [ "${actual}" -le 0 ]; then
            echo "Invalid zero-length payload binary: ${bin_path}" >&2
            exit 1
        fi
        if [ -n "${bundle}" ]; then
            rel_path="./${bin_path#"${release_root}/"}"
            expected="$(tar -tzvf "${bundle}" "${rel_path}" | awk '{print $3}' | head -n1)"
            if [ -z "${expected}" ] || [ "${expected}" -le 0 ]; then
                echo "Missing archive entry size for ${rel_path}" >&2
                exit 1
            fi
            if [ "${actual}" != "${expected}" ]; then
                echo "Size mismatch for ${rel_path}: expected ${expected}, got ${actual}" >&2
                exit 1
            fi
        fi
        if ! file "${bin_path}" | grep -q "ELF"; then
            echo "Payload binary is not an ELF executable: ${bin_path}" >&2
            exit 1
        fi
        readelf_out="$(readelf -h "${bin_path}" 2>&1 || true)"
        if printf '%s\n' "${readelf_out}" | grep -q "Error:"; then
            printf '%s\n' "${readelf_out}" >&2
            echo "Invalid ELF metadata in payload binary: ${bin_path}" >&2
            exit 1
        fi
    done

    if [ "${checked}" -eq 0 ]; then
        echo "No payload binaries found in ${bin_dir}" >&2
        exit 1
    fi
}

echo "[bootstrap] Extracting ${bundle} to ${release_root}"
gzip -t "${bundle}"
sudo mkdir -p "${release_root}"
sudo tar -C "${release_root}" -xzf "${bundle}"
validate_payload_binaries "${release_root}" "${bundle}"
sudo ln -sfn "${release_root}" "${state_root}/current"

echo "[bootstrap] Updating active payload"
if [ -d "${state_root}/active" ] && [ ! -L "${state_root}/active" ]; then
    sudo rm -rf "${state_root}/active"
fi
sudo ln -sfn "${state_root}/current" "${state_root}/active"

echo "[bootstrap] Seeding base /bin/cfr from payload"
if [ -d "${state_root}/current/payload/bin/cfr" ]; then
    sudo rm -rf "${state_root}/base/bin-cfr"
    sudo mkdir -p "${state_root}/base/bin-cfr"
    sudo cp -a "${state_root}/current/payload/bin/cfr/." "${state_root}/base/bin-cfr/"
fi

echo "[bootstrap] Installing activation scripts"
sudo mkdir -p /usr/local/libexec/ota-agent
sudo cp "${state_root}/current/bootstrap/ota-agent-drive-stack-activate.sh" \
    /usr/local/libexec/ota-agent/drive-stack-activate.sh
if [ -f "${state_root}/current/bootstrap/bootstrap-carputer.sh" ]; then
    sudo install -m 0755 "${state_root}/current/bootstrap/bootstrap-carputer.sh" \
        /usr/local/bin/bootstrap-carputer
fi
if [ -f "${state_root}/current/bootstrap/bootstrap-startup.sh" ]; then
    sudo cp "${state_root}/current/bootstrap/bootstrap-startup.sh" \
        /usr/local/libexec/ota-agent/bootstrap-startup.sh
fi
sudo cp "${state_root}/current/bootstrap/ota-agent-drive-stack.service" \
    /etc/systemd/system/ota-agent-drive-stack.service
sudo chmod 0755 /usr/local/libexec/ota-agent/drive-stack-activate.sh
if [ -f /usr/local/libexec/ota-agent/bootstrap-startup.sh ]; then
    sudo chmod 0755 /usr/local/libexec/ota-agent/bootstrap-startup.sh
fi

echo "[bootstrap] Starting activation service"
sudo systemctl daemon-reload
sudo systemctl enable ota-agent-drive-stack.service
sudo systemctl restart --no-block ota-agent-drive-stack.service || \
    sudo systemctl start --no-block ota-agent-drive-stack.service
ota_service_src="${state_root}/current/payload/etc/systemd/system/ota-agent.service"
if [ -f "${ota_service_src}" ]; then
    echo "[bootstrap] Installing ota-agent.service from payload"
    sudo cp "${ota_service_src}" /etc/systemd/system/ota-agent.service
    sudo systemctl daemon-reload
else
    ota_service_legacy="${state_root}/current/payload/local/ota-agent/etc/systemd/system/ota-agent.service"
    if [ -f "${ota_service_legacy}" ]; then
        echo "[bootstrap] Installing ota-agent.service from legacy payload"
        sudo cp "${ota_service_legacy}" /etc/systemd/system/ota-agent.service
        sudo systemctl daemon-reload
    fi
fi
if systemctl list-unit-files --no-legend ota-agent.service | grep -q '^ota-agent.service'; then
    echo "[bootstrap] Restarting ota-agent.service"
    sudo systemctl restart ota-agent.service
fi
if systemctl list-unit-files --no-legend bootstrap-carputer.service | grep -q '^bootstrap-carputer.service'; then
    echo "[bootstrap] Disabling bootstrap-carputer.service"
    sudo systemctl disable --now bootstrap-carputer.service || true
fi
EOF

log "Bootstrap complete on ${HOST}"
