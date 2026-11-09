# Native source layout

| Directory | Responsibility |
| --- | --- |
| `runtime/` | Run lifecycle, startup checks, reporting and restoration orchestration |
| `testing/` | Suite, case and phase API, scheduling and resource cleanup |
| `scenarios/` | Encounter definitions and scenario-specific rules |
| `fixtures/` | Opponent construction, equipment, spells and protection |
| `arena/` | Travel, observer positioning and scene control |
| `configuration/` | Tuning profile parsing and validation |
| `ui/` | Combat Tests menu rendering and controls |
| `diagnostics/` | Spell execution diagnostics |
| `telemetry/` | Combat observations, measurements and comparisons |
| `interference/` | Controls for other combat systems during a run |
| `integrations/` | Engine hooks and runtime compatibility checks |

Implementation files and private headers stay together. Engine-independent rules
live beside the subsystem that uses them. `tests/native/` mirrors these groups;
each `*_test.cpp` is discovered recursively as an independent test executable.

Start with [the testing API](testing/README.md) to define a suite, or
[`DefenderSuite.h`](scenarios/DefenderSuite.h) for the defender cases.
