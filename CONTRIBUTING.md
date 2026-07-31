# Contributing to ClaVel

ClaVel is at `v0.1.0` — early and experimental. Every contribution matters, whether it's a bug report, a documentation fix, or a new feature. Thank you for considering helping.

## Ways to contribute

- **Bug reports** — open an issue with a minimal reproduction
- **Bug fixes** — pick an open issue, comment that you're working on it, open a PR
- **Documentation** — typos, unclear explanations, missing examples
- **Features** — check the roadmap in the README first; open an issue to discuss before building

## Development setup

```bash
# Dependencies (Linux)
sudo apt install gcc cmake ninja-build libsqlite3-dev

# Clone and build
git clone https://github.com/cbbeandresvargas/clavel
cd clavel
cmake -B cmake-build -G Ninja
cmake --build cmake-build

# Run
./cmake-build/clavel_app

# Watch mode (auto-rebuild on changes)
./cmake-build/clavel-cli watch
```

## Workflow

```bash
# 1. Fork the repo on GitHub, then clone your fork
git clone https://github.com/<your-username>/clavel
cd clavel
git remote add upstream https://github.com/cbbeandresvargas/clavel

# 2. Create a branch off main
git checkout -b fix/csrf-null-crash
# or
git checkout -b feat/websocket-support

# 3. Make your changes, then build and test
cmake --build cmake-build
./cmake-build/clavel_app

# 4. Commit with a clear message
git commit -m "fix: resolve NULL dereference in CSRF middleware on fresh sessions"

# 5. Push and open a PR
git push origin fix/csrf-null-crash
```

## C code style

| Rule | Example |
|---|---|
| Functions: `Module_verb_noun` | `Request_parse_post`, `DB_table` |
| Types: `PascalCase` | `QueryBuilder`, `TemplateData` |
| Constants/macros: `SCREAMING_SNAKE` | `CLAVEL_MAX_ROUTES`, `DB_DRIVER_SQLITE` |
| Indentation: 4 spaces | (no tabs) |
| Line length: ≤ 100 chars | |
| No dynamic allocation | Use `Arena_alloc`, never `malloc`/`free` in request handlers |
| Prepared statements only | Never interpolate values into SQL strings |
| Comments: explain *why*, not *what* | Non-obvious invariants only |

## Memory model

Every request runs inside a 1 MB `Arena`. All per-request allocations must use:

```c
Arena_alloc(arena, size)
Arena_strndup(arena, str, len)
Arena_strdup(arena, str)
Arena_sprintf(arena, fmt, ...)
```

Never call `malloc`/`calloc`/`strdup` in request handlers — those allocations would leak. The `Arena` is destroyed after the response is sent; `O(1)`, no fragmentation.

## PR checklist

- [ ] The code builds without errors: `cmake --build cmake-build`
- [ ] No new compiler warnings introduced
- [ ] No `malloc`/`free` in request handlers (use arena)
- [ ] No string interpolation in SQL (use ORM with prepared statements)
- [ ] PR description explains **what** and **why**

## Commit messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
fix: resolve NULL dereference in CSRF middleware
feat: add WebSocket upgrade support
docs: clarify arena allocator lifetime in README
chore: bump mongoose to 7.15
```

## Questions?

Open an issue — there's no Discord yet, but we respond to GitHub issues.
