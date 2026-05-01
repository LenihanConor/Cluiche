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
- Fill in the Traceability table with correct parent links
- Set status to Draft
- Register the feature in the parent system spec's features table

Step 3 — Decision cascade:
Read every Binding=Yes decision from all parent specs (Platform PD-, App AD-, System SD-).
For each one, populate a row in the Binding Decisions Compliance table:
  - Summarise the decision in plain language
  - Explain how this feature's design complies with it
  - If compliance is impossible or unclear, mark as CONFLICT and add to Open Questions

If any conflicts exist, surface them to the user immediately before proceeding.

Step 4 — AI Review Questions:
Read the full spec chain and populate the AI Review Questions table. For every gap,
ambiguity, conflict, or unchecked binding decision found, add a row:
  - Section: which section it relates to
  - Question: clearly stated
  - Suggested Default: your best guess if you have enough context

Present unanswered questions to the user one at a time.
Fill in the Answer column as they respond and update the spec after each answer.

Step 5 — Approval:
Once all Binding Decisions are marked Compliant, all AI Review Questions have answers,
and all Open Questions are resolved, ask:
"All decisions compliant and questions resolved. Shall I mark this spec Approved?"

Plan format (when creating the plan at implementation start):
The plan file must include an ## Implementation Patterns section placed before the task
table. For each phase or major task, document the specific code patterns, classes, and
conventions intended — so they can be validated against the actual implementation before
the task is marked Done.
