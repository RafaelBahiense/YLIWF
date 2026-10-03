# Credits

**Original mod:** Simple Follower Framework by **Ivy / ItzIvy05** and contributors.
Their follower behavior, scripts, and record design are the foundation of YLIWF.
YLIWF substantially rewrites the architecture while retaining a largely similar
gameplay feature set; see [architecture changes](docs/architecture.md).

- [Original repository](https://github.com/ItzIvy05/SimpleFollowerFramework)
- [Original Nexus release](https://www.nexusmods.com/skyrimspecialedition/mods/174017)

**Fork maintainer:** Me. Copyright attribution and the dated fork origin are in
[NOTICE](NOTICE). Individual changes and contributors are recorded in Git history.
The incorporated upstream revision carries GPL-3.0.

## Libraries and development tools

| Component | Project | Role |
| --- | --- | --- |
| CommonLibSSE-NG | [CharmedBaryon/CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) | Skyrim/SKSE C++ interfaces |
| fmt | [fmtlib/fmt](https://github.com/fmtlib/fmt) | Formatting |
| spdlog | [gabime/spdlog](https://github.com/gabime/spdlog) | Logging |
| SKSE-MCP | [QTR-Modding/SKSE-MCP](https://github.com/QTR-Modding/SKSE-MCP) | Menu framework interface |
| rapidcsv | [d99kris/rapidcsv](https://github.com/d99kris/rapidcsv) | CSV support |
| Mutagen | [Mutagen-Modding/Mutagen](https://github.com/Mutagen-Modding/Mutagen) | C# ESP authoring and inspection |
| Caprica | [Orvid/Caprica](https://github.com/Orvid/Caprica) | Papyrus compiler |
| vcpkg | [microsoft/vcpkg](https://github.com/microsoft/vcpkg) | Native dependency resolution |
| SKSE64 | [ianpatt/skse64](https://github.com/ianpatt/skse64) | Game runtime and compiler imports |

Dependency notices accompany release archives in the `licenses/` directory beside
this file. Packaging details are in the source archive's `docs/build-release.md`.
Bethesda owns Skyrim and its game content. Compiler SDK inputs come from the
user's installed Creation Kit.
