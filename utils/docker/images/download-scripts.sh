#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

#
# download-scripts.sh - downloads latest version of
#			codecov's uploader to generate and upload reports.
#			It's useful, since they may break our coverage.
#

set -e

if [ "${SKIP_SCRIPTS_DOWNLOAD}" ]; then
	echo "Variable 'SKIP_SCRIPTS_DOWNLOAD' is set; skipping scripts' download"
	exit
fi

mkdir -p /opt/scripts

if ! [ -x "$(command -v curl)" ]; then
	echo "Error: curl is not installed."
	return 1
fi

# Download codecov and check integrity
mkdir -p codecov-tmp
cd codecov-tmp

curl https://keybase.io/codecovsecurity/pgp_keys.asc | gpg --no-default-keyring --keyring trustedkeys.gpg --import
curl -Os https://uploader.codecov.io/latest/linux/codecov
curl -Os https://uploader.codecov.io/latest/linux/codecov.SHA256SUM
curl -Os https://uploader.codecov.io/latest/linux/codecov.SHA256SUM.sig
gpgv codecov.SHA256SUM.sig codecov.SHA256SUM
sha256sum -c codecov.SHA256SUM
chmod +x codecov

mv -v codecov /opt/scripts/codecov

cd ..
rm -rf codecov-tmp
