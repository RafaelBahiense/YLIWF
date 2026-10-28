# Add-on development

Use the public APIs below to build gameplay integrations, diagnostics and tools.

| Contract | Documentation |
| --- | --- |
| Menu registration, cached context and preview fixtures | [Menu tabs](menu-tabs.md) |
| External follower ownership, commands, movement and core-owned policies | [Follower integration](follower-integration.md) |

Public headers live in `include/`. Add-ons must not include core private headers.
Check API sizes and versions, validate live state before gameplay operations,
and keep Skyrim access on the main thread. Registered callbacks must remain valid
until process exit; runtime unloading is unsupported.

## Source and build

```text
src/addons/<addon>/
  addon.json
  README.md
  native/       # C++ implementation and private headers
  plugin/       # Optional Mutagen ESP definitions
  papyrus/      # Optional scripts
  preview/      # Optional UI simulation fixtures
  tests/native/ # Native test entry points (*_test.cpp)
```

Create directories only when needed. Each add-on's README covers its requirements,
installation and behavior; detailed implementation notes belong in its `docs/`.

Manifests declare an independent name and version:

```json
{"name": "Example", "version": "1.0.0", "adapterApiVersion": 1}
```

`adapterApiVersion` declares the follower adapter API version. Query and validate
each host interface used by the implementation.

CMake discovers native components and `*_test.cpp` tests from add-on manifests.
Papyrus sources compile against the core bridge and ship in the add-on archive.
Optional ESPs declare `pluginFile` and provide an `IAddonPluginGenerator`;
ModTools discovers those generators in its assembly.

Builds record input hashes and package each add-on separately with its matching
source archive and notices. Core and add-on versions remain independent.
See the [build guide](../build-release.md) for commands and release requirements.
External authors can use their own SKSE project with the public SDK headers.
