#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

#
# download-scripts.sh - downloads specific version of
#			codecov's bash script to generate and upload reports.
#			It's useful, since they may break our coverage.
#

set -e

# master: Merge pull request #342 from codecov/revert-proj-name-..., 18.08.2020
CODECOV_VERSION="e877c1280cc6e902101fb5df2981ed1c962da7f0"

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
curl https://keybase.io/codecovsecurity/pgp_keys.asc | gpg --no-default-keyring --keyring trustedkeys.gpg --import
curl -Os https://uploader.codecov.io/latest/linux/codecov
curl -Os https://uploader.codecov.io/latest/linux/codecov.SHA256SUM
curl -Os https://uploader.codecov.io/latest/linux/codecov.SHA256SUM.sig
gpgv codecov.SHA256SUM.sig codecov.SHA256SUM
sha256sum -c codecov.SHA256SUM
chmod +x codecov

mv -v codecov /opt/scripts/codecov

cd ..
rm -rf codecov-bash
