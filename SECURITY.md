# Security policy

Northstar is pre-alpha and does not yet publish supported binary releases. Security fixes and release support windows will be defined before the first public image.

## Reporting a vulnerability

Please report suspected vulnerabilities privately through the repository's GitHub Security Advisories page once the public repository is connected to its organization. If that page is not available, contact the project maintainers through the private channel listed by the organization rather than opening a public issue.

Include:

- affected commit, package, image, or configuration;
- exact reproduction steps and required privileges;
- impact and likely attack path;
- logs or proof of concept with secrets removed;
- a safe contact method for follow-up.

Allow maintainers reasonable time to investigate and publish a fix or mitigation. Coordinated disclosure timing will be agreed with the reporter.

## Security boundaries for the first milestones

- The shell and desktop services run as an unprivileged user.
- Reboot and shutdown require controlled authorization.
- Session logs must not contain secrets.
- Release signing keys never live in the public repository or pull-request runner.
- Privileged image and package builds run only on protected, disposable infrastructure.
- Public fork pull requests must never reach a persistent privileged builder.

See [`docs/QUALITY_GATES.md`](docs/QUALITY_GATES.md) for the security and release gates that turn these principles into testable requirements.
