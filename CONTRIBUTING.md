# Contributing to Perastage

First of all, thank you for taking the time to help improve Perastage.

Perastage is an open-source tool for working with stage, lighting, MVR and GDTF workflows. The project is still growing, so bug reports, ideas, tests with real show files, documentation improvements and code contributions are all very welcome.

You do not need to be an experienced developer to contribute. If you found a bug, have an idea, or something was confusing, that feedback is already useful.

## Ways to contribute

You can help Perastage in several ways:

- Report bugs or crashes.
- Suggest new features or improvements.
- Test the application with real MVR, GDTF, truss or fixture data.
- Improve documentation.
- Help with translations or clearer wording.
- Submit code changes through pull requests.
- Share screenshots, logs or example files when possible.

## Before opening an issue

Before creating a new issue, please check if a similar one already exists.

If it already exists, feel free to add extra information there instead of opening a duplicate. Extra details are always helpful, especially if you can reproduce the issue in a slightly different setup.

## Reporting bugs

When reporting a bug, please try to include as much useful information as possible.

A good bug report usually includes:

- The Perastage version you are using.
- Your operating system.
- What you were trying to do.
- What happened instead.
- The steps needed to reproduce the issue.
- Screenshots or screen recordings, if they help explain the problem.
- Any crash report, log file or error message.
- A sample MVR, GDTF or project file, if you are allowed to share it.

If you cannot share the original file because it belongs to a real production, venue, client or manufacturer, that is completely fine. A simplified test file that reproduces the same problem is also very helpful.

Please avoid uploading private, confidential or copyrighted show files unless you have permission to share them publicly.

## Suggesting features

Feature requests are welcome.

When suggesting a feature, please describe:

- What problem it would solve.
- How you imagine it working.
- Whether it is related to MVR, GDTF, drawing, export, import, visualization, UI, workflow, or something else.
- If another application already does something similar, feel free to mention it.

The more context you provide, the easier it is to understand the real use case behind the request.

## Code contributions

Code contributions are welcome, but for large changes it is usually better to open an issue first. This helps avoid duplicated work or changes that do not fit the current direction of the project.

For small fixes, documentation improvements or simple corrections, you can open a pull request directly.

## Development guidelines

Please try to keep the codebase clean, modular and easy to maintain.

General guidelines:

- Keep changes focused and avoid mixing unrelated modifications in the same pull request.
- Prefer small, clear commits over very large commits.
- Keep business logic separated from the GUI when possible.
- Avoid creating very large files. Split code into smaller modules when it makes sense.
- Write code that is easy to extend and easy to replace at the UI level.
- Use clear names for classes, functions and variables.
- Add concise comments where they help explain intent.
- Comments inside the code should be written in English.
- Avoid unnecessary dependencies unless there is a clear reason to add them.
- Keep the project portable across supported platforms whenever possible.

## Pull request checklist

Before opening a pull request, please check the following:

- The project builds successfully.
- The change has been tested locally.
- The pull request has a clear description.
- The change is related to a specific issue when possible.
- New behavior is documented if needed.
- UI changes include screenshots when useful.
- The code follows the existing project style.
- The change does not introduce unrelated formatting changes.

## Working with MVR and GDTF files

Perastage works with formats that are commonly used in real productions, manufacturers' libraries and lighting workflows.

When contributing changes related to MVR or GDTF:

- Try to follow the official format structure as closely as possible.
- Be careful with compatibility.
- Avoid assumptions that only work with one specific exporter or manufacturer.
- Test with more than one file when possible.
- Keep imported and exported data as predictable as possible.

If you are not sure whether a behavior is correct according to the standard, mention it in the issue or pull request. Discussion is welcome.

## Documentation contributions

Documentation improvements are very welcome.

This includes:

- Fixing typos.
- Improving explanations.
- Adding screenshots.
- Clarifying installation steps.
- Explaining workflows.
- Updating outdated information.
- Making the documentation easier for new users.

Clear documentation is especially important because many Perastage users may come from the live events, lighting or stage design world rather than from software development.

## Be friendly and constructive

Please keep discussions respectful and constructive.

It is completely fine to disagree about how something should work, but try to explain the reason behind your opinion. The goal is to make Perastage better for everyone using it.

## New to GitHub?

No problem.

If you are new to GitHub and only created an account to report something, you are still very welcome here.

Just open an issue and explain the problem as clearly as you can. Screenshots, steps to reproduce the problem, and example files are often more useful than technical language.

## License

By contributing to Perastage, you agree that your contributions will be released under the same license as the project.

Please only contribute code, files or documentation that you have the right to share.

## Localization

New user-facing UI text must be marked for gettext translation in the same change that introduces it, while project data, imported data, identifiers, serialization keys, protocol values, and diagnostic-only logs remain stable and untranslated. See `docs/localization.md` for the marking, catalog-update, audit, and translator workflows, and `docs/localization_glossary.md` for preferred Spanish terminology.
