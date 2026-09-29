# Dependency Management

## Canonical sources

Official vcpkg builds are reproducible from `vcpkg.json` and its
`builtin-baseline`. `dependencies/dependency-policy.json` classifies maintenance
intent without duplicating versions. The generated
`dependencies/resolved-vcpkg.json` records direct versions, roles, requested
features, groups, and registry license expressions at exactly that baseline;
`.github/badges/wxwidgets.json` is derived from that state. Never edit either
generated file manually or derive it from an installed tree.

Regenerate both files from a vcpkg checkout whose `HEAD` is the manifest baseline:

```bash
python3 scripts/dependencies/generate_state.py --vcpkg /path/to/vcpkg
```

## Maintenance groups

- **feature-performance:** wxWidgets and meshoptimizer can trigger a quarterly
  candidate because their changes directly affect features or performance.
- **security-interop:** network, XML, PDF, compression, and discovery libraries
  can trigger a candidate so security and interoperability changes receive review.
- **stable-runtime:** GLEW, NanoVG, and backward-cpp are reviewed as collateral
  but do not alone trigger a baseline move.
- **build-tooling:** gettext is a host tool and does not alone trigger a move.
- **visual and compatibility:** selected assets are manual; system OpenGL/GLU,
  GTK/libsecret, compiler, and runtime libraries follow supported platforms.

Stable dependencies are not blanket-pinned with overrides because a vcpkg
baseline is global and overrides hide collateral maintenance debt. A human may
add a narrowly justified override in a dependency update PR only when a proven
compatibility break must be held back, with an exit plan and licensing review.

## Quarterly and out-of-band review

The scheduled or manually dispatched dependency review reads registry files as
data, compares every direct package, validates requested features, and produces
a report. A feature-performance or vcpkg security-interop change permits the
workflow to update only the manifest baseline and generated state/badge on a
branch and open a **draft** PR. Stable-runtime/build-tooling-only changes remain
a report. All collateral changes and license-expression changes are visible;
full CI and human review are mandatory and auto-merge is forbidden. Critical
security advisories may start the same workflow out of band rather than waiting
for the quarter.

The repository must grant `GITHUB_TOKEN` read/write workflow permissions and
enable **Allow GitHub Actions to create and approve pull requests** under
**Settings > Actions > General**. The workflow uses no personal access token;
its push and draft-PR steps fail with an actionable settings message when the
repository policy blocks either operation.

For a local no-mutation review using two fixed vcpkg checkouts:

```bash
python3 scripts/dependencies/review.py \
  --current-vcpkg /path/to/current-vcpkg \
  --candidate-vcpkg /path/to/candidate-vcpkg \
  --candidate-baseline <candidate-sha> \
  --report /tmp/dependency-review.md
```

Dry-run is the default: it changes no repository file, pushes nothing, and
opens no PR. `--apply` is reserved for trusted workflow preparation or an
intentional maintainer update.

## Local and platform lanes

Official CI checks out the pinned registry baseline. Windows local development
intentionally uses external classic vcpkg with manifest mode and installation
off; compatible packages may be newer, and the setup inventory reports rather
than rejects version divergence. Linux/WSL system packages are a compatibility
lane using distribution versions and are not exact-version-pinned. Host-only
gettext is reported separately from runtime dependencies.

## Vendored, standards, and legal review

nlohmann/json is checked for quarterly awareness only. Its source replacement,
stb_easy_font, selected/custom Lucide assets, and Noto Sans fonts always require
manual source, behavior, visual, provenance, and license review. Automation must
never replace them. MVR/GDTF schemas and standards resources are governed by
specification compatibility and are explicitly excluded from generic dependency
updates.

Curated notices remain in `THIRD_PARTY_NOTICES.md`; installed vcpkg package
notices staged by `PerastageVcpkgNotices.cmake` are the authoritative supplement.
Any changed registry license expression is a prominent review flag, not an
automatic legal conclusion. A maintainer must review upstream license text,
distribution obligations, and notice coverage before merging.
