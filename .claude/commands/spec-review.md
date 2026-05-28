Review an existing spec and refresh its decisions and open design questions.

The user will provide a spec path or name at any level (Platform / App / System / Feature).

1. Read the spec
2. Read all parent specs in the chain

For Platform / App / System specs:
3. Check the Decisions table for completeness — are rationale and scope filled in?
4. For each Binding=Yes decision, check whether child specs (if readable) honour it
5. Surface any new open design questions based on gaps found

For Feature specs:
3. Check that constraining Binding=Yes decisions from parents are listed
4. Flag any CONFLICT or missing constraints
5. Check open design questions — are they still relevant? Any new ones?

Present findings to the user.
When done, summarise what changed and ask if the status should be updated.
