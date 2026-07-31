# Contributing to Perastage

Thank you for taking the time to help improve Perastage.

Perastage is a free and open-source desktop application for stage, lighting, MVR, and GDTF workflows. The project is actively developed and welcomes contributions from software developers, lighting professionals, testers, translators, technical writers, and users working with real production files.

You do not need to be an experienced developer to contribute. A clear bug report, a reproducible test file, documentation feedback, or verification on a different operating system can be as valuable as a code change.

## Ways to contribute

You can help Perastage by:

- Reporting bugs, crashes, compatibility problems, or regressions.
- Suggesting workflow improvements and new features.
- Testing development builds on Windows, macOS, or Linux.
- Testing with real MVR, GDTF, truss, fixture, and project files.
- Improving documentation, translations, and user-facing wording.
- Adding or improving automated tests.
- Reviewing technical behavior against the official MVR and GDTF specifications.
- Submitting focused code changes through pull requests.

## Project communication

Use the most appropriate GitHub area:

- **Issues** for reproducible bugs, accepted tasks, and clearly scoped feature requests.
- **Discussions** for questions, early ideas, design exploration, and proposals that still need their scope defined.
- **Pull requests** for concrete changes that are ready for review or active work shown as a draft.

For significant features, architectural changes, new dependencies, file-format behavior, or broad refactors, open an issue or discussion before implementation. This reduces duplicated work and helps confirm that the proposed direction fits the project.

Small fixes, tests, documentation improvements, and straightforward corrections may be submitted directly as pull requests.

## Before opening an issue

Search existing open and closed issues before creating a new one. If a similar issue already exists, add any new reproduction steps, logs, affected files, or platform details there instead of opening a duplicate.

Do not upload confidential, private, or copyrighted production files unless you have permission to share them publicly. A reduced test file that reproduces the same problem is usually preferable.

## Reporting bugs

A useful bug report should include, when available:

- The exact Perastage version, commit, release, or development artifact used.
- The operating system and relevant hardware details.
- What you were trying to do.
- What you expected to happen.
- What happened instead.
- Clear steps to reproduce the problem.
- Screenshots or a short recording when they clarify visible behavior.
- Crash reports, logs, warnings, or error messages.
- A minimal MVR, GDTF, PSTG, or other sample file when sharing is permitted.
- Whether the problem also occurs in the latest available development build.

If the issue cannot be reproduced by a maintainer, you may be asked to test a development artifact or provide additional diagnostics. This does not mean the report is being dismissed; some problems depend on a specific operating system, driver, locale, file, or machine configuration.

## Suggesting features

Feature proposals should explain:

- The real workflow problem being solved.
- Who would benefit from the change.
- How the proposed workflow should behave from the user's perspective.
- Which areas are likely to be affected, such as MVR, GDTF, drawing, import, export, visualization, UI, or project persistence.
- Similar behavior in other applications, when relevant.
- What should remain outside the initial scope.

Prefer a small first deliverable over a large all-at-once implementation. Complex features may be divided into checkpoints or incremental pull requests.

## Contribution workflow

External contributors should normally work through a fork. Direct write access to the main repository is not required.

1. Fork `PeramatoG/Perastage` on GitHub.
2. Clone your fork locally.
3. Add the main repository as an `upstream` remote.
4. Create a focused branch from the current `upstream/main`.
5. Implement and test one coherent change.
6. Push the branch to your fork.
7. Open a pull request targeting `PeramatoG/Perastage:main`.

Example setup:

```bash
git clone https://github.com/<your-user>/Perastage.git
cd Perastage
git remote add upstream https://github.com/PeramatoG/Perastage.git
git fetch upstream
git switch -c fix/short-description upstream/main
```

Recommended branch prefixes include:

- `fix/` for bug fixes.
- `feature/` for new functionality.
- `refactor/` for behavior-preserving restructuring.
- `docs/` for documentation.
- `tests/` for test-only work.
- `build/` for build, dependency, CI, or packaging work.

Branch names should be short, descriptive, and use lowercase words separated by hyphens.

## Draft pull requests

Open a **draft pull request** early for work that needs architectural discussion, affects several modules, or may take multiple iterations. A draft allows maintainers to review the direction before the implementation becomes expensive to change.

A draft pull request does not need to be complete, but it should state:

- The intended goal.
- The current scope.
- Important design decisions.
- Known limitations or unresolved questions.
- Which parts are ready for review.

Mark the pull request ready for review only when its intended scope is complete, the relevant checks have been run, and the description reflects the final behavior.

## Keep pull requests focused

Each pull request should represent one coherent change.

