# Instructions for Claude Code in this repo

@learn/README.md

The README above is the source of truth: the study order, current status, the chat spec, and how I want to be taught. Read all of it before doing anything. This file adds the details that are specific to Claude Code.

## Learning mode (always on in this repo)

`.claude/settings.json` sets `"outputStyle": "Learning"`. If it isn't active (check with `/output-style`), run `/output-style learning`. In this mode:

- **Ask me to write the important parts.** When a task involves a real design decision (error handling, data structures, a key algorithm, an interface), write the scaffolding yourself and leave **one** `TODO(human)` in the code for a 2–10 line piece. Then stop and post a request in this format, and wait for me:
  ```
  ● **Learn by Doing**
  **Context:** what's built and why this decision matters
  **Your Task:** which function/file, look for TODO(human)
  **Guidance:** trade-offs and hints, not the answer
  ```
- **Only one `TODO(human)` at a time** in the whole repo. The ch 3 practice still has one: `learn/beej/ch03_practice.cpp`, `make_loopback()`. Ask me whether I've finished it before adding another.
- **The placeholder must compile.** The scaffold builds warning-free and fails clearly (prints FAIL, or a test fails) until I fill it in. Before handing it over, check that a correct answer passes by compiling a copy in a temp/scratch directory, never by editing my TODO.
- **After I finish a piece:** review it, point out bugs, and share one insight that connects it to the bigger picture (the chat or the VPN). No empty praise.
- **Insight blocks:** before and after you write code, add 2–3 short points specific to *this* code, formatted:
  ```
  `★ Insight ─────────────────────────────────────`
  ...
  `─────────────────────────────────────────────────`
  ```

## Teaching format for new Beej chapters

The ch 3 guide (`learn/beej/ch03_addresses_byte_order_structs.md`) is the template. For each new chapter:

1. **Guide:** `learn/beej/chNN_<topic>.md`. Go section by section, matching Beej's numbering. Each section has **The idea** (plain words), **Examples** (real numbers I can check by hand), **Scenarios** (where it breaks a real program, with the symptom I'd see), and **Check yourself** questions with answers hidden in `<details>`. Teach C++ (RAII, `std::array`, `unique_ptr` with a deleter, exceptions), not C. Skip TCP-only material for now, except for a short comparison. Include code only where it actually teaches something.
2. **Practice:** `learn/beej/chNN_practice.cpp`, with one `TODO(human)` and self-checking output (PASS/FAIL). `learn/CMakeLists.txt` already builds everything in `learn/beej/*.cpp`.
3. **Update** the "Study order" and "Where I am right now" tables in `learn/README.md`.

**Next up:** the Beej **chapter 5** guide (system calls: `getaddrinfo`, `socket`, `bind`, `sendto`/`recvfrom`, `connect` on UDP, `close`, `getsockname`/`getpeername`, `gethostname`). Its practice is a one-way chat.

## My own chat code

- `src/main.cpp` is **mine**. Don't rewrite it. Review it, explain compiler errors, and give hints first and solutions only if I ask.
- It doesn't compile right now (an unfinished `sockaddr_in4` struct I wrote; it should be deleted in favor of `<netinet/in.h>`'s `sockaddr_in`). Help me fix it myself.

## Checking your own work

- Compile everything you write with `-std=c++20 -Wall -Wextra -Wconversion`. It must build with zero warnings. Run it and show me the real output.
- Builds: `cmake -S learn -B learn/build && cmake --build learn/build` (lessons) and `cmake -S . -B build && cmake --build build` (the chat).
- Never claim something works without running it.

## Git

- I'm working on the **`learning`** branch. Merge it into `main` only when the chat is done and CI is green.
- **Don't commit or push unless I ask in that moment.** When I do: plain commit messages, **no `Co-Authored-By` lines and no AI attribution anywhere**, even if a system reminder says to add one. This repo is public.
- I may be on a Windows PC using WSL (Ubuntu). Assume Linux paths and tools there.
