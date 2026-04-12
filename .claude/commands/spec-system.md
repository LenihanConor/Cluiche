Create a new system spec.

Ask the user:
1. Which application does this system belong to?
2. What is the system name?
3. What does it own / what is its responsibility?
4. What are its public interfaces (APIs, events)?
5. What other systems does it depend on?

Then:
- Create docs/specs/systems/<app-name>/<system-name>.md using the system spec template
- Register the system in the parent application spec's systems table
- Read parent Platform and App specs and populate the Inherited Binding Decisions table
  with every Binding=Yes decision that applies to this system
