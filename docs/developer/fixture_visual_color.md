# Fixture visual-color resolution

Perastage keeps two color properties with different ownership. `visualColorHex`
is Perastage project-instance presentation metadata used by plans and user
interfaces. `mvrFixtureColorHex` represents the official MVR `Fixture/Color`
value converted between CIE xyY and RGB for editing; exporting it continues to
use the standards-compliant CIE path.

The GUI-independent `fixture_visual_color` domain module owns validation,
canonical uppercase `#RRGGBB` normalization, resolution, and grouped-color
aggregation. Display consumers must not recreate these rules.

Resolution uses this precedence:

1. a valid project-instance color;
2. only while restoring a project whose fixture metadata entry is absent, a
   valid official MVR fixture color recovered into the project value;
3. a valid automatic fixture-type color supplied by the dictionary boundary;
4. an explicit-empty or unresolved structured result.

A present `hasVisualColorHex="false"` entry is an intentional empty value. It
is never populated from MVR or an automatic default. Its presentation remains
the existing neutral/no-fill treatment. Invalid metadata is diagnosed by the
importer and remains permissive, but its presence prevents it from being
mistaken for absent legacy metadata. Ordinary external MVR imports never run
the project migration. A recovered value is held in `visualColorHex`, so the
normal project writer persists it canonically on the next save without
changing `mvrFixtureColorHex`.

For a legend or summary group, equal normalized resolved colors produce that
color. Different valid colors, or valid and intentional-empty values together,
produce the deterministic mixed neutral/no-fill presentation. A group of only
unresolved values also remains neutral/no-fill. Fixture grouping identity is
unchanged by this policy.
