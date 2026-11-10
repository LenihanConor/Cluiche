# CoW (Crucible of Wings)

Game built on Dia + CluicheGameBaseline. Sibling application to CluicheTest/CluicheEditor — see [docs/specs/applications/cow/cow.md](../../docs/specs/applications/cow/cow.md).

This is a directory skeleton only — no `.vcxproj`/solution wiring yet. That's a separate, deliberate follow-up (touches `Cluiche/Cluiche.sln` and build files shared with every other app).

- `Shared/` — CoW-specific code reused across more than one CoW system (tier 3 of the four-tier model in the app spec). Code specific to a single feature stays with that feature instead.
