---
name: domes-github-workflow
description: DOMES git and GitHub workflow standards for branches, worktrees, commits, pull requests, reviews, issues, CI labels, and release hygiene. Use when preparing commits or PRs, reviewing diffs, naming branches, triaging GitHub issues, responding to review feedback, or deciding how to publish Codex changes for the DOMES repo.
---

# DOMES GitHub Workflow

Use this skill for DOMES-specific git and GitHub conventions. Prefer the Codex GitHub plugin/app
when available for PR, review, and issue operations; use `gh` as a fallback when connector coverage
is insufficient.

## Branches And Worktrees

Regular branch format:

```text
<type>/<short-description>
```

Codex-created worktree or branch format:

```text
codex/<type>/<short-description>
```

Types:

- `feat`: new functionality.
- `fix`: bug fixes.
- `refactor`: restructuring without behavior change.
- `docs`: documentation only.
- `test`: tests.
- `chore`: build, CI, tooling, dependency changes.

For substantial work from `main`, prefer a worktree:

```bash
mkdir -p .worktrees
git worktree add .worktrees/<name> -b codex/<type>/<short-description>
```

Do not create or publish a PR without asking the user first, unless the user issued the exact
autonomous-continuation directive defined in root `AGENTS.md`. That directive authorizes one focused
execution issue and one selected-package PR, but never merge, release, or unrelated GitHub state.
It permits `hw-test` only when required and the milestone-manager registered-board preflight passes.

## Commit Messages

Use conventional commits:

```text
<type>(<scope>): <subject>

<body>

<footer>
```

Subject rules:

- Imperative mood: `add`, not `added` or `adds`.
- Lowercase first word unless it is a proper noun.
- No period.
- Keep near 50 characters when practical.

Body rules:

- Explain what and why, not a blow-by-blow of how.
- Wrap around 72 characters when practical.
- Include issue references such as `Fixes #42` where appropriate.

Do not add AI co-author trailers unless the user explicitly asks.

## Before Committing

```bash
git status --short
git diff
git diff --staged
git diff --staged | rg -i "password|secret|key|token"
```

Verify affected components before committing. For firmware changes, use the verification ladder in
root `AGENTS.md`.

## Pull Requests

Title format should match the main commit subject:

```text
<type>(<scope>): <description>
```

Use the CEO-first structure in `.github/pull_request_template.md` and the filled reference in
`references/templates.md`. Lead with plain-language purpose, outcome, and decision context. Put
internal package codes, hashes, commands, protocol details, and long test evidence in the collapsed
technical appendix. Define unavoidable acronyms and specialized terms.

Every description must distinguish:

- what the PR changes and why the company should care;
- passed, failed, provisional, pending, and not-run evidence;
- what approval authorizes and explicitly does not authorize; and
- remaining work, ownership, and the next decision or dependency.

Keep PRs focused. Self-review before requesting review. Link issues with `Fixes #123` or
`Relates to #123`. Avoid force-pushing after review unless the user or maintainer requests it.

## Reviews

In review mode, lead with findings ordered by severity. Check:

- Behavior and edge cases.
- Race conditions and FreeRTOS interactions.
- Heap allocation after init.
- ISR safety.
- Thread safety and const-correctness.
- Protocol compatibility and generated protobuf use.
- Missing tests or hardware verification.

Use precise file and line references.

## CI And Hardware Testing

Every PR should pass relevant build, unit, CLI, and firmware checks. The `hw-test` label triggers
self-hosted erase, factory-programming, OTA/recovery, and forced-rollback validation. Ask before
adding it unless root `AGENTS.md` makes the exact registered-board workflow part of an authorized
autonomous-continuation cycle.

## Templates

Use `references/templates.md` for commit, PR, issue, and review-comment templates.
