# Core and MVR coverage

Perastage maintains an informational Linux/GCC coverage report for production code
under `core/` and `mvr/`. It also reports `core/inspection/` as an explicit
**Inspection** subset of Core. Tests, generated code, dependencies, GUI, and viewer
modules are outside the reported denominator. There is no line, function, or branch
percentage gate; failures indicate a broken build, test, or report collection rather
than a low percentage.

The dedicated `Core MVR Coverage` workflow runs on manual dispatch and on relevant
pushes to `main`. It does not run for pull requests or pure documentation and version
changes, and it is not one of the required `Protect main` checks. The workflow disables
compiler caching for instrumented project objects while retaining vcpkg dependency
caches. It builds the complete production and test target set, runs registered tests,
and uploads `core-mvr-coverage-report` with:

- `coverage.txt`, a human-readable summary;
- `index.html` and per-file HTML pages;
- `coverage.json` and `coverage.xml`, machine-readable reports;
- `summary.md`, the concise Core, Inspection, MVR, and combined result plus largest
  file gaps.

Inspection lines contribute to both the Core row and the Inspection subset row. The
combined figure remains Core plus MVR only, so the Inspection subset is not counted a
second time. An empty Inspection subset is reported safely as `n/a`. The report is
informational and does not enforce a minimum percentage for any scope.

## Local reproduction

From an already provisioned Linux build environment, install the pinned report tool
and run:

```bash
python3 -m pip install gcovr==8.3
cmake -S . -B build/linux-coverage -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DPERASTAGE_ENABLE_COVERAGE=ON \
  -DPERASTAGE_ENABLE_COMPILER_CACHE=OFF \
  -DPERASTAGE_ENABLE_MVR_XCHANGE_MDNS=OFF
cmake --build build/linux-coverage --target all
xvfb-run -a ctest --test-dir build/linux-coverage --output-on-failure
python3 .github/scripts/generate_core_mvr_coverage.py \
  --build-dir build/linux-coverage \
  --output-dir out/core-mvr-coverage \
  --commit "$(git rev-parse HEAD)"
```

The workflow artifact and job summary are authoritative for current coverage
figures. Local results are suitable for before/after comparison only when the
compiler, report tool, source revision, and selected tests are held constant.

The normative MVR archive assertions in the characterization suite use the official
[MVR 1.6 file format and archive rules](https://github.com/mvrdevelopment/spec/blob/main/mvr-spec.md#file-format),
[GeneralSceneDescription node](https://github.com/mvrdevelopment/spec/blob/main/mvr-spec.md#generalscenedescription-node),
and [UUID type](https://github.com/mvrdevelopment/spec/blob/main/mvr-spec.md#attribute-type-uuid).
