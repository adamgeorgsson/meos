# Web Frontend

## Quality Checks

Before committing any changes in this directory, always run these checks from `src/ui/web/`:

```bash
npm run typecheck   # tsc -b --noEmit
npm run lint        # eslint .
npm test            # vitest run
```

All three must pass. Do not commit code that fails typecheck or linting.
