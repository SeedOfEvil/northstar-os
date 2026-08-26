# Continuous integration workflows

`ci.yml` runs two required, non-release checks for pull requests and `main`:

- fast repository contracts on a GitHub-hosted Ubuntu runner; and
- the complete non-privileged `make test` gate, including the CMake build,
  shell contracts, Qt tests, and QML tests, in a disposable FreeBSD 15.1 amd64
  virtual machine.

Every remote action is pinned to a full commit SHA. The FreeBSD job installs
build dependencies as root inside the disposable guest, then builds and tests
Northstar as a dedicated unprivileged account.

These workflows must never receive package-signing keys or production
repository credentials. Package publication, image assembly, destructive
update/rollback checks, and physical hardware acceptance remain protected
release operations outside untrusted pull-request execution.
