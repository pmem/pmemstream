#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

#
# set-ci-vars.sh -- set CI variables common for both:
#                   Travis and GitHub Actions CIs
#

set -e

function get_last_merge {
	LAST_MERGE=$(git log --merges --pretty=%H -1)
	if [ -n "${GITHUB_ACTIONS}" ] && [ "${GITHUB_EVENT_NAME}" == "pull_request" ] && [ "${LAST_MERGE}" == "${LAST_COMMIT}" ]; then
		# GitHub Actions commits its own merge in case of pull requests
		# so the first merge commit has to be skipped.

		LAST_MERGE=$(git log --merges --pretty=%H -2 | tail -n1)
		# If still the last commit is a merge commit it means we're manually
		# merging changes (probably back from stable branch). We have to use
		# left parent of the merge and the current commit for COMMIT_RANGE.
	fi
	if [ "${LAST_MERGE}" == "" ]; then
		# possible in case of shallow clones
		# or new repos with no merge commits yet
		# - pick up the first commit
		LAST_MERGE=$(git log --pretty=%H | tail -n1)
	fi
	echo ${LAST_MERGE}
}

function get_last_commit {
	git log --no-merges --pretty=%H -1
}

function get_commit_range_from_last_merge {
	LAST_MERGE=$(get_last_merge)
	LAST_COMMIT=$(get_last_commit)
	# get commit id of the last merge
	COMMIT_RANGE="${LAST_MERGE}..${LAST_COMMIT}"
	# make sure it works now
	if ! git rev-list ${COMMIT_RANGE} >/dev/null; then
		COMMIT_RANGE=""
	fi
	echo ${COMMIT_RANGE}
}

COMMIT_RANGE_FROM_LAST_MERGE=$(get_commit_range_from_last_merge)
LAST_MERGE=$(get_last_merge)

if [ -n "${TRAVIS}" ]; then
	CI_COMMIT=${TRAVIS_COMMIT}
	CI_COMMIT_RANGE="${TRAVIS_COMMIT_RANGE/.../..}"
	CI_BRANCH=${TRAVIS_BRANCH}
	CI_EVENT_TYPE=${TRAVIS_EVENT_TYPE}
	CI_REPO_SLUG=${TRAVIS_REPO_SLUG}

	# CI_COMMIT_RANGE is usually invalid for force pushes - fix it when used
	# with non-upstream repository
	if [ -n "${CI_COMMIT_RANGE}" -a "${CI_REPO_SLUG}" != "${GITHUB_REPO}" ]; then
		if ! git rev-list ${CI_COMMIT_RANGE}; then
			CI_COMMIT_RANGE=${COMMIT_RANGE_FROM_LAST_MERGE}
		fi
	fi

	case "${TRAVIS_CPU_ARCH}" in
	"amd64")
		CI_CPU_ARCH="x86_64"
		;;
	*)
		CI_CPU_ARCH=${TRAVIS_CPU_ARCH}
		;;
	esac

elif [ -n "${GITHUB_ACTIONS}" ]; then
	CI_COMMIT=${GITHUB_SHA}
	CI_COMMIT_RANGE=${COMMIT_RANGE_FROM_LAST_MERGE}
	CI_BRANCH=$(echo ${GITHUB_REF} | cut -d'/' -f3)
	CI_REPO_SLUG=${GITHUB_REPOSITORY}
	CI_CPU_ARCH="x86_64" # GitHub Actions supports only x86_64

	case "${GITHUB_EVENT_NAME}" in
	"schedule")
		CI_EVENT_TYPE="cron"
		;;
	*)
		CI_EVENT_TYPE=${GITHUB_EVENT_NAME}
		;;
	esac

else
	CI_COMMIT=$(git log --pretty=%H -1)
	CI_COMMIT_RANGE=${COMMIT_RANGE_FROM_LAST_MERGE}
	CI_CPU_ARCH="x86_64"
fi

export CI_COMMIT=${CI_COMMIT}
export CI_COMMIT_RANGE=${CI_COMMIT_RANGE}
export CI_LAST_MERGE=${LAST_MERGE}
export CI_BRANCH=${CI_BRANCH}
export CI_EVENT_TYPE=${CI_EVENT_TYPE}
export CI_REPO_SLUG=${CI_REPO_SLUG}
export CI_CPU_ARCH=${CI_CPU_ARCH}

echo CI_COMMIT=${CI_COMMIT}
echo CI_COMMIT_RANGE=${CI_COMMIT_RANGE}
echo CI_LAST_MERGE=${CI_LAST_MERGE}
echo CI_BRANCH=${CI_BRANCH}
echo CI_EVENT_TYPE=${CI_EVENT_TYPE}
echo CI_REPO_SLUG=${CI_REPO_SLUG}
echo CI_CPU_ARCH=${CI_CPU_ARCH}
