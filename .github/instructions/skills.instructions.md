---
applyTo: "**"
---

# Available Skills

Reusable skills (migration patterns, code transforms, workflows) are defined in `.claude/skills/`.

Before starting any task, scan that directory for relevant skills:

- Top-level `.md` files are self-contained skill instructions.
- Subdirectories contain a `SKILL.md` (metadata) and a `prompt.md` (full instructions).

Read the matching skill's prompt before writing code — it contains project-specific patterns, gotchas, and examples that must be followed.
