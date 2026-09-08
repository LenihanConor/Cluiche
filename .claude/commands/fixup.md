Auto-diagnose and fix common build/test errors — missing includes, wrong signatures, typos.

Usage: /fixup [error output]

Follow the full protocol in `.claude/skills/fixup.md`. Summary:

1. **Classify** — Match error against known categories: missing include, wrong signature, typo, missing vcxproj source, missing dependency, redefinition, namespace error. If not mechanical, switch to /debug.
2. **Locate** — Find the correct fix (grep for symbol, read declaration, check vcxproj).
3. **Apply** — Minimal edit. Fix all errors of same type in one pass.
4. **Rebuild** — `dia run <target> --build-only`
5. **Report** — Fixed (one-line summary), or loop (max 3 passes), or escalate to /debug.

Constraints:
- Max 3 fix passes before escalating
- Only mechanical errors — design problems escalate immediately
- Never change public API signatures without flagging
- Never add new dependencies without confirming correctness
