---
name: cow-mode
description: Switch this session into CoW (Crucible of Wings) mode — the dedicated `cow` git worktree/branch, which carries a permission guardrail requiring explicit approval before editing Dia, CluicheTest, CluicheEditor, CluicheGameBaseline, GoogleTests, or shared build/spec files. Use when starting or resuming CoW game work.
---

Call `EnterWorktree` with `path: ".claude/worktrees/cow"` to switch this session into the CoW worktree.

If that path doesn't exist, tell the user the `cow` worktree is missing and stop — do not create a new one under that name without asking; recreating it loses the branch's guardrail `.claude/settings.json` unless it's restored from git first.

Once switched, confirm to the user: the current branch is `cow`, and `Edit`/`Write`/`NotebookEdit` outside `Cluiche/CoW/**` and `docs/specs/applications/cow/**` will now prompt for approval. To leave, use `ExitWorktree` (`action: "keep"` to preserve the worktree, which is almost always what you want here since `cow` is a persistent branch, not a throwaway task worktree).
