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
## Summary
<!-- 1-3 sentence description of what this PR does -->

## Changes
<!-- Bullet list of specific changes -->
-

## Testing
- [ ] Builds for ESP32-S3 (`idf.py build`)
- [ ] Unit tests pass, if applicable
- [ ] Tested on hardware, if applicable
- [ ] No new compiler warnings

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Documentation updated, if needed
- [ ] No heap allocation after init
- [ ] Thread-safe, if shared state

## Notes
<!-- Additional context, trade-offs, or follow-up work -->
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
