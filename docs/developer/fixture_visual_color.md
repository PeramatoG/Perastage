# Fixture visual-color resolution

Perastage keeps two color properties with different ownership. `visualColorHex`
is Perastage project-instance presentation metadata used by plans and user
interfaces. `mvrFixtureColorHex` represents the official MVR `Fixture/Color`
value converted between CIE xyY and RGB for editing; exporting it continues to
use the standards-compliant CIE path.

`Fixture::visualColorState` durably distinguishes missing project metadata,
present metadata, and an intentional `ExplicitEmpty`. The separate
`automaticVisualColorHex` candidate holds type metadata, so automatic values
are never mistaken for project-instance input.

The GUI-independent `fixture_visual_color` domain module owns validation,
canonical uppercase `#RRGGBB` normalization, resolution, and grouped-color
aggregation. Display consumers must not recreate these rules.

Resolution uses this precedence:

1. a valid project-instance color;
2. only while restoring a project whose fixture metadata entry is absent, a
   valid official MVR fixture color recovered into the project value;
3. a valid root, dictionary, or deterministic automatic fixture-type color;
4. an explicit-empty or unresolved structured result.

A present `hasVisualColorHex="false"` entry is an intentional empty value. It
is never populated from MVR or an automatic default. Its presentation remains
the existing neutral/no-fill treatment. Invalid metadata is diagnosed by the
importer and remains permissive, but its presence prevents it from being
mistaken for absent legacy metadata. Ordinary external MVR imports never run
the project migration. A recovered value is held in `visualColorHex`, so the
normal project writer persists it canonically on the next save without
changing `mvrFixtureColorHex`.

`ResolveFixturePresentationColor` is the fixture-level boundary used by the 3D
opaque and selection paths, fixture summaries, and shared layout legends.
Layout preview, print, and PDF legends consume the shared legend result.
Automatic persistence only changes `Missing` fixtures, so refresh and scene
synchronization preserve `ExplicitEmpty`; save and reload retain both the state
and any canonical recovered project value.

For a legend or summary group, equal normalized resolved colors produce that
color. Different valid colors, or valid and intentional-empty values together,
produce the deterministic mixed neutral/no-fill presentation. A group of only
unresolved values also remains neutral/no-fill. Fixture grouping identity is
unchanged by this policy.
