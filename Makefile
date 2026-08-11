SHELL := /bin/sh

.DEFAULT_GOAL := help

.PHONY: help check-host bootstrap configure build test welcome-app-test qml-surface-test image-input-test runtime-bundle-test nested-wayfire-package-test image-assembler-test image-boot-smoke-test image-update-rollback-gate-test capture-runtime-bundle prepare-image-inputs validation-deployment-audit update-helper-test update-broker-smoke transactional-update-smoke run-shell shell-smoke shell-restart-smoke install-user install-console-autostart disable-console-autostart install-sddm-fallback disable-sddm-fallback package pkg-repository-smoke signed-development-repository-smoke vm-smoke nested-wayfire nested-wayfire-session nested-wayfire-session-supervised image diagnostics

MANIFEST ?= packaging/manifests/bootstrap-packages.txt
NORTHSTAR_USER ?=
NORTHSTAR_WAYFIRE_BIN ?=
OUTPUT ?= /tmp/northstar-diagnostics
BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Debug
NORTHSTAR_PREFIX ?= $(HOME)/.local
NORTHSTAR_SHELL_BIN ?= $(NORTHSTAR_PREFIX)/bin/northstar-shell
NORTHSTAR_PKG_CLIENT ?= 0
NORTHSTAR_UPDATE_BROKER_BIN ?= $(BUILD_DIR)/src/update/northstar-update-broker
VALIDATION_DEPLOYMENT_MANIFEST ?= /usr/local/etc/northstar/validation-deployment.conf
IMAGE_LOCK ?= image/manifests/northstar-15.1-amd64-qcow2.lock
IMAGE_ARTIFACTS ?= .artifacts/m5-inputs
IMAGE_INPUT_OUTPUT ?= .artifacts/m5-resolved-inputs
IMAGE_RUNTIME_ROOTS ?= image/manifests/northstar-runtime-roots.txt
IMAGE_RUNTIME_OUTPUT ?= .artifacts/m5-runtime-bundle
IMAGE_PACKAGE_CACHE ?= .artifacts/m5-package-cache
NORTHSTAR_IMAGE_PACKAGE ?= .artifacts/m5-inputs/northstar-0.1.4-amd64.pkg
NORTHSTAR_COMPAT_PACKAGE ?= .artifacts/m5-compat-package/northstar-wayfire-nested-0.10.1.746bc7e.pkg
IMAGE_SOURCE_DATE_EPOCH ?= $(shell sed -n 's/^SOURCE_DATE_EPOCH=//p' "$(IMAGE_LOCK)")
PROJECT_COMMIT ?= $(shell git rev-parse HEAD 2>/dev/null)
PROJECT_ROOT ?= .

