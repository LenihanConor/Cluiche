Create a new feature spec. Run an interview before writing anything.

Step 1 — Interview the user:
1. Which application and system does this feature belong to?
2. What is the feature name?
3. What problem does it solve? (one sentence)
4. What are the acceptance criteria? (list until they say done)
5. Any data models or API shapes already decided?
6. Any files or modules you know it will touch?
7. Any known open questions or blockers?

Step 2 — Write the spec:
- Create docs/specs/features/<app>/<s>/<feature-name>.md
- Add a `Parent:` line linking to the parent system spec
- Set status to Draft
- Register the feature in the parent system spec's features table

Step 3 — Binding Decisions:
Read Binding=Yes decisions from parent specs (Platform PD-, App AD-, System SD-).
Only list decisions that actually constrain this feature's design — not a full compliance
matrix. For each constraining decision:
  - State the decision
  - Explain how this feature complies
  - If compliance is impossible or unclear, mark as CONFLICT and surface to the user

If no parent decisions constrain this feature, state "No binding constraints apply."

Step 4 — Open Design Questions:
Surface 2-3 real design uncertainties or risks — things the user might want to revisit
during implementation. These should be specific to this feature, not generic checklists.
Skip this step entirely if the design is straightforward.

Present questions to the user. Fill in answers as they respond.

Step 5 — Approval:
Ask: "Shall I mark this spec Approved?"

Plan format (when creating the plan at implementation start):
The plan file must include an ## Implementation Patterns section placed before the task
table. For each phase or major task, document the specific code patterns, classes, and
conventions intended — so they can be validated against the actual implementation before
the task is marked Done.
