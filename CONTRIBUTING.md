# Contributing

## Workflow

1. Start from an Issue with an explicit scope and acceptance criteria.
2. Add or update proportional UML documentation before implementing behavior or architecture.
3. Obtain design approval, then create a dedicated branch from `main`.
4. Keep changes focused and follow Clean Architecture, SOLID, KISS, DRY, and YAGNI.
5. Use Angular Conventional Commits: `type(scope): description`.
6. Open a pull request linked to the Issue and address all review comments before merging.
7. Merge only after explicit maintainer approval, then remove the merged branch.

Documentation-only changes do not require new UML when they do not alter behavior or architecture.
CAN transmission requires a dedicated safety design and remains disabled by default.

## Validation

Use the isolated test environment and run:

```bash
.venv-test/bin/pytest
.venv-test/bin/ruff check .
.venv-test/bin/mypy
```

New features must maintain at least 90% total coverage and include happy-path, sad-path, and
edge-case tests. Never commit credentials, `.env` files, real captures, or charger-identifying
data.

## Pull requests

Describe the change and its validation, link the Issue with `Closes #<number>`, and complete this
checklist:

- [ ] The design documentation is current and approved when required.
- [ ] Happy paths, sad paths, and edge cases are covered when behavior changes.
- [ ] Tests pass with at least 90% total coverage.
- [ ] Ruff and strict mypy checks pass.
- [ ] No credentials, captures, or charger-identifying data are included.
