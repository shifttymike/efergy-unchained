#!/bin/sh
set -eu
dest=${1:-/tmp/openssl-1.0.2u.tar.gz}
curl --fail --location --show-error \
  https://www.openssl.org/source/old/1.0.2/openssl-1.0.2u.tar.gz \
  -o "$dest"
printf '%s\n' \
  'ecd0c6ffb493dd06707d38b14bb4d8c2288bb7033735606569d8f90f89669d16  '"$dest" \
  | sha256sum -c -
