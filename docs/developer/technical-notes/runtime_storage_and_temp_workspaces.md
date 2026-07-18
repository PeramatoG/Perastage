# Runtime storage and temporary workspace refactor

Perastage now has a GUI-independent runtime storage layer for operation-scoped workspaces, scene/session resource leases, and session caches. The refactor moves audited MVR import, merge, export staging, GDTF loader extraction, truss staging/cache paths, MVR-xchange receive staging, OBJ conversion output, primitive previews, and dictionary bundle staging under a single Perastage-owned temporary root.

The central rule is that raw temporary paths must not cross subsystem boundaries without an accompanying owner. Operation-only directories use `TemporaryWorkspace`; scene resources are transferred to shared leases stored by `MvrScene`; cache entries own or reside below the session cache root.
