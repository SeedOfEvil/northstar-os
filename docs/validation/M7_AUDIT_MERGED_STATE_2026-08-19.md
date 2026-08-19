# The deployment state between handoffs — 2026-08-19

PR #107, verified on NSTAR-DEV01 (FreeBSD 15.1-RELEASE-p2) at commit
`a796728`. This change touches only `tools/` and `tests/unit/`, so it produces
no new binary and nothing was rebuilt or reinstalled.

## The failure

Immediately after PR #106 merged, with the deployment manifest freshly
updated and correct, the audit still failed:

```
FAIL: source branch is not a codex/* validation branch
FAIL: ui lane deployment audit found 1 failure(s)
```

Nothing was wrong with the deployment. The auditor required `source_branch` to
match `codex/*`, which is true only while a handoff is in flight. The moment
that branch is squash-merged it is deleted, and the machine correctly returns
to `main` — and from then until the next handoff begins, the check could not
pass no matter what state the machine was in.

This is the same fault PR #102 fixed for the lane expectation: a gate that a
legitimate state cannot satisfy. Such a gate does not make anything safer. It
trains everyone reading the output to skip a failure line, which is exactly
the habit that lets a real failure through.

## The fix, and why it is not a rubber stamp

`main` is now accepted alongside `codex/*`. On its own that would make the
check meaningless, because "main" could then describe any working state at
all, including uncommitted experiments nobody reviewed.

So resting on `main` carries an extra requirement the in-flight state does not
have: the deployed revision must be reachable from `origin/main`.

```sh
if [ "$source_branch" = main ]; then
    if ! git -C "$canonical_checkout" rev-parse --verify -q origin/main >/dev/null 2>&1; then
        fail "deployment rests on main but origin/main is unknown to the checkout"
    elif git -C "$canonical_checkout" merge-base --is-ancestor "$source_revision" origin/main 2>/dev/null; then
        note "deployment rests on merged main between handoffs"
    else
        fail "deployment rests on main but $source_revision is not merged into origin/main"
    fi
fi
```

An absent `origin/main` is a failure rather than a pass, because the check
cannot be performed and claiming it was would be worse than saying so. A
branch that is neither `codex/*` nor `main` is still refused outright.

## Proving the test catches the defect

A test that passes against both the old and new tool proves nothing. The new
cases were run against the pre-fix auditor from `054c850`, restored from git
into the working tree:

| Auditor | Result |
| --- | --- |
| Pre-fix (`054c850`) | **FAIL** — `audit rejected a deployment resting on merged main` |
| Fixed (`a796728`) | **PASS** — all three summary lines |

The working tree was restored afterwards and confirmed clean.

Three cases were added, and the second is the one that matters:

1. A merged revision on `main` passes and reports the state it accepted.
2. An **unmerged** revision described as `main` fails, naming that reason. The
   commit is real and the checkout is clean; only reachability from
   `origin/main` separates it from case 1.
3. A branch outside the convention is refused outright.

## Automated evidence

- `sh tests/unit/test-validation-deployment-audit.sh` on FreeBSD — exit 0,
  three PASS lines.
- The pre-fix control above.
- `sh -n` on both changed scripts — exit 0.
- The real deployment audit on DEV01, resting on `main` at `054c850`:

```
NOTE: deployment rests on merged main between handoffs
NOTE: deployed source is ahead of the signed publication, expected in the ui lane
WARN: historical build remains outside retention boundaries: ...pr103-dd79fca
WARN: historical build remains outside retention boundaries: ...pr104-59a05ee
WARN: historical build remains outside retention boundaries: ...pr106-52dccf9
exit: 0
```

**0 failures.** The three warnings are accumulated build trees from merged and
accepted work; the auditor exempts only the canonical build, so they are a
retention decision rather than a defect, and are left for Hector to make.

## Not claimed

The C++ suite was not re-run for this change. No C++ source was touched, and
the build tree on the machine was produced from the previous revision, so
running it would have measured the old code and reported it as evidence for
this one.

There is no interactive checklist. The change alters what a command-line audit
prints and nothing a user of the desktop can see, so there is nothing for the
walkthrough to exercise. Acceptance rests on the automated evidence above.

## Interactive acceptance

Accepted by Hector on 2026-08-19 on the automated evidence above, there being
no user-visible behaviour to walk through.

Status: **accepted**.
