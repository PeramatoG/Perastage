# DMX Capability Matrix

This matrix defines the acceptance baseline for incremental PRs that add or improve fixture functionality in `peraviz`.

## Status legend

- **Yes**: Implemented and validated for the current scope.
- **Partial**: Implemented only for a subset of fixtures/modes/ranges, or missing full validation.
- **No**: Not implemented yet.

## Capability coverage

| Capability | GDTF parsing | DMX binding | Runtime visual | Available tests | Known risks |
|---|---|---|---|---|---|
| Dimmer | Partial | Partial | Partial | Limited integration coverage in existing fixture rendering tests. | Inconsistent mapping across fixture profiles may cause brightness jumps. |
| Pan/Tilt | Partial | Partial | Partial | Limited motion-path validation in current runtime tests. | 16-bit coarse/fine handling may differ by fixture definitions. |
| Zoom | Partial | Partial | Partial | No dedicated zoom-only regression suite yet. | Beam angle interpretation can vary between manufacturers. |
| CMY | Partial | Partial | Partial | Covered indirectly by color rendering checks only. | Channel order and inversion differences may produce incorrect color mixing. |
| Gobo | Partial | Partial | Partial | Existing gobo control documentation and focused checks are available, but not a full matrix test suite. | Slot indexing and rotation mode semantics can diverge from fixture specs. |
| Prism | No | No | No | No dedicated prism tests currently. | Multi-facet behavior and indexing standards differ per fixture family. |
| Strobe | Partial | Partial | Partial | Basic intensity/time behavior is partially exercised in runtime tests. | Frequency curves and random strobe modes are not uniformly modeled. |
| Color Wheel | Partial | Partial | Partial | Partial checks exist via color-related fixture scenarios. | Split-color interpolation and wheel indexing can be fixture-specific. |
| Animation Wheel | No | No | No | No dedicated animation-wheel tests currently. | Continuous wheel motion and phase offsets are currently undefined. |
| Iris | No | No | No | No dedicated iris tests currently. | Aperture-to-beam visual response curve is not standardized internally. |
| Frost | No | No | No | No dedicated frost tests currently. | Diffusion model may mismatch manufacturer-level photometric expectations. |

## PR acceptance rule for incremental fixture features

Every incremental PR that touches fixture capabilities **must update this matrix**:

1. Set the status for each affected capability (GDTF parsing, DMX binding, runtime visual).
2. List tests added or executed for the capability.
3. Update known risks when behavior remains partial or unsupported.

A PR is considered complete only when this matrix reflects the current implementation state and test evidence for the changed capability scope.
