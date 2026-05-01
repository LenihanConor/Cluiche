Trace the full lineage of a feature from Platform down to implementation.

The user will provide a feature name or path.

1. Find the feature spec in docs/specs/features/
2. Read its Traceability table
3. Read the parent system spec
4. Read the parent application spec
5. Read the platform spec

Output a summary:
- Platform: [name + one-line summary]
  - Application: [name + purpose]
    - System: [name + responsibility]
      - Feature: [name + purpose + status]
        - Tasks: list with checkboxes
        - Open Questions: list if any
        - AI Review Questions: X answered / Y total

Flag any AI Review Questions across the chain that are still unanswered.

Also surface all Binding=Yes decisions from the chain and their compliance status:
  ✅ Compliant · ⚠️ Conflict · ⬜ TBD

Then answer: "Is there anything in the platform or application spec that constrains
this feature's implementation — and is the feature currently compliant with all
binding decisions?"
