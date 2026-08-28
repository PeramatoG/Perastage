## Summary

<!-- Briefly explain what this pull request changes and why. -->

## Related issue or discussion

<!-- Link the relevant issue or discussion, for example: Closes #123. Write "N/A" when this is a small standalone correction. -->

## Scope

<!-- List the behavior, modules, or files intentionally changed by this pull request. -->

## Out of scope

<!-- List related work intentionally deferred or excluded. Write "N/A" when there is nothing relevant to note. -->

## User-facing impact

- [ ] New feature
- [ ] Improvement
- [ ] Bug fix
- [ ] Performance
- [ ] Stability / reliability
- [ ] Documentation
- [ ] Internal only

User-facing release note:

<!-- Write one short user-friendly sentence suitable for release notes, or write "Internal only". -->

## Architecture and implementation

<!-- Describe affected modules, ownership boundaries, significant design decisions, new dependencies, or intentional deviations from existing patterns. Write "N/A" for straightforward documentation or isolated changes. -->

## MVR / GDTF compatibility

<!-- For changes affecting MVR or GDTF, describe the applicable standard behavior, compatibility fallbacks, exporters or manufacturers considered, and unsupported cases. Write "N/A" when unrelated. -->

- [ ] Input remains permissive where safe and appropriate.
- [ ] Perastage-created or exported output remains strict and canonical.
- [ ] Original external GDTF files are not mutated merely because they were read.
- [ ] Not applicable.

## Testing

Platforms tested:

- [ ] Windows
- [ ] macOS
- [ ] Linux
- [ ] Not applicable

Validation performed:

- [ ] Built successfully
- [ ] Relevant automated tests passed
- [ ] Tested manually
- [ ] Regression test added or updated
- [ ] Architecture checks passed when applicable
- [ ] Not applicable

Commands, configurations, sample files, and results:

<!-- Include enough information for a reviewer to reproduce the validation. Distinguish pre-existing failures from failures introduced by this pull request. -->

## Documentation and localization

- [ ] User documentation updated where required
- [ ] Developer or technical policy documentation updated where required
- [ ] New or changed GUI presentation text is gettext-ready
- [ ] Catalogs are synchronized when applicable
- [ ] COMPLETE catalogs have no missing or fuzzy entries
- [ ] `perastage_check_translations` passed
- [ ] Intentional CLI, protocol, and technical strings remain stable English
- [ ] Documentation links remain valid
- [ ] No documentation or localization update is required

Documentation changed or reason not required:

<!-- Mention the relevant files or briefly explain why no update is needed. -->

## Visual evidence

<!-- Add screenshots or recordings for meaningful UI or rendering changes. Write "N/A" when unrelated. -->

## Third-party code, assets, and data

<!-- Identify the source and license of any new external code, models, images, fonts, fixture files, or other assets. Confirm that confidential production material is not included. Write "N/A" when none were added. -->

## Known limitations and follow-up work

<!-- Describe deliberate limitations, compatibility concerns, migration notes, or follow-up tasks. Write "None" when there are no known limitations. -->

## Contributor checklist

- [ ] The pull request contains one coherent change.
- [ ] Unrelated formatting or refactoring changes were avoided.
- [ ] The code and comments are written in English.
- [ ] Business logic and file-format logic remain outside the GUI where practical.
- [ ] New responsibilities were placed in an appropriate module instead of growing a hotspot unnecessarily.
- [ ] New dependencies, external material, and licensing have been documented.
- [ ] I understand and take responsibility for all submitted code, including AI-assisted code.

## Release notes draft

- [ ] Updated `docs/release-notes-draft.md`
- [ ] Not needed because this change is internal only
- [ ] Not needed for another documented reason above
