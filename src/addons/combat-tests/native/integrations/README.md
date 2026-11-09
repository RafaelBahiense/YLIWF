# Fixture health writes

`HealthWrites` supplies the runtime-specific write bridge; `FixtureProtection`
owns the active opponent scope and suppression policy. No invulnerability flag
is required. Only finite negative Health damage modifiers for the published
opponent references are suppressed. Other writes retain their original arguments.

The Skyrim 1.6.1170 profile identifies five direct callers of the shared
actor-value backend: two queued-write sites, a transfer site, the public
actor-value wrapper and combat hit processing. Address Library IDs locate the
functions; offsets select the verified calls. Installation checks runtime,
`CALL rel32` opcode and destination for every site before changing any of them.
Modified/unsupported callsites leave controlled-opponent tests unavailable.

Each call forwards to the original backend entry, preserving hooks installed
there. Callsites are checked again before scope acquisition and while reporting
protection. Relays are allocated within the intersection of all five signed
rel32 ranges; allocation failure leaves the sites untouched.

The startup preflight exercises the public wrapper and the installed combat
callsite's actual destination. Both must increment their own suppression counter
without changing opponent health. Reports separate public, combat and other
write counts, including those probes. Ongoing health checks invalidate a run if
a write bypasses protection. This does not intercept arbitrary direct memory
writes by another plug-in.

Scope changes occur on the game thread. Hook callbacks read immutable actor IDs
and update atomic counters. Finish, reset and pre-load clear the active scope;
ordinary actors, the finite-health ally and natural encounters remain unprotected.
