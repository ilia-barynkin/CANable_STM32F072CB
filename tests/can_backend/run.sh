#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$test_dir/../.." && pwd)"
test_binary="$(mktemp)"
trap 'rm -- "$test_binary"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DCANABLE_GS_USB \
    -I"$test_dir/stubs" -I"$repo_dir/Core/Inc" \
    "$repo_dir/Core/Src/gs_can.c" "$test_dir/test_gs_can.c" \
    -o "$test_binary"
"$test_binary"
