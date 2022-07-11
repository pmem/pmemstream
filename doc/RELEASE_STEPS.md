## pmemstream release steps

This document contains all the steps required to make a new release of pmemstream.

\#define $VERSION = current full version (e.g. 1.0.2); $VER = major+minor only version (e.g. 1.0)

Make a release locally:
- add an entry to ChangeLog, remember to change the day of the week in the release date
  - for major/minor releases mention compatibility with the previous release
  - for major/minor releases add new ChangeLog template (for $VER+1)
- update project's version in top-level CMakeLists.txt
- git commit -a -S -m "$VERSION release"
- git tag -a -s -m "Version $VERSION" $VERSION
- update project's version in top-level CMakeLists.txt: VERSION_PRERELEASE should be set to "devel"
- git commit -a -m "Update version to devel"
- for major/minor release:
  - create stable-$VER branch now: git checkout -b stable-$VER

Publish changes:
- for major/minor release:
  - git push upstream HEAD:master $VERSION
  - create and push to upstream stable-$VER branch
- for patch release:
  - git push upstream HEAD:stable-$VER $VERSION
  - create PR from stable-$VER to next stable (or master, if release is from most recent stable branch)

Publish package and make it official:
- go to [GitHub's releases tab](https://github.com/pmem/pmemstream/releases/new):
  - tag version: $VERSION, release title: pmemstream version $VERSION, description: copy entry from ChangeLog and format it with no tabs and no characters limit in line
- announce the release on pmem group and on pmem slack channel(s)

Later, for major/minor release:

- once 'main' branch of pmem.io repository contains new documentation:
  - add there new links to manpages
  - update there "Releases' support status" table (update any other release's status if needed)