help:
	@printf '%s\n' 'Northstar development commands:'
	@printf '%s\n' '  make check-host   Validate the supported FreeBSD host'
	@printf '%s\n' '  make bootstrap    Install the pinned M0 development environment'
	@printf '%s\n' '  make configure    Configure the CMake build'
	@printf '%s\n' '  make build        Build project components'
	@printf '%s\n' '  make test         Run unit and integration tests'
	@printf '%s\n' '  make welcome-app-test  Test the bundled Northstar Welcome launcher'
	@printf '%s\n' '  make qml-surface-test  Check product-critical QML surface wiring'
	@printf '%s\n' '  make image-input-test  Test pinned M5 image-input preparation'
	@printf '%s\n' '  make runtime-bundle-test  Test exact offline runtime capture'
	@printf '%s\n' '  make nested-wayfire-package-test  Test scfb compatibility packaging'
	@printf '%s\n' '  make image-assembler-test  Test privileged QCOW2 input preflight'
	@printf '%s\n' '  make image-boot-smoke-test  Test snapshot-only QCOW2 boot-smoke contract'
	@printf '%s\n' '  make image-update-rollback-gate-test  Test the reboot-spanning image update gate'
	@printf '%s\n' '  make capture-runtime-bundle  Capture accepted installed runtime packages'
	@printf '%s\n' '  make prepare-image-inputs  Verify staged M5 artifacts and record provenance'
	@printf '%s\n' '  make validation-deployment-audit  Audit the canonical VM deployment (read-only)'
	@printf '%s\n' '  make update-helper-test  Test the bounded update-helper request contract'
	@printf '%s\n' '  make update-broker-smoke  Verify and stage a disposable update request (root)'
	@printf '%s\n' '  make transactional-update-smoke  Prove update/rollback ordering with isolated tools (root)'
	@printf '%s\n' '  make run-shell    Start the Northstar shell session'
	@printf '%s\n' '  make shell-smoke  Check the live shell session'
	@printf '%s\n' '  make shell-restart-smoke  Restart only the live shell and verify clients survive'
	@printf '%s\n' '  make install-user Install the built session and shell below NORTHSTAR_PREFIX'
	@printf '%s\n' '  make install-console-autostart Enable desktop start after local console login'
	@printf '%s\n' '  make disable-console-autostart Disable the local console-login desktop start'
	@printf '%s\n' '  make install-sddm-fallback Enable the branded SDDM Proxmox fallback policy (root)'
	@printf '%s\n' '  make disable-sddm-fallback Disable the branded SDDM Proxmox fallback policy (root)'
	@printf '%s\n' '  make package      Build Northstar packages'
	@printf '%s\n' '  make pkg-repository-smoke  Validate a disposable signed pkg repository'
	@printf '%s\n' '  make signed-development-repository-smoke  Validate Northstar signed channel (root)'
	@printf '%s\n' '  make vm-smoke     Run the M0 native smoke preflight'
	@printf '%s\n' '  make nested-wayfire Build the supplemental X11/pixman Wayfire lane'
	@printf '%s\n' '  make nested-wayfire-session Build and install the supplemental user session'
	@printf '%s\n' '  make nested-wayfire-session-supervised Opt in to the northstar-session nested lane'
	@printf '%s\n' '  make image        Assemble a release image'
	@printf '%s\n' '  make diagnostics  Collect non-secret diagnostics'
	@printf '%s\n' ''
	@printf '%s\n' 'M0 and the first M1 shell slice are functional; later targets remain guarded.'

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
	@$(MAKE) welcome-app-test
	@$(MAKE) qml-surface-test
	@sh tests/unit/test-nested-wayfire-session.sh
	@sh tests/unit/test-console-autostart.sh
	@sh tests/unit/test-sddm-theme.sh
	@sh tests/unit/test-sddm-fallback.sh
	@sh tests/unit/test-session-script.sh
	@sh tests/unit/test-update-helper.sh
	@sh tests/unit/test-validation-deployment-audit.sh
	@sh tests/unit/test-image-inputs.sh
	@sh tests/unit/test-runtime-bundle.sh
	@sh tests/unit/test-nested-wayfire-package.sh
	@sh tests/unit/test-image-assembler.sh
	@sh tests/unit/test-image-update-rollback-gate.sh
	@$(MAKE) build
	@sh tests/unit/test-session-entrypoint.sh "$(BUILD_DIR)"
	@ctest --test-dir "$(BUILD_DIR)" --output-on-failure

welcome-app-test:
	@sh tests/unit/test-welcome-app.sh

qml-surface-test:
	@sh tests/unit/test-qml-surfaces.sh

image-input-test:
	@sh tests/unit/test-image-inputs.sh

runtime-bundle-test:
	@sh tests/unit/test-runtime-bundle.sh

nested-wayfire-package-test:
	@sh tests/unit/test-nested-wayfire-package.sh

image-assembler-test:
	@sh tests/unit/test-image-assembler.sh

image-boot-smoke-test:
	@sh tests/unit/test-image-boot-smoke.sh

image-update-rollback-gate-test:
	@sh tests/unit/test-image-update-rollback-gate.sh