- Avoid mixing unrelated fixes, refactors, formatting changes, and features.
- Avoid repository-wide reformatting unless it is the explicit purpose of the pull request.
- Refactor incrementally around the code being changed.
- Avoid large rewrites when a staged migration is possible.
- Document intentionally deferred work instead of silently expanding the scope.

Small and focused pull requests are easier to understand, test, review, revert, and maintain.

## Development principles

Perastage should remain professional, modular, portable, and maintainable.

- Write all new or updated code, identifiers, code comments, and developer documentation in English.
- Use clear and responsibility-oriented names for files, classes, functions, and variables.
- Keep business logic and file-format logic outside the GUI whenever possible.
- Design services and domain logic so that the GUI can be changed without rewriting core behavior.
- Avoid adding major responsibilities to already large files.
- When touching a file near approximately 1200-1500 lines, prefer extracting a responsibility before adding substantial new behavior.
- Keep module ownership explicit in CMake. Do not use recursive source globbing for project sources.
- Prefer interfaces or helpers owned by the responsible module over direct cross-module coupling.
- Avoid unnecessary dependencies. New dependencies require a clear technical reason, compatible licensing, and cross-platform consideration.
- Preserve Windows, macOS, and Linux compatibility whenever the affected code is intended to be portable.
- Add concise comments to explain intent, constraints, or non-obvious decisions; do not narrate self-explanatory code.

Architecture and repository conventions are documented under `docs/developer/`, especially:

- `docs/developer/architecture.md`
- `docs/developer/repository_layout.md`
- `docs/developer/build.md`
- `docs/developer/packaging.md`
- `docs/developer/documentation_policy.md`

More specialized technical policies in that directory are authoritative for the areas they cover.

## MVR and GDTF policy

MVR and GDTF behavior must follow the official specifications as closely as reasonably possible.

Perastage applies an intentional compatibility model:

- **Reading should be permissive.** When safe and unambiguous, Perastage should accept legacy, incomplete, non-canonical, or externally generated files so users can recover and work with real production data.
- **Writing should be strict and canonical.** Files created, modified, normalized, or exported by Perastage should follow the supported official MVR and GDTF structure and should not introduce undocumented custom structures into standard data.

Changes affecting MVR or GDTF must therefore:

- Verify behavior against the applicable official specification or schema.
- Avoid assumptions tied to one manufacturer, exporter, visualizer, or console.
- Preserve unknown but valid standard data whenever possible.
- Keep tolerant input repair separate from canonical output generation.
- Avoid mutating original external GDTF files merely because they were read.
- Use shared readers, writers, canonicalizers, mutation services, and diagnostic types instead of duplicating format logic in GUI code.
- Test with multiple representative files when possible.
- Add regression tests for corrected parsing, serialization, canonicalization, or compatibility behavior.
- Document deliberate deviations, compatibility fallbacks, or unsupported areas.

For current GDTF write behavior, see `docs/developer/gdtf_mutation_policy.md`. Local specification references and related policies are available under `docs/reference/` and `docs/developer/`.

When specification text, schemas, existing behavior, and compatibility needs appear to conflict, describe the conflict in the issue or pull request rather than hiding it behind a workaround.

## Project data and UI boundaries

Keep these categories separate:

- Standard GDTF type-level data.
- Standard MVR scene-instance data.
- Perastage project state and overrides.
- Derived or cached values.
- GUI-only selections and transient interaction state.

Do not store project-specific or GUI-specific state inside GDTF or MVR standard fields unless the format defines that meaning. Non-standard behavior must be isolated, documented, and prevented from leaking into canonical exports.

## Documentation requirements

Update documentation in the same pull request when a change adds, removes, renames, or significantly changes:

- A user-facing feature or workflow.
- A preference or visible UI behavior.
- A supported file type.
- Import, export, packaging, or installer behavior.
- A keyboard shortcut or its scope.
- Text-to-scene parsing or generation behavior.
- An architectural contract or module boundary.
- MVR or GDTF write behavior.

Minor internal refactors, formatting-only changes, and invisible maintenance work do not require user documentation unless they alter an established technical contract.

Avoid duplicating long explanations across files. Maintain one source of truth and link to it from relevant entry points.

## Localization

New user-facing UI text must be marked for gettext translation in the same change that introduces it.

Project data, imported data, identifiers, serialization keys, protocol values, and diagnostic-only logs must remain stable and untranslated.

See:

- `docs/developer/localization.md`
- `docs/developer/localization_glossary.md`

## Testing and validation

