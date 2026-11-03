# Progress and handoff: read this first

This file says where the project stands, how I like to be taught, and exactly what's left to finish the UDP chat. If you're helping me in a new session (a tutor or an AI assistant), read this whole file and then [ROADMAP.md](ROADMAP.md) before doing anything.

*Last updated: 2026-11-03*

---

## 1. Who I am and how to help me

- I'm a beginner-to-intermediate C++ learner. I used to let AI write my code; now I'm learning to write it myself.
- **Goal:** a secure VPN in C++ by about April 2027, for new-grad job applications. This UDP chat is the first step. See [ROADMAP.md](ROADMAP.md).
- **How to teach me:**
  - Explain in plain words, short sentences, analogies. Define jargon the first time.
  - Go **section by section and in depth**. It's my first time seeing this material. Aim to make me a pro at each topic: what it is, examples, real scenarios where it goes wrong, then check-yourself questions.
  - Show code only where code actually helps. Concepts like byte order can be taught with tables and worked numbers.
  - I'm reading **[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)** (written in C). Teach it **translated to C++** (RAII, `std::array`, exceptions), and map every lesson to its Beej section number.
  - **I write my own chat** in `src/main.cpp`. Review my code and point out bugs. Give hints before solutions. Don't rewrite my code for me.
  - Ask me to write small pieces (2–10 lines) at the design decisions, with a `TODO(human)` marker in the file.
  - Connect ideas to the VPN where possible (replay protection, NAT keepalives, TUN routing).
- **Git rules:** I commit and push myself. Only do it if I explicitly ask in that moment. **No `Co-Authored-By` or any AI attribution lines in commits or PRs, ever.** This repo is public.

---

## 2. Setting up on a new computer

The chat uses **POSIX sockets** (`<sys/socket.h>`). It builds on **macOS and Linux**, not native Windows.

- **Windows PC:** install **WSL** (Ubuntu) and do everything inside it: `wsl --install` in PowerShell, reboot, open "Ubuntu". VS Code + the "WSL" extension lets you edit the files.
- **Inside Linux/WSL:**
  ```sh
  sudo apt update && sudo apt install -y build-essential clang cmake git
  git clone https://github.com/Sushanta451/udp-chat.git
  cd udp-chat
  git switch learning-materials     # until it's merged into main
  ```
- **Build the lessons:**
  ```sh
  cmake -S learn -B learn/build
  cmake --build learn/build
  ./learn/build/net3_beej_ch3_ch5_sockets
  ```
- **Build the real chat:** `cmake -S . -B build && cmake --build build`

Linux differences from macOS that you'll notice: `sockaddr_in` has no `sin_len` field (one more reason to never write your own), the ephemeral port range is 32768–60999, and ports below 1024 need root.

---

## 3. Where everything is

| Path | What |
|---|---|
| `src/main.cpp` | **My chat.** I write this myself. |
| `learn/README.md` | Index of all lessons, with the study order |
| `learn/ROADMAP.md` | The 6-month plan, from the chat to the VPN |
| `learn/chapters/` | C++ lessons, learncpp.com chapters 10–22 |
| `learn/networking/net1`, `net2` | C++ prep: error handling, multi-file programs, argv, random, chrono |
| `learn/networking/net3_beej_ch3_ch5_sockets.cpp` | Beej ch 3 and 5 in C++: runnable demos of every socket call the chat needs |
| `learn/networking/net4_beej_ch7_udp_and_poll.cpp` | Beej ch 7: `poll()`, the chat's engine |
| `learn/beej/ch03_*.md` | **The in-depth written guide for Beej chapter 3** |
| `learn/beej/ch03_practice.cpp` | Practice for chapter 3 (has my `TODO(human)`) |
| `learn/assignment/` | Packet format + sequence tracker, with 19 tests |

---

## 4. Status right now

