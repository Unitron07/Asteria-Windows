#!/bin/sh
# Generate deterministic Asteria assets; preserve the approved black background.
set -eu
exec python3 "$(dirname "$0")/generate-asteria-icons.py"
