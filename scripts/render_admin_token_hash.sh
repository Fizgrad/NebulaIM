#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || -z "${1}" ]]; then
    echo "usage: $0 <raw-admin-token>" >&2
    exit 2
fi

token="$1"
hash="$(printf '%s' "${token}" | sha256sum | awk '{print $1}')"
echo "${hash}"
