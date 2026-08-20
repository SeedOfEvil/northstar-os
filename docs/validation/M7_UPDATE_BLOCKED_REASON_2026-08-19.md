# Saying why updates are blocked — 2026-08-19

PR #113, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2, Qt 6.11.1) at
commit `7688aad`, built in `/home/northstar/builds/pr113-7688aad` with the
project's canonical `-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`.

## What the Software Center was saying

Six lines, ending in:

```
Catalogue digest verified, but publication signature is not verified:
public-key fingerprint is not trusted for this repository
Update planning is blocked: the publication signature is not verified.
```

Every word true. Nothing in it says what is wrong or what would fix it.

## What is actually wrong

| | |
| --- | --- |
| Trust material in `/usr/local/etc/northstar/` | describes revision **78**, installed 10 August |
| Repository the active pkg configuration names | publishes revision **86**, generated 15 August |

A signature over revision 86 cannot match metadata describing revision 78, so
verification fails and planning stops. That is correct behaviour. The problem
was that the surface could not describe it, because `UpdatePlanController` only
ever read the installed trust material and never looked at what the repository
claims to be.

This is the same shape as the deployment manifest debt PR #107 dealt with:
material installed once, and never refreshed when the thing it describes moved
on.

## What this changes

The controller now reads the repository's own `publication-record.conf`,
through the local path the active pkg configuration names, and reports the two
revisions that disagree:

```
active repository path: /home/northstar/validation/development-channel-r86
trusted revision  : 78
published revision: 86
signature verified: false
blocked reason    : This system trusts revision 78, but the repository
                    publishes revision 86. Updates stay blocked until the
                    trust material for revision 86 is installed.
```

That is a read-only probe against the real machine, not a fixture. The Software
Center leads with that sentence; the check-by-check detail stays below it for
anyone who wants it.

Only a **local** repository is read. A repository reached over the network
cannot have its revision established without fetching, and this surface fetches
nothing.

## What is deliberately unchanged

**No trust decision is relaxed.** A revision mismatch blocks planning exactly
as it did before, through exactly the same checks. The only difference is that
the refusal is described in terms of what is out of step.

Nothing here installs trust material, refreshes a fingerprint store, or applies
an update. Doing so touches signed-repository material, which the project's
rules govern separately, and it is a decision to be taken deliberately rather
than as a side effect of making an error message readable.

## Automated evidence

- Clean build, 0 errors.
- `env QT_QPA_PLATFORM=offscreen ctest --test-dir /home/northstar/builds/pr113-7688aad --output-on-failure`
  — **37/37 suites passed, 0 failed**.
- Two new cases in `northstar-updateplancontroller`: the local repository path
  is read from a written pkg configuration, and a repository reached over the
  network, a configuration for a different repository, and an empty directory
  each yield nothing rather than a guess.
- The probe output above, taken from the machine's real configuration.
- `northstar-shell --qml-self-test` on the installed binary — exit 0, no QML
  warnings.

### A raw string that ended early

The first build failed with `expected ')'`. The pattern matching a repository
URL contains `)"`, which is the closing delimiter of a default raw string, so
the literal ended in the middle of the regular expression. It now uses a
delimiter the pattern does not contain.

## Not claimed

The update path is still blocked, and this change does not unblock it. What it
does is make the blockage legible, which is the difference between a surface
that reports a state and one that only reports a symptom.

Whether the trust material *should* be refreshed to revision 86 is an open
question this does not answer. Revision 86 was generated on 15 August from
source revision `448b297`, which is not reachable from any current ref, and the
packaging lane has not moved since. Refreshing trust to a publication built
from an unreachable commit deserves a deliberate decision.

The new panel text is not covered by the QML surface tests from PR #109.

## Interactive acceptance

Status: **open**.
