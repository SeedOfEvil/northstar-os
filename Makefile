SHELL := /bin/sh

.DEFAULT_GOAL := help

.PHONY: help check-host bootstrap configure build test run-shell package vm-smoke image diagnostics

MANIFEST ?= packaging/manifests/bootstrap-packages.txt
NORTHSTAR_USER ?=
OUTPUT ?= /tmp/northstar-diagnostics

help:
	@printf '%s\n' 'Northstar development commands:'
	@printf '%s\n' '  make check-host   Validate the supported FreeBSD host'
	@printf '%s\n' '  make bootstrap    Install the pinned M0 development environment'
	@printf '%s\n' '  make configure    Configure the CMake build'
	@printf '%s\n' '  make build        Build project components'
	@printf '%s\n' '  make test         Run unit and integration tests'
	@printf '%s\n' '  make run-shell    Start the Northstar shell session'
	@printf '%s\n' '  make package      Build Northstar packages'
	@printf '%s\n' '  make vm-smoke     Run the M0 native smoke preflight'
	@printf '%s\n' '  make image        Assemble a release image'
	@printf '%s\n' '  make diagnostics  Collect non-secret diagnostics'
	@printf '%s\n' ''
	@printf '%s\n' 'M0 commands are functional; later milestone targets remain guarded.'

check-host:
	@sh tools/check-host.sh

bootstrap:
	@if [ -z "$(NORTHSTAR_USER)" ]; then \
		printf '%s\n' 'ERROR: pass NORTHSTAR_USER=<development-user>' >&2; \
		exit 2; \
	fi
	@sh tools/bootstrap-dev.sh --user "$(NORTHSTAR_USER)" --manifest "$(MANIFEST)"

test:
	@sh tests/unit/test-m0-scripts.sh

vm-smoke:
	@sh tests/vm/m0-smoke.sh

diagnostics:
	@sh tools/collect-diagnostics.sh --output "$(OUTPUT)"

configure build run-shell package image:
	@printf '%s\n' "Northstar: '$@' is planned for a later milestone." >&2
	@exit 2
