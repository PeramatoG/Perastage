# GDTF electrical power

Core's `ReadGdtfPower` is the canonical reader. The legacy snapshot fields
`powerConsumptionWPresent` and `powerConsumptionW` expose only planning/input
power. Absence stays explicit; authored zero remains present and diagnostic.
Raw attribute text, parsed optional values and paths are retained separately
for Consumers, Properties and Beams.

Resolution prefers one unambiguous Consumer `ElectricalPayLoad` (also reads
`ElectricalPayload`), then one Properties/PowerConsumption value, then unknown.
Multiple Consumers, multiple instances of an input, or unresolved input topology
leave planning power unknown even if Properties provides a candidate. Multiple
Properties entries are not summed. A 1% relative / 0.001 W absolute tolerance
identifies material disagreement between the two representations.

Emitter total never supplies planning power. A common named DMX geometry root
is traversed when available. Without named mode roots, unreferenced top-level
roots are traversed; referenced definitions are not also counted as independent
roots. Each GeometryReference expands its target once per physical instance.
An active recursion stack detects cycles, while ambiguous/missing names,
incompatible mode roots, recursion limits and non-finite totals leave the total
unresolved. Invalid Beam values also suppress the derived total, independently
of valid electrical input. No partial total is published.

Diagnostics cover unknown planning power, ambiguous Consumers/Properties,
conflicting electrical values, malformed/negative/zero electrical values,
invalid Beam values, unresolved geometry and emitter total exceeding input.
Input above emitter total is allowed. The Inspector shows resolved power,
canonical source path, informational Beam total and diagnostics. Core diagnostics
are technical English; labels are localized at the GUI boundary.

`Fixture::planningPowerOverrideW` takes precedence over separately retained
`sourcePowerConsumptionW`. The existing effective `powerConsumptionW` remains
usable by calculation consumers. Table/editor power edits are instance overrides;
they do not require writable archives or propagate to other fixture instances.
Clearing the project power edit removes its override and restores the source value.
PSTG stores overrides by fixture UUID in `fixture_planning_power_overrides` in
project configuration, and restores them after importing its scene. Undo snapshots
include the fixture model. Replacing a fixture clears its previous power override.
MVR export does not automatically embed project power overrides into GDTFs.

Explicit GDTF document writes prefer one existing Consumer that is not instanced
through references. Otherwise they require one existing Properties/PowerConsumption
with valid nonnegative Value and a nonempty canonical Connector. Attributes such
as Connector, PowerFactor, voltage and frequency are preserved. No new topology
or incomplete PowerConsumption element is created. Unsafe writes fail before any
weight/description mutation is published, leaving the project override available.
Every correction uses the existing standard Revision infrastructure with effective
change recording; repeated correction actions get separate revisions. No private
GDTF XML is introduced.

Conservative limitations: different mode geometry roots are unresolved at type
level; multiple Consumers are not classified automatically into alternative inputs
or subsystems. Correct planning values in the project when vendor topology cannot
be interpreted safely. There is no new electrical topology editor in this change.
