# CI vcpkg binary cache

`ci-tests.yml` and `vcpkg-binary-cache.yml` share
`.github/scripts/vcpkg_cache_contract.py`, schema **v5**. Both restore and save
exactly `.vcpkg-cache/binary` (including the same slash spelling on Windows).
Actions cache versions also depend on paths and compression; identical key text
alone does not make different path lists compatible. Downloads retain their
separate v3 cache. Installer workflows keep their existing independent contracts.

Only ABI-addressed ZIP archives are cached by these two workflows. Installed and
packages trees are reconstructed by vcpkg on each fresh runner, so cached installed
state cannot bypass its binary ABI decisions. No ABI overrides, compiler hash
suppression, package renaming, or SDK metadata rewriting are used.

The key prefix includes OS, architecture, triplet, runtime boundary, tool identity,
vcpkg baseline, and the existing manifest/configuration hash. Linux runtime identity
is distribution plus release (for example `ubuntu-24.04` versus `ubuntu-26.04`).
Compiler versions/target, glibc, linker, CMake and Ninja versions select a preferred
snapshot. `ImageVersion`, dates and run IDs are excluded. Fallback first relaxes
the manifest, then tools/baseline within the same runtime boundary; vcpkg still
checks each archive's ABI. There is no fallback across Ubuntu releases or macOS
SDK identities. Keep `ubuntu-latest` and `windows-latest`: migration creates a new
runtime boundary without permanently freezing runners. macOS retains its existing
hash of Xcode version and resolved SDK path and its SDK metadata guard.

Actions caches are immutable. The restore lookup ends in `snapshot`; saved keys
instead contain a deterministic digest of archive names and contents. Repairs also
include a digest of the restored key, so an older complete inventory can supersede
a newer incomplete snapshot even if that inventory was saved previously. Prefix restore
selects the newest matching snapshot. Thus a normal warm restore reports a fallback
hit, even with identical environment and dependencies. After successful install,
changed contents get a new key, including after an incomplete exact/fallback hit;
unchanged contents skip saving. Empty snapshots are not saved. Bump `SCHEMA` for a
contract change or an intentionally retired generation; do not introduce dates.
Concurrent saves of identical contents can race harmlessly; check the cache action
log for actual upload confirmation, not just its step outcome.

Remote NuGet configuration remains centralized. Test jobs request `read`, and only
the main-only warming workflow requests `readwrite` with `packages: write` and an
exact trusted SHA checkout. NuGet credentials remain under `RUNNER_TEMP`, outside
cached paths and artifacts. Configuration validation proves local XML structure,
not feed authorization or publication. PR cache writes remain isolated to their
merge ref. Arbitrary source dispatches cannot save persistent vcpkg snapshots or
downloads, using the existing trusted-source scope resolver.

## Evidence and remaining uncertainty

[CI run 41187027988](https://github.com/PeramatoG/Perastage/actions/runs/41187027988),
Windows job 124495961654, restored an exact v4 binary snapshot, restored **0** local
and **0** NuGet packages, rebuilt dependencies (vcpkg reported 51 minutes), then
skipped saving. NuGet returned package-specific `NotFound` URLs containing ABI
versions. This demonstrates that the exact snapshot was insufficient, not that
all remote caching is useless. The warming workflow at the audited revision used
v3 mixed-tree caches on Windows, incompatible with the test job's v4 archive path
and key. Linux used mixed installed trees with no release/tool identity.

The most recent manually dispatched warming run found during this audit,
[30166602199](https://github.com/PeramatoG/Perastage/actions/runs/30166602199), succeeded,
but its job logs now return HTTP 410. Publication of the currently requested ABI
versions could not be established. Linux job-log retrieval failed through the
connector, and its telemetry artifact download returned HTTP 403. Linux remote
restore latency therefore remains unmeasured in this audit. A package 404 can mean
absence, an unpublished ABI, or hidden access restrictions; configuration success
alone cannot distinguish these. Remote caching is retained pending measurements.

## Cold/warm validation

1. Run `python3 -m unittest discover -s tests/ci -p test_vcpkg_cache_contract.py`.
   After merging, dispatch **vcpkg Binary Cache** on **main**, initially with
   `bypass_local_compiled_cache=true`. This prevents local snapshots from hiding
   remote behavior and seeds missing ABIs. Inspect the install log's NuGet upload
   reports and package versions in GitHub Packages. Existing installed packages or
   locally restored archives do not necessarily republish to NuGet.
2. Dispatch the same main SHA again with bypass still true, on the same compatible
   environment. Compare environment summaries and representative `.abi.json`
   files. Require matching requested ABI package versions and successful NuGet
   read-back; successful setup or an upload report alone is insufficient. Summaries
   show NuGet restored counts and provider duration separately from full install
   duration. Investigate 401/403, package-specific 404s, and different compiler/ABI
   hashes before attributing zero hits or Linux latency to the cache backend.
3. Dispatch warming with bypass **false** to publish a default-branch v5 Actions
   snapshot. On the new schema this is the cold local run. Repeat on the same
   compatible environment for the warm local run, then dispatch **CI Debug Tests**
   on main with profile `pr` (or `release`) to execute the existing CTest suites.
   Compare matched key, restored/rebuilt counts, install duration, and save action
   logs. A warm prefix hit and skipped unchanged-content save are expected. Record
   both durations; do not infer a speedup from cache-hit status alone.

A trusted main warming run is required to publish remote packages; main snapshots
are reusable by PRs, while PR-scoped snapshots cannot seed main. If the runner or
compiler changes between validation runs, compare identities and ABIs before
calling the second run warm. Install logs/ABI files are retained with existing
telemetry artifacts, including successful warming runs. Counts across retries are
restore events and unique build-start package identifiers, not proof of remote
publication. No post-change CI timings are claimed by this change.
