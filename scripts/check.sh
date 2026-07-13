#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd -P)"
BUILD_DIR="${BILLIARDGL_BUILD_DIR:-${REPO_ROOT}/build/check}"

python3 "${REPO_ROOT}/tests/check_text_encoding_tests.py" -v
python3 "${REPO_ROOT}/scripts/check_text_encoding.py" --root "${REPO_ROOT}"

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --parallel
ctest --test-dir "${BUILD_DIR}" --output-on-failure --label-exclude rendered
