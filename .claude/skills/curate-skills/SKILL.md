---
name: curate-skills
description: "Audit and clean up .claude/skills/ to keep only skills that actively support PRD implementation. Removes redundant, outdated, or unused skills and consolidates overlapping ones. Triggers on: curate skills, clean up skills, audit skills, prune skills, skill review."
user-invocable: true
---

# Curate Skills

Audit `.claude/skills/` against the PRDs in `plan/` to ensure every skill earns its place. Fewer, higher-quality skills means Ralph spends less context on irrelevant patterns and produces better results.

## Procedure

### 1. Inventory

List every skill in `.claude/skills/` and for each one note:
- **File/dir name**
- **One-line summary** of what it teaches
- **Size** (approximate line count)

### 2. Cross-reference with PRDs

Read all `plan/prd-*.md` files. For each skill, determine:

| Question | How to check |
|---|---|
| **Referenced?** | Is the skill file explicitly mentioned (`\.claude/skills/...`) in any PRD? |
| **Covered by a story?** | Does at least one user story need the patterns this skill describes? |
| **Up to date?** | Do the patterns match the current state of the codebase, or do they describe code that has already been migrated/deleted? |

### 3. Classify each skill

Assign one of these verdicts:

- **KEEP** — Actively referenced by an incomplete PRD story and its patterns are current.
- **MERGE** — Overlaps significantly with another skill; content should be consolidated into one.
- **UPDATE** — Still relevant but contains stale patterns or outdated file paths.
- **REMOVE** — Not referenced by any PRD, covers a completed story, or duplicates knowledge already in a PRD's "Codebase Patterns" section.

### 4. Present findings

Output a markdown table:

```
| Skill | Verdict | Reason | Action |
|-------|---------|--------|--------|
| string-migration.md | KEEP | Referenced by US-013a | — |
| win32-replacements.md | MERGE | Overlaps with path-normalization.md | Merge into win32-migration.md |
| form-patterns.md | REMOVE | Duplicates react-form-patterns.md | Delete |
```

**Do NOT execute any changes yet.** Wait for user approval.

### 5. Execute approved changes

After the user confirms:

1. **REMOVE**: `git rm` the file.
2. **MERGE**: Create the consolidated file, then `git rm` the originals. Update any PRD references.
3. **UPDATE**: Edit the skill in place with corrected content.
4. Grep all `plan/prd-*.md` files for references to renamed/removed skills and update paths.

### Rules

- Never delete a skill that is the sole source of a pattern needed by an incomplete story.
- When merging, preserve all unique information — don't silently drop content.
- Keep skill files concise. A skill over 200 lines should be split by sub-topic or trimmed.
- If a skill's knowledge has been fully absorbed into a PRD's "Codebase Patterns" section, the skill is redundant — mark it REMOVE.
- Skills that exist only as "nice to have" reference without a concrete PRD story are candidates for REMOVE. PRDs are the source of truth for what work remains.