| Item | Status |
|---|---|
| learncpp chapters 10–22 lessons | written; I'm working through them |
| Beej ch 3 in-depth guide | ✅ written (`learn/beej/ch03_addresses_byte_order_structs.md`) |
| Beej ch 3 practice: `make_loopback()` | ⬜ **my next task**: fill the `TODO(human)` until it prints `PASS` |
| Beej ch 5 in-depth guide | ⬜ **next to write**, in the same format as ch 3 |
| Beej ch 7 (blocking, poll, encapsulation) guide | ⬜ after ch 5 |
| Assignment Part A (packet) and B (tracker) | ⬜ not started: `assignment_tests` says 2/19 |
| Assignment Part C (the real chat) | ⬜ |
| `src/main.cpp` | ⚠️ **doesn't compile.** It has my first attempt at a `sockaddr_in4` struct. That attempt was wrong: it used a 4-byte family and a 16-bit IPv4 address, and you shouldn't define your own sockaddr at all. Delete the struct and use `<netinet/in.h>`'s `sockaddr_in`. CI will be red until it compiles. |

### What I've already learned (don't re-teach unless I ask)

- IPv4 = 4 bytes, IPv6 = 16 bytes. IPv6 is stored as a byte array, `uint8_t[16]`, not as a 128-bit int; standard C++ has none.
- Subnets/CIDR, ports and their ranges, NAT and why keepalives exist.
- Byte order: why `htons` is needed even Mac-to-Mac (the kernel requires network order in `sin_port`; forgetting it binds the wrong port silently), and when *not* to convert (bytes, strings, byte arrays, `inet_pton` output).
- The `sockaddr` family and why you cast plus pass a length.

---

## 5. The plan to finish the UDP chat (in order)

Each step ends with something you can run. Don't skip ahead.

1. **Beej ch 3:** read `learn/beej/ch03_*.md` and answer every check-yourself question. Finish `ch03_practice.cpp` → `PASS`.
2. **Beej ch 5 guide** (to be written next, same format): `getaddrinfo` in depth (hints, the linked list, `AI_PASSIVE`, freeing it with a `unique_ptr`), `socket`, `bind`, `sendto`/`recvfrom` (UDP message boundaries, truncation, the in/out length), `connect` on UDP, `close`/RAII, `getsockname`/`getpeername`. Skip TCP-only `listen`/`accept`/`send`/`recv`, except for a short comparison. Practice: a **one-way chat**: one program binds and prints, the other reads `std::getline` and sends.
3. **Beej ch 6.3 + 7.1 + 7.2:** blocking, and `poll()` on stdin + the socket at the same time → a **two-way chat** with no sequence numbers yet. Study `net4` *after* trying it yourself.
4. **Assignment Part A:** `encode`/`decode` of `[4-byte big-endian seq][text]`. Byte order in practice (shifts, not `memcpy`). `decode` must survive hostile input.
5. **Assignment Part B:** the sequence tracker: gap / late / duplicate. → `19/19 tests passed`. Rerun with `-fsanitize=address,undefined`.
6. **Assignment Part C:** wire it all together in `src/` (see `learn/assignment/README.md` Part C): flags `--port --peer --drop`, a RAII socket, the `poll` loop, warnings, a summary on `/quit`. Add a `chatcore` library to the root `CMakeLists.txt`.
7. **Done when:** two terminals chat, `--drop 30` produces correct warnings, CI is green on Ubuntu + macOS, and the README example output matches what really happens.
8. **Then ROADMAP Phase 2:** a sliding-window replay bitmap, fuzzing `decode()`, the IP/UDP header basics.

### The chat's spec (so nothing is ambiguous)

- Run: `./build/udpchat --port 9000 --peer 127.0.0.1:9001 [--drop 10]`
- Packet: a 4-byte big-endian sequence number (starting at 1), then the UTF-8 text. Max text = `kMaxText` in `learn/assignment/packet.h`.
- Receive output: `[peer] #N text`, plus `[warn] gap: #4 missing`, `[warn] late: #4`, `[warn] duplicate: #5 already received`.
- `/quit` prints a summary like `received 47, missing 3: #4 #19 #33`.
- Bad packets (too short or too long) are ignored, never a crash.

---

## 6. Prompt to start a new session

> Read `learn/PROGRESS.md` and `learn/ROADMAP.md`. I'm on step N of section 5. Teach me in the style described there. Use learning mode: give me `TODO(human)` pieces to write. [Then say what you want: "write the Beej chapter 5 guide", "review my main.cpp", "I'm stuck on …"]

If your tool has a "learning" output style, turn it on for this project. (It's a per-computer setting, so it didn't come with the repo.)
