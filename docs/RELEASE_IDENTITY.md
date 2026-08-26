# Northstar release identity

Northstar's product and package version has one source: the repository-root
`VERSION` file. It contains exactly one semantic version in `X.Y.Z` form.

The root CMake project reads that value and supplies it to CPack, every
first-party executable, and generated application-bundle manifests. The
Makefile also derives the default package artifact name from it. Do not add a
second application, package, or image version constant.

Repository publication requires the same `VERSION` file and rejects a
Northstar package whose metadata version or origin disagrees. Its signed
publication record carries both `northstar_version` and `source_revision`.

Image lock files are immutable release records. `--check-lock` can validate a
historical lock independently, but staging an image for the current checkout
also requires all of the following:

- `NORTHSTAR_PACKAGE_VERSION` equals `VERSION`;
- `NORTHSTAR_PACKAGE` is `northstar-<VERSION>-amd64.pkg`;
- `NORTHSTAR_SOURCE_REVISION` equals the selected clean Git commit.

Changing `VERSION` therefore starts a release transaction: build the matching
package, publish repository metadata from that exact commit, and create a new
immutable image lock. Never edit an accepted historical lock to describe a
different artifact.

Run `sh tools/ci/version-contracts.sh` for the fast release-identity gate. It is
also part of the protected pull-request repository checks.
