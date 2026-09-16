#!/usr/bin/env bash
set -Eeuo pipefail

VERSION="31.2.0"
OUTPUT_DIR="${OUTPUT_DIR:-$PWD}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Run this packaging step on a Mac; it requires hdiutil, lipo, and codesign." >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    set -- \
        "ELARVON-Core-${VERSION}-macos-arm64-unsigned.zip" \
        "ELARVON-Core-${VERSION}-macos-x86_64-unsigned.zip"
fi

mkdir -p "${OUTPUT_DIR}"
work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT

sign_app() {
    local app_path="$1"
    if [[ -n "${APPLE_SIGN_IDENTITY:-}" ]]; then
        codesign --force --deep --options runtime --timestamp \
            --sign "${APPLE_SIGN_IDENTITY}" "${app_path}"
        codesign --verify --deep --strict --verbose=2 "${app_path}"
    else
        codesign --force --deep --sign - "${app_path}"
    fi
}

notarize_if_configured() {
    local dmg_path="$1"
    if [[ -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" ]]; then
        xcrun notarytool submit "${dmg_path}" --wait \
            --apple-id "${APPLE_ID}" \
            --team-id "${APPLE_TEAM_ID}" \
            --password "${APPLE_APP_PASSWORD}"
        xcrun stapler staple "${dmg_path}"
    fi
}

make_dmg() {
    local app_path="$1"
    local architecture="$2"
    local volume_dir="${work_dir}/volume-${architecture}"
    local dmg_path="${OUTPUT_DIR}/ELARVON-Core-${VERSION}-${architecture}.dmg"

    rm -rf "${volume_dir}"
    mkdir -p "${volume_dir}"
    ditto "${app_path}" "${volume_dir}/ELARVON-Core.app"
    ln -s /Applications "${volume_dir}/Applications"
    hdiutil create -volname "ELARVON Core" -srcfolder "${volume_dir}" \
        -ov -format UDZO "${dmg_path}"

    if [[ -n "${APPLE_SIGN_IDENTITY:-}" ]]; then
        codesign --force --timestamp --sign "${APPLE_SIGN_IDENTITY}" "${dmg_path}"
    fi
    notarize_if_configured "${dmg_path}"
    shasum -a 256 "${dmg_path}"
}

arm_app=""
intel_app=""
for archive in "$@"; do
    if [[ ! -f "${archive}" ]]; then
        echo "Missing macOS application archive: ${archive}" >&2
        exit 1
    fi

    case "${archive}" in
        *arm64*) architecture="arm64" ;;
        *x86_64*) architecture="x86_64" ;;
        *) echo "Cannot determine architecture from ${archive}." >&2; exit 1 ;;
    esac

    unpack_dir="${work_dir}/unpacked-${architecture}"
    mkdir -p "${unpack_dir}"
    ditto -x -k "${archive}" "${unpack_dir}"
    app_path="$(find "${unpack_dir}" -maxdepth 3 -type d -name 'ELARVON-Core.app' -print -quit)"
    if [[ -z "${app_path}" ]]; then
        echo "ELARVON-Core.app was not found in ${archive}." >&2
        exit 1
    fi

    sign_app "${app_path}"
    make_dmg "${app_path}" "${architecture}"
    if [[ "${architecture}" == "arm64" ]]; then arm_app="${app_path}"; else intel_app="${app_path}"; fi
done

if [[ -n "${arm_app}" && -n "${intel_app}" ]]; then
    universal_app="${work_dir}/universal/ELARVON-Core.app"
    mkdir -p "$(dirname "${universal_app}")"
    ditto "${arm_app}" "${universal_app}"
    codesign --remove-signature "${universal_app}" 2>/dev/null || true
    lipo -create \
        "${arm_app}/Contents/MacOS/ELARVON-Core" \
        "${intel_app}/Contents/MacOS/ELARVON-Core" \
        -output "${universal_app}/Contents/MacOS/ELARVON-Core"
    lipo -info "${universal_app}/Contents/MacOS/ELARVON-Core"
    sign_app "${universal_app}"
    make_dmg "${universal_app}" "universal"
fi

echo "ELARVON macOS DMG packaging completed."
