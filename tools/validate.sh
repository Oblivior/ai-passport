#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_model.c main/pet_model.c \
        -o "${test_dir}/test_pet_model"
    "${test_dir}/test_pet_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_life.c main/pet_life.c main/pet_model.c main/pet_protocol.c \
        -o "${test_dir}/test_pet_life"
    "${test_dir}/test_pet_life"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_lunch.c main/pet_lunch.c main/pet_life.c main/pet_model.c \
        -o "${test_dir}/test_pet_lunch"
    "${test_dir}/test_pet_lunch"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_house.c main/pet_house.c main/pet_catalog.c main/pet_life.c main/pet_model.c \
        -o "${test_dir}/test_pet_house"
    "${test_dir}/test_pet_house"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_settlement.c main/pet_house.c main/pet_catalog.c main/pet_life.c main/pet_model.c main/pet_protocol.c \
        -o "${test_dir}/test_pet_settlement"
    "${test_dir}/test_pet_settlement"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_ui_progress.c main/pet_ui_text.c main/pet_catalog.c \
        -o "${test_dir}/test_pet_ui_progress"
    "${test_dir}/test_pet_ui_progress"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Itests/nvs \
        tests/test_pet_house_store.c main/pet_house_store.c main/pet_house.c main/pet_catalog.c main/pet_life.c main/pet_model.c \
        -o "${test_dir}/test_pet_house_store"
    "${test_dir}/test_pet_house_store"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_training.c main/pet_training.c main/pet_bond.c main/pet_catalog.c main/pet_life.c main/pet_model.c \
        -o "${test_dir}/test_pet_training"
    "${test_dir}/test_pet_training"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_pet_meet.c main/pet_meet.c main/pet_catalog.c \
        -o "${test_dir}/test_pet_meet"
    "${test_dir}/test_pet_meet"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Itests/nvs \
        tests/test_pet_bond_store.c main/pet_bond.c main/pet_catalog.c main/pet_life.c main/pet_model.c \
        -o "${test_dir}/test_pet_bond_store"
    "${test_dir}/test_pet_bond_store"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Itests/service -Itests/lvgl/stubs \
        tests/test_pet_service.c main/pet_house.c main/pet_bond.c main/pet_catalog.c main/pet_life.c main/pet_model.c main/pet_protocol.c main/pet_meet.c \
        -o "${test_dir}/test_pet_service"
    "${test_dir}/test_pet_service"
    python3 tests/test_pet_companion.py
    python3 tests/test_pet_settlement.py
    python3 tests/test_ui_fonts.py
    python3 tests/test_digimon_sprites.py
    python3 tests/test_verify_firmware.py
    python3 tests/test_pet_install.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
