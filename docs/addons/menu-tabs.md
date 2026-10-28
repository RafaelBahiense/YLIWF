# Add-on menu tabs

`include/MenuTabs.h` defines the versioned x64 registration API. Query
`YLIWF_GetMenuAPI` from the core DLL during SKSE `PostLoad`, then call
`registerTab` with a stable ID, title, context and render callback. The core
copies labels and returns a nonzero handle. Identical registration is
idempotent; conflicting IDs, invalid records and more than 16 tabs are rejected.

Render callbacks draw with SKSE Menu Framework's `ImGuiMCP` API. They run on the
menu thread and must not access Skyrim objects or perform gameplay operations.
The supplied context contains the current cached core-managed roster, names,
loaded/waiting/dead flags and ready combat-role bits. Bit positions are the
core's combat-role IDs. Context and follower pointers are valid only for that
callback. Registration grants no gameplay permission; backend commands must
validate current state on the main thread.

Callbacks and their context must remain loaded until process exit. Runtime
unloading and unregistering are unsupported. Register before menus open, do not
throw across the API, and check interface sizes and versions.

The core owns page registration and snapshot refresh. Each add-on owns its UI,
selection state, commands and reports. Gameplay operations use separate,
validated backend services.

The standalone preview discovers add-on `preview/*.cpp` fixtures through their
manifests and compiles each fixture with its production `native/Menu.cpp`.
Fixtures register factories through `tools/ui-preview/include/AddonTabs.h` and
supply simulated backend services.