capture-runtime-bundle:
	@sh image/scripts/capture-runtime-bundle.sh \
		--roots "$(IMAGE_RUNTIME_ROOTS)" \
		--northstar-package "$(NORTHSTAR_IMAGE_PACKAGE)" \
		--compat-package "$(NORTHSTAR_COMPAT_PACKAGE)" \
		--package-cache "$(IMAGE_PACKAGE_CACHE)" \
		--source-date-epoch "$(IMAGE_SOURCE_DATE_EPOCH)" \
		--output "$(IMAGE_RUNTIME_OUTPUT)"

prepare-image-inputs:
	@sh image/scripts/prepare-image-inputs.sh \
		--lock "$(IMAGE_LOCK)" \
		--artifacts "$(IMAGE_ARTIFACTS)" \
		--output "$(IMAGE_INPUT_OUTPUT)" \
		--project-root "$(PROJECT_ROOT)" \
		--project-commit "$(PROJECT_COMMIT)"

validation-deployment-audit:
	@sh tools/audit-validation-deployment.sh --manifest "$(VALIDATION_DEPLOYMENT_MANIFEST)" --strict

update-helper-test:
	@sh tests/unit/test-update-helper.sh

update-broker-smoke:
	@NORTHSTAR_UPDATE_BROKER_BIN="$(NORTHSTAR_UPDATE_BROKER_BIN)" sh tests/vm/update-broker-smoke.sh

transactional-update-smoke:
	@sh tests/vm/transactional-update-smoke.sh

vm-smoke:
	@if [ -n "$(NORTHSTAR_WAYFIRE_BIN)" ]; then \
		NORTHSTAR_WAYFIRE_BIN="$(NORTHSTAR_WAYFIRE_BIN)" sh tests/vm/m0-smoke.sh; \
	else \
		sh tests/vm/m0-smoke.sh; \
	fi

pkg-repository-smoke:
	@if [ "$(NORTHSTAR_PKG_CLIENT)" = 1 ]; then \
		sh tests/vm/pkg-repository-smoke.sh --client; \
	else \
		sh tests/vm/pkg-repository-smoke.sh; \
	fi

package: build
	@cd "$(BUILD_DIR)" && cpack -G FREEBSD

signed-development-repository-smoke:
	@NORTHSTAR_SOURCE_REVISION="$(NORTHSTAR_SOURCE_REVISION)" \
		sh tests/vm/signed-development-repository-smoke.sh

nested-wayfire:
	@sh tools/build-nested-wayfire.sh

nested-wayfire-session: nested-wayfire
	@sh tools/install-nested-wayfire-session.sh

nested-wayfire-session-supervised: nested-wayfire install-user
	@sh tools/install-nested-wayfire-session.sh --supervised

diagnostics:
	@sh tools/collect-diagnostics.sh --output "$(OUTPUT)"

configure:
	@cmake -S . -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)" -DBUILD_TESTING=ON

build: configure
	@cmake --build "$(BUILD_DIR)" --parallel 2

run-shell: build
	@exec "$(BUILD_DIR)/src/shell/northstar-shell"

install-user: build
	@cmake --install "$(BUILD_DIR)" --prefix "$(NORTHSTAR_PREFIX)"

install-console-autostart:
	@sh tools/install-console-autostart.sh --enable

disable-console-autostart:
	@sh tools/install-console-autostart.sh --disable

install-sddm-fallback:
	@sh tools/install-sddm-fallback.sh --enable

disable-sddm-fallback:
	@sh tools/install-sddm-fallback.sh --disable

shell-smoke: build
	@NORTHSTAR_SHELL_BIN="$(BUILD_DIR)/src/shell/northstar-shell" sh tests/integration/test-shell-session.sh

shell-restart-smoke: install-user
	@NORTHSTAR_SHELL_BIN="$(NORTHSTAR_SHELL_BIN)" sh tests/integration/test-shell-session.sh --restart

image:
	@printf '%s\n' 'Northstar: run the guarded PR76 assembler on a marked disposable FreeBSD builder.' >&2
	@printf '%s\n' 'See docs/M5_QCOW2_BUILDER.md; direct make image remains intentionally non-privileged.' >&2
	@exit 2
