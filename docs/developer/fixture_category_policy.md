# Fixture category policy

Perastage fixture categories are application metadata. They are not a normative GDTF 1.2 or MVR 1.6 category enumeration.

## Precedence and provenance

Import applies category evidence from strongest to weakest:

1. explicit fixture or project metadata, including supported legacy per-fixture metadata;
2. canonical root Perastage `FixtureTypeInfoMap` metadata;
3. dictionary metadata for the resolved fixture type;
4. a cached result for the same resolved fixture type identity;
5. structural and attribute signals in the referenced GDTF;
6. GDTF fixture-name signals;
7. `Unknown`.

An explicit scene value is stable: dictionary and inferred values cannot replace it. `Manual` identifies explicit provenance. `AutoFallback` identifies inferred provenance and carries the public inference reason. Canonical root metadata is keyed and deduplicated by normalized fixture type/GDTF identity. If fixtures conflict, explicit `Manual` category wins; deterministic ordering resolves equally strong visual metadata.

## Strict output and tolerant input

New MVR output stores category and visual metadata only in root Perastage `UserData/Data/FixtureTypeInfoMap`. It embeds every referenced canonical GDTF exactly once under a portable archive identity. Each `GDTFSpec` resolves to an embedded resource, and each non-empty `GDTFMode` resolves inside that resource.

Import may infer safe category signals from an incomplete but well-formed GDTF ZIP/XML document without first requiring strict publication canonicalization. It does not infer a precise category from empty or ambiguous evidence. Missing or malformed descriptions produce `Unknown` with a reason. Name matching uses tokenized, case-normalized content and does not depend on filesystem casing, the caller's working directory, or dictionary save order.

Legacy MVR files may reference an older GDTF filename while containing one unambiguous canonical equivalent. This is tolerant compatibility input, not strict output. Perastage deterministically remaps that reference; ambiguous resources are rejected rather than selected silently.
