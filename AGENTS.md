# Agent Instructions

## Worktrees — mandatory

- Create worktrees only at `<repo-parent>/<repo-name>.worktrees/<branch>`.
- Never create worktrees inside the repository, including `.claude/worktrees`.
- Base new task worktrees on the latest `origin/main` unless the user requests another ref.
- Initialize submodules separately in every worktree.
