Review an existing spec and populate or refresh its decisions and AI Review Questions.

The user will provide a spec path or name at any level (Platform / App / System / Feature).

1. Read the spec
2. Read all parent specs in the chain

For Platform / App / System specs:
3. Check the Decisions table for completeness — are rationale and scope filled in?
4. For each Binding=Yes decision, check whether child specs (if readable) honour it
5. Populate or refresh the AI Review Questions table with any gaps found

For Feature specs:
3. Collect ALL Binding=Yes decisions from the full parent chain
4. For each one, check or populate the Binding Decisions Compliance table
5. Flag any CONFLICT rows and add them to Open Questions
6. Populate or refresh the AI Review Questions table

Present unanswered questions to the user one at a time.
Fill in answers as they respond and update the spec file after each answer.

When all questions are answered and all binding decisions are compliant, summarise
what changed and ask if the status should be updated.