Build and test the affected configuration locally whenever reasonably possible. The supported setup and commands are documented in `docs/developer/build.md`.

At minimum, contributors should:

- Build the affected target successfully, or explain why a local build was not possible.
- Test the changed behavior manually when it has user-facing effects.
- Run relevant automated tests.
- Add or update tests for regressions, parsers, serializers, persistence, and non-trivial business logic.
- Avoid weakening or deleting a valid test only to make a change pass.
- Distinguish pre-existing failures from failures introduced by the pull request.

The following architecture checks are mandatory for code changes that can affect their scope:

```bash
tests/check_perastage_tree_modules.sh
tests/check_no_configmanager_get_in_gui.sh
```

When a new architectural boundary is introduced, add a small and focused `tests/check_*.sh` or equivalent validation in the same pull request when practical.

GitHub Actions will run repository checks on pull requests targeting `main`. A green CI result supports review but does not replace manual verification or code review.

## Pull request description

A pull request should clearly state:

- What problem it solves.
- What was changed.
- What was intentionally left out.
- Which issue or discussion it relates to.
- Which modules and architectural boundaries are affected.
- How the change was tested.
- Which platforms were tested.
- Which MVR, GDTF, or project files were used, when relevant.
- Any user-facing impact and proposed release-note text.
- Known limitations, compatibility concerns, or follow-up work.
- The origin and license of any new third-party code, assets, schemas, or sample data.

Include screenshots or recordings for meaningful visual UI changes.

During review, respond to comments by updating the pull request, explaining the reasoning, or discussing alternatives. Do not resolve a technical concern only by hiding or suppressing the symptom.

## Commits and merge policy

Use clear commit messages that describe the purpose of the change. During development, multiple small commits are acceptable; contributors are not expected to rewrite their history repeatedly just to make review easier.

The maintainer decides when a pull request is ready to merge and which merge method is appropriate. Focused contribution pull requests will normally be squash-merged so that `main` receives one clear and revertible commit.

Do not force-push after review has begun unless history cleanup is necessary and reviewers are informed, because force-pushing can invalidate review references.

## AI-assisted contributions

AI-assisted development is allowed. Contributors may use Codex, Claude, ChatGPT, code completion tools, or other assistants.

However:

- The contributor remains fully responsible for every submitted line and design decision.
- Generated code must be understood, reviewed, tested, and adapted to Perastage conventions.
- Do not submit invented APIs, unsupported format claims, fabricated test results, or copied material with uncertain licensing.
- Mention substantial AI assistance when it is relevant to understanding the implementation or review process.
- Never include credentials, private production files, personal data, or other confidential material in prompts or generated artifacts.

Tool usage does not reduce the required standards for correctness, maintainability, licensing, security, or specification compliance.

## Third-party code, assets, and licenses

Only contribute code, documentation, fixtures, profiles, schemas, models, images, fonts, or other assets that you have the right to share.

When introducing third-party material:

- Identify its source.
- Confirm that its license is compatible with Perastage.
- Add required attribution and license files.
- Avoid copying code from projects with unclear or incompatible terms.
- Prefer official specifications, schemas, and primary documentation for format behavior.

Do not include real show files, venue plans, manufacturer data, or client material without permission.

## Security issues

Do not report security vulnerabilities in a public issue. Follow the private reporting instructions in `SECURITY.md`.

## Review, acceptance, and project direction

Submitting a pull request does not guarantee that it will be merged. A change may require revision, be divided into smaller steps, remain open while design questions are resolved, or be declined when it conflicts with project scope, standards compliance, portability, maintainability, or current priorities.

The maintainer is responsible for final integration decisions, repository access, release publication, and the protection of `main`. External contributors normally work through forks and pull requests; direct repository write access is not required to participate fully.

Disagreement is welcome when it is constructive. Explain the technical or workflow reason behind a proposal, consider alternatives, and prioritize the long-term quality of the project over ownership of a particular implementation.

## Be friendly and constructive

Perastage follows the `CODE_OF_CONDUCT.md`. Keep discussions respectful, patient, and focused on improving the project.

Many contributors and users come from live events, lighting, production, or stage design rather than traditional software-development backgrounds. Clear explanations and practical reproduction details are more valuable than unnecessary jargon.

## New to GitHub?

You are welcome even if this is your first open-source contribution.

For a first contribution, documentation, tests, small bug fixes, and reduced reproduction files are good starting points. Open an issue or discussion when you need help defining the scope before writing code.

## License

By contributing to Perastage, you agree that your contribution will be released under the same license as the project, currently the GNU General Public License v3.0.

See `LICENSE.txt` for the complete license terms.
