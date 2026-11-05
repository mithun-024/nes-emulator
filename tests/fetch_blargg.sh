#!/usr/bin/env sh
# Downloads blargg's NES hardware test ROMs (not checked into git) into
# tests/fixtures/nes-test-roms. The blargg test suite skips when absent.
set -e

DIR="$(cd "$(dirname "$0")" && pwd)/fixtures/nes-test-roms"

if [ -d "$DIR/.git" ]; then
    echo "Updating $DIR"
    git -C "$DIR" pull --ff-only
else
    echo "Cloning nes-test-roms into $DIR"
    git clone --depth 1 https://github.com/christopherpow/nes-test-roms "$DIR"
fi

echo "Done. Re-run the tests to include the blargg suite."
