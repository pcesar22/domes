# GitHub Templates Reference

## Commit Message Templates

Feature:

```text
feat(<scope>): <description>

<Why this feature is needed>
```

Bug fix:

```text
fix(<scope>): <description>

<What was broken and why this fixes it>

Fixes #<issue-number>
```

Refactor:

```text
refactor(<scope>): <description>

<Why this refactor improves the code>
```

Documentation:

```text
docs(<scope>): <description>
```

## Pull Request Template

```markdown
## Executive summary
<!-- Plain-language change, reason, bottom-line result, and whether behavior changes. -->

## Why this matters
<!-- Customer, product, business, schedule, safety, reliability, or engineering-risk context. -->

## Status at a glance

| Area | Result | What it means |
| --- | --- | --- |
| Product or user behavior | Changed / Unchanged | Plain-language consequence |
| Automated software checks | Passed / Failed / Pending / Not applicable | Scope covered |
| Physical-device evidence | Passed / Failed / Provisional / Not tested / Not applicable | Claim boundary |
| Program or release status | Changed / Unchanged | Gate, schedule, or dependency consequence |

## What this PR changes
-

## What approval means

Approving this PR means:

> <!-- One plain-language decision sentence. -->

It does **not** mean:

-

## What happens next

1.

## Verification summary

- Automated software checks:
- Physical-device checks:
- Not tested or intentionally excluded:
- Independent review:

<details>
<summary>Technical evidence and reproducibility details</summary>

### Implementation details

-

### Verification details

-

### Tracking

- Issue:
- Related PRs:

</details>

## Plain-language glossary

- **Term:** Definition.
```

## Issue Templates

Bug report:

```markdown
## Bug Description
<!-- Clear description of the bug -->

## Steps to Reproduce
1.
2.
3.

## Expected Behavior
<!-- What should happen -->

## Actual Behavior
<!-- What actually happens -->

## Environment
- ESP-IDF version:
- Chip: ESP32-S3
- Hardware revision:

## Additional Context
<!-- Logs, screenshots, etc. -->
```

Feature request:

```markdown
## Feature Description
<!-- Clear description of the feature -->

## Use Case
<!-- Why this feature is needed -->

## Proposed Solution
<!-- How it might be implemented -->

## Alternatives Considered
<!-- Other approaches considered -->

## Additional Context
<!-- Mockups, examples, references -->
```

## Review Comment Templates

Suggestion:

````markdown
**Suggestion:** Consider using `X` here instead of `Y` because <reason>.

```cpp
// suggested code
```
````

Question:

```markdown
**Question:** Could you explain why <specific thing>?
```

Blocking issue:

````markdown
**Blocking:** This will cause <problem> because <reason>.

```cpp
// suggested fix
```
````

## gh CLI Templates

Create a feature PR:

```bash
gh pr create \
  --title "feat(<scope>): <description>" \
  --body "## Summary
<description>

## Changes
- <change 1>
- <change 2>

## Testing
- [ ] Builds for ESP32-S3
- [ ] Tested on hardware

## Notes
"
```

Create an issue:

```bash
gh issue create \
  --title "Bug: <description>" \
  --body "## Description
<description>

## Steps to Reproduce
1.
2.

## Expected vs Actual
Expected:
Actual:
"
```
