# Check if we need to prepend docker commands with sudo
SUDO := $(shell docker version >/dev/null 2>&1 || echo "sudo")

# https://github.com/opencontainers/image-spec/blob/master/annotations.md
BUILD_DATE := $(shell date -u +%FT%TZ)

# Base image, defaults to ubuntu 20.04 focal
DISTRO ?= focal

# Deb package version to use. For example "17.0.0-557.096f6a7.focal"
# Not needed for testing builds
GRAVITY_VERSION ?=

# Set default tag
ifndef TAG
	TAG = ${USER}/gravity:$(shell git describe --always --tags --long)
endif

docker-build:
ifndef GRAVITY_VERSION
	$(error GRAVITY_VERSION environment variable must be set. For example GRAVITY_VERSION=17.0.0-557.096f6a7.focal )
endif
ifndef TAG
	TAG = stellar/gravity:$(GRAVITY_VERSION)
endif
	$(SUDO)  docker build --pull \
	--label org.opencontainers.image.created="$(BUILD_DATE)" \
	--build-arg GRAVITY_VERSION=${GRAVITY_VERSION} \
	--build-arg DISTRO=${DISTRO} \
	-t ${TAG} .

docker-build-testing:
	cd ../ && $(SUDO) docker build --pull -f docker/Dockerfile.testing -t ${TAG} .
