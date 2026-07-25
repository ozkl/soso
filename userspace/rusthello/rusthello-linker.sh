#!/bin/sh
set -eu

output=
previous=
for argument in "$@"; do
    if [ "$previous" = "-o" ]; then
        output=$argument
        break
    fi
    previous=$argument
done

"${CC:-cc}" "$@"

# Cargo links the hashed executable first and then promotes it to target/{debug,release}.
# Copy that executable into the initrd input directory after a successful link.
case "${output##*/}" in
    rusthello-*)
        project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
        mkdir -p "$project_dir/../bin"
        cp "$output" "$project_dir/../bin/rusthello"
        ;;
esac
