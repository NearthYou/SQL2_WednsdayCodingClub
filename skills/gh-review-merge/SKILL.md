---
name: gh-review-merge
description: Address GitHub pull request review feedback, push follow-up commits, and merge the branch into main. Use when a user asks Codex to read PR comments, decide which feedback is valid, implement fixes, rerun tests, reply on review threads, update the PR with new results, or finish the merge.
---

# GH Review Merge

Use this skill to close the loop on a GitHub PR after implementation is mostly done.

## Workflow

1. Inspect the current repository state.
   - Check `git status --short --branch`.
   - Identify the current branch and whether unrelated local changes exist.
   - Read the target PR info and existing review comments with GitHub MCP tools.

2. Triage review comments before editing.
   - Treat comments about correctness, rollback safety, corrupted input handling, encoding, test gaps, and documentation drift as high priority.
   - Mark a comment as non-actionable only when the code already does what the reviewer asked or the suggestion conflicts with the current design.
   - When rejecting a comment, reply with a short technical reason and a code reference.

3. Apply the smallest coherent fix set.
   - Edit only the files needed for the accepted comments.
   - Preserve unrelated user changes.
   - Prefer one commit for one logical review pass.

4. Revalidate after changes.
   - Run the narrowest relevant tests first.
   - Run the repository's broader test script when available.
   - If a reviewer raised a corrupted-input or rollback issue, add or update a regression test.

5. Update GitHub review threads.
   - Push the branch before replying when possible.
   - Reply to each addressed review comment with the commit SHA and a one-line summary.
   - If the toolset cannot edit the PR body, add a top-level PR comment with the latest delta and validation results.
   - If the toolset cannot press GitHub's "Resolve conversation" button, say that explicitly and leave a reply instead of pretending it was resolved.

6. Merge only after the branch is clean and pushed.
   - Confirm the PR is mergeable.
   - If no direct merge API is available, merge locally:
     - `git fetch origin`
     - `git switch main`
     - confirm `main` matches `origin/main`
     - `git merge --no-ff <feature-branch> -m "Merge pull request #<n> from <owner>/<branch>"`
     - `git push origin main`
   - Verify that the PR shows `merged: true` or that `origin/main` points at the merge commit.

## Guardrails

- Do not revert unrelated work in the repository.
- Do not say a review thread was resolved unless a tool actually supports that action.
- If GitHub CLI auth is broken, use GitHub MCP tools for reading and commenting, and local `git` for commit, push, and merge.
- Report any leftover untracked files that were not part of the merge.
