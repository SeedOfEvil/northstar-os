# Continuous integration workflow placeholder

The workflow directory is scaffolded in PR 1. The CI implementation is PR 3 work and will split checks between GitHub-hosted documentation/tooling jobs, native FreeBSD Cirrus jobs, and protected release builders.

Do not add a persistent privileged runner here. Public pull-request code must not be able to reach package-signing keys, image builders, or other release infrastructure.
