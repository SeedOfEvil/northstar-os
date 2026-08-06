SHELL := /bin/sh

.DEFAULT_GOAL := help

.PHONY: help check-host bootstrap configure build test run-shell package vm-smoke image diagnostics

help:
	@printf '%s\n' 'Northstar development commands:'
	@printf '%s\n' '  make check-host   Validate the supported FreeBSD host'
	@printf '%s\n' '  make bootstrap    Install the pinned M0 development environment'
	@printf '%s\n' '  make configure    Configure the CMake build'
	@printf '%s\n' '  make build        Build project components'
	@printf '%s\n' '  make test         Run unit and integration tests'
	@printf '%s\n' '  make run-shell    Start the Northstar shell session'
	@printf '%s\n' '  make package      Build Northstar packages'
	@printf '%s\n' '  make vm-smoke     Run the disposable VM smoke test'
	printf '%s\n' '  make image        Assemble a release image'
	@printf '%s\n' '  make diagnostics  Collect non-secret diagnostics'
	@printf '%s\n' ''
	@printf '%s\n' 'The implementation targets are introduced milestone by milestone.'

check-host bootstrap configure build test run-shell package vm-smoke image diagnostics:
	@printf '%s\n' "Northstar: '$@' is planned but not implemented in PR 1." >&2
	@exit 2
