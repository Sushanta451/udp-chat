# Learning: C++ and networking, from the UDP chat to a VPN

Everything I need to learn the basics and finish the UDP chat is in this file. It covers setup, the study order, where I am now, the plan, and how I like to be taught. The six-month plan beyond the chat is in **[ROADMAP.md](ROADMAP.md)**.

> **Helping me in a new session (a tutor or an AI assistant)?** Read this whole file first, especially [How to teach me](#how-to-teach-me) and [Where I am right now](#where-i-am-right-now).

*Last updated: 2026-11-03. All of this work happens on the **`learning`** branch.*

---

## Start here

### This branch

```sh
git clone https://github.com/Sushanta451/udp-chat.git
cd udp-chat
git switch learning
```

Work and commit on `learning`. When the chat is finished and CI is green, merge it into `main`.

### Setting up a computer

The code uses **POSIX sockets** (`<sys/socket.h>`). It builds on **macOS and Linux**, not native Windows.

- **Windows:** install **WSL** (Ubuntu). Run `wsl --install` in PowerShell, reboot, open "Ubuntu", and do everything in there. VS Code + the "WSL" extension edits the files.
- **Linux / WSL packages:** `sudo apt update && sudo apt install -y build-essential clang cmake git`
- **macOS:** `xcode-select --install`, plus `brew install cmake`

```sh
cmake -S learn -B learn/build          # once
cmake --build learn/build              # after every change
./learn/build/net3_beej_ch3_ch5_sockets
ctest --test-dir learn/build --output-on-failure   # the assignment tests

cmake -S . -B build && cmake --build build         # the real chat (src/)
```

Linux differs from macOS in a few small ways: `sockaddr_in` has no `sin_len` field (one more reason to never write your own), the ephemeral port range is 32768–60999, and ports below 1024 need root.

---

## Study order

Do these in order. Each row ends with something you can run.

| # | What | Where | Done when |
|---|---|---|---|
| 1 | C++ chapters 10–22 + F | [chapters/](chapters/), table below | you can answer each file's CHECK YOURSELF out loud |
| 2 | **Toolboxes** (skim once, come back when needed): net1 = what to do when things fail, net2 = files, CMake, flags, random, time | [net1](networking/net1_error_handling.cpp), [net2](networking/net2_program_structure.cpp) | you've run both once. Each file's "WHEN TO OPEN THIS FILE" table says when to come back |
| 3 | **Beej ch 3**: addresses, ports, byte order, `sockaddr`, `inet_pton`, NAT | [beej/ch03_addresses_byte_order_structs.md](beej/ch03_addresses_byte_order_structs.md) | every check-yourself question answered |
| 4 | Beej ch 3 practice | [beej/ch03_practice.cpp](beej/ch03_practice.cpp) (`TODO(human)`) | it prints `PASS` |
| 5 | **Beej ch 5**: `getaddrinfo`, `socket`, `bind`, `sendto`/`recvfrom`, `connect`, `close` | Beej PDF + [net3](networking/net3_beej_ch3_ch5_sockets.cpp). *In-depth guide `beej/ch05_*.md` still to write.* | a **one-way chat** you wrote: one program prints, the other sends lines |
| 6 | **Beej 6.3, 7.1, 7.2**: datagram examples, blocking, `poll()` | Beej PDF, then [net4](networking/net4_beej_ch7_udp_and_poll.cpp) | a **two-way chat** you wrote (try it before reading net4) |
| 7 | Assignment Part A: the packet | [assignment/](assignment/README.md) | packet tests pass |
| 8 | Assignment Part B: the sequence tracker | [assignment/](assignment/README.md) | `19/19 tests passed`, clean under `-fsanitize=address,undefined` |
| 9 | Assignment Part C: **the real chat** in `src/` | [assignment/README.md](assignment/README.md#part-c-make-it-a-real-chat) | see "Done when" below |

---

## Where I am right now

| Item | Status |
|---|---|
| learncpp chapters 10–22 | lessons written; working through them |
| Beej ch 3 guide | ✅ written |
| Beej ch 3 practice: `make_loopback()` | ⬜ **my next task** |
| Beej ch 5 guide | ⬜ **next to write**, in the same format as ch 3 |
| Beej ch 7 guide (blocking, poll, encapsulation) | ⬜ after ch 5 |
| Assignment A + B | ⬜ not started (`assignment_tests`: 2/19) |
| Assignment C (the chat) | ⬜ |
| `src/main.cpp` | ⚠️ **doesn't compile** yet. It has my first attempt at a `sockaddr_in4` struct, which was wrong: a 4-byte family, a 16-bit IPv4 address, and you shouldn't define your own sockaddr at all. Delete it and use `sockaddr_in` from `<netinet/in.h>`. CI stays red until it compiles. |

### What I've already learned (don't re-teach unless I ask)

- **IPv4** = 4 bytes (`uint32_t`), **IPv6** = 16 bytes. IPv6 is stored as a byte array (`uint8_t[16]`), not a 128-bit int; standard C++ has none.
- **Subnets / CIDR** (network part + host part, the netmask AND), **ports** (16-bit; the well-known / registered / ephemeral ranges; port 0 = "OS picks"), **NAT** (the router's mapping table; why keepalives exist).
- **Byte order:** the network is big-endian, my Mac is little-endian. `htons` is needed even Mac-to-Mac, because the kernel requires network order in `sin_port`: `sin_port = 4950` without `htons` silently binds port 22035. Don't convert single bytes, strings, byte arrays, or `inet_pton` output. Convert once, at the boundary.
- **`sockaddr` structs:** generic `sockaddr*` + a specific struct + a length; `sockaddr_storage` for receiving.

---

## Finishing the UDP chat

### Done when

- Two terminals chat: `./build/udpchat --port 9000 --peer 127.0.0.1:9001` and the mirror image.
- `--drop 30` on one side produces correct `gap` / `late` / `duplicate` warnings.
- CI is green on Ubuntu and macOS, and the main README's example output matches what really happens.

### Spec

- **Flags:** `--port <n>` (required), `--peer <ip>:<port>` (required), `--drop <0-100>` (optional, % of sends to skip on purpose). Bad flags print usage and exit 1.
- **Packet:** a 4-byte big-endian sequence number (starting at 1), then the UTF-8 text. Max text is `kMaxText` in [assignment/packet.h](assignment/packet.h). Never `memcpy` a struct onto the wire.
- **Receiving:** `[peer] #N text`, plus `[warn] gap: #4 missing`, `[warn] late: #4`, `[warn] duplicate: #5 already received`.
- **Bad packets** (too short, too long) are ignored. The program never crashes on network input.
- **`/quit`** prints a summary like `received 47, missing 3: #4 #19 #33`.
- **Structure:** `packet.*` and `sequence_tracker.*` go in a `chatcore` library (net2 shows how), with a small `main.cpp`. The socket is a RAII class.

### After the chat

ROADMAP Phase 2: a sliding-window replay bitmap (`std::bitset<64>`, RFC 6479), fuzzing `decode()`, and the basics of IP/UDP headers. Then encryption (Phase 3).

---

## How to teach me

- I'm a beginner-to-intermediate C++ learner. I used to let AI write my code; now I'm learning to write it myself. **Goal:** a secure VPN in C++ by about April 2027, for new-grad jobs. This chat is step one.
- **Plain words, short sentences, analogies.** Define jargon the first time.
- **Go section by section, in depth.** It's my first time seeing this material. For each topic: what it is, worked examples, real scenarios where it breaks, then check-yourself questions with hidden answers. Aim to make me a pro.
- **Code only where it helps.** Concepts like byte order are better taught with tables and worked numbers.
- I'm reading **[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)** (written in C). Teach it **in C++** (RAII, `std::array`, exceptions), and tag everything with its Beej section number. Put new guides in `learn/beej/` (format: `ch03_*.md` + `ch03_practice.cpp`).
- **I write my own chat** in `src/main.cpp`. Review my code and point out bugs. Give hints before solutions. Don't rewrite it for me.
- **Learning mode:** ask me to write small pieces (2–10 lines) at the real design decisions, marked `TODO(human)` in the file, with only one at a time.
- Connect ideas to the VPN where it's natural (replay protection, NAT keepalives, TUN routing).
- **Git:** I commit and push myself, unless I explicitly ask in the moment. **No `Co-Authored-By` or any AI attribution in commits or PRs, ever.** This repo is public.

**To start a new session, say:** *"Read `learn/README.md`. I'm on step N of the study order. Teach me the way it describes, in learning mode."* Then say what you want ("write the Beej chapter 5 guide", "review my main.cpp", "I'm stuck on …"). If your tool has a "learning" output style, turn it on. It's a per-computer setting, so it doesn't come with the repo.

---

## About the C++ lessons

This folder teaches the C++ you need to build a **secure VPN**. It follows [learncpp.com](https://www.learncpp.com) chapters 10 to 22, every lesson in every chapter, and explains each one with a networking example.

## What a VPN actually does (the 30-second version)

A VPN is a program that:

1. **Grabs** packets your computer is about to send (from a fake network card called a *TUN device*).
2. **Locks** them (encrypts them) so nobody in between can read or change them.
3. **Wraps** them in a UDP packet and sends them to the other side.
4. The other side **unwraps**, **unlocks**, and **delivers** them.

Every chapter here teaches a piece of that: how bytes and numbers work (ch10), how to hold a packet (ch16, ch17), how to model a peer or a session (ch13–15), how to manage keys and sockets safely (ch19, ch22), and so on.

## How to study one chapter

1. **Read the chapter on learncpp.com.** It's the textbook.
2. **Read our file** for that chapter, top to bottom. Every lesson has four parts:
   - **IN PLAIN WORDS**: what it is, simply.
   - **WHY A VPN CARES**: where it shows up in network code.
   - **RULE**: what to actually do.
   - a small function that shows it working.
3. **Run it** and match each line of output to the code that printed it.
4. Do the **PRACTICE** exercises at the bottom. Type them yourself.
5. Answer the **CHECK YOURSELF** questions out loud. If you can't, reread that lesson.

Plan on about 2–4 days per chapter. Going slowly is fine. Skipping the practice exercises is not.

## Build and run

```sh
cmake -S learn -B learn/build        # once
cmake --build learn/build            # after every change

./learn/build/ch10_type_conversion   # run a chapter
```

Or compile one file by hand (do this at least once so you see what CMake does for you):

```sh
clang++ -std=c++20 -Wall -Wextra -Wconversion learn/chapters/ch10_type_conversion.cpp -o ch10 && ./ch10
```

## The chapters

| Chapter | File | What you learn | VPN example |
|---|---|---|---|
| 10 | [ch10_type_conversion.cpp](chapters/ch10_type_conversion.cpp) | conversions, narrowing, `static_cast`, `using` aliases, `auto` | a 4-byte value squeezed into a 2-byte header field |
| 11 | [ch11_overloading_and_templates.cpp](chapters/ch11_overloading_and_templates.cpp) | overloading, `= delete`, default args, function templates | one template that reads any header field |
| F | [chF_constexpr_functions.cpp](chapters/chF_constexpr_functions.cpp) | `constexpr`, `consteval`, compile-time checks | protocol sizes checked by the compiler |
| 12 | [ch12_references_and_pointers.cpp](chapters/ch12_references_and_pointers.cpp) | references, pointers, `nullptr`, `const`, out-params, `std::optional` | `recvfrom` filling in the sender's address |
| 13 | [ch13_enums_and_structs.cpp](chapters/ch13_enums_and_structs.cpp) | `enum class`, structs, padding, class templates | message types and why you never `memcpy` a struct onto the wire |
| 14 | [ch14_intro_to_classes.cpp](chapters/ch14_intro_to_classes.cpp) | classes, `private`, constructors, `explicit`, copy constructor | a `Peer` whose secret key can't leak |
| 15 | [ch15_more_on_classes.cpp](chapters/ch15_more_on_classes.cpp) | `this`, header files, destructors, `static`, `friend` | a destructor that wipes key memory |
| 16 | [ch16_vector.cpp](chapters/ch16_vector.cpp) | `std::vector`, the unsigned-index trap, capacity | packet buffers and a replay-protection window |
| 17 | [ch17_arrays.cpp](chapters/ch17_arrays.cpp) | `std::array`, C arrays, decay, C strings | 32-byte keys, and how buffer overflows happen |
| 18 | [ch18_iterators_and_algorithms.cpp](chapters/ch18_iterators_and_algorithms.cpp) | iterators, `<algorithm>`, timing code | constant-time comparison, so attackers can't time you |
| 19 | [ch19_dynamic_allocation.cpp](chapters/ch19_dynamic_allocation.cpp) | `new`/`delete`, leaks, use-after-free, `void*` | the memory bugs behind real VPN security holes (CVEs) |
| 20 | [ch20_functions.cpp](chapters/ch20_functions.cpp) | function pointers, stack vs heap, recursion, `argv`, lambdas | a handler table per message type, and the `printf` bug |
| 21 | [ch21_operator_overloading.cpp](chapters/ch21_operator_overloading.cpp) | every kind of operator overloading, deep copies | an `IPv4Address` type that hands out tunnel IPs |
| 22 | [ch22_move_and_smart_pointers.cpp](chapters/ch22_move_and_smart_pointers.cpp) | move semantics, `unique_ptr`, `shared_ptr`, `weak_ptr` | a socket and a secret key that can't be copied |

### Bonus: networking files

These aren't part of chapters 10–22, but the VPN needs them. Do them **in order**, after chapter 22. Files 3 and 4 follow [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/), translated from C to C++: read the Beej sections first, then the file.

| # | File | Beej sections | What you learn |
|---|---|---|---|
| 1 | [net1_error_handling.cpp](networking/net1_error_handling.cpp) | (C++ prep) | `errno`, exceptions, `optional`, what to do with bad network data |
| 2 | [net2_program_structure.cpp](networking/net2_program_structure.cpp) | (C++ prep) | headers vs `.cpp` files, linking, CMake, command-line flags, random numbers, time |
| 3 | [net3_beej_ch3_ch5_sockets.cpp](networking/net3_beej_ch3_ch5_sockets.cpp) | ch 3, ch 5 | IP addresses, byte order, the `sockaddr` structs, `getaddrinfo`, `socket`/`bind`/`sendto`/`recvfrom` |
| 4 | [net4_beej_ch7_udp_and_poll.cpp](networking/net4_beej_ch7_udp_and_poll.cpp) | 7.1, 7.2, 7.6 | blocking, `poll()`, a packet format: the engine of the chat (and of the VPN) |

**Files 1 and 2 are toolboxes, not networking.** net1 answers "something failed, now what?" and net2 answers "how do I turn one file into a real program with flags and several files?". Skim them once, then reopen them when your chat hits one of the moments in their "WHEN TO OPEN THIS FILE" table. Files 3 and 4 are the networking itself.

### Then: the assignment

**[assignment/README.md](assignment/README.md)**: write the packet format and the sequence tracker for the chat. The tracker is the first version of a real VPN feature called **replay protection**.

## Cheat sheets

### "Who frees this?" (pointers)

| You have... | Use |
|---|---|
| a normal value you own | a plain variable: `Packet p;` |
| a list of bytes, or text | `std::vector<Byte>`, `std::string` |
| a fixed-size key or nonce | `std::array<Byte, 32>` |
| one object on the heap, one owner | `std::unique_ptr<T>` (make it with `std::make_unique`) |
| a C resource (`addrinfo`, `FILE*`) | `std::unique_ptr<T, Deleter>` |
| a socket (an `int`) | a small class that closes it in its destructor (ch22) |
| many owners, no clear single one | `std::shared_ptr<T>` (rare) |
| just *looking* at something | `const T&`, or `const T*` if it might be missing |
| `new` / `delete` by hand | almost never |

### How to pass things into a function

| Thing | Pass it as |
|---|---|
| small: `int`, `uint32_t`, pointers, `string_view`, `span` | by value: `f(int x)` |
| big, and you only read it | `f(const T& x)` |
| you need to change the caller's copy | `f(T& x)` |
| you're going to keep it | by value, then `std::move` it into place |

### Security rules you'll see in every chapter

1. **Data from the network is hostile.** Check every length and every byte before you trust it. Bad input means: drop the packet, keep running. Never crash.
2. **Never write your own encryption.** Use a vetted library like [libsodium](https://doc.libsodium.org/). The "encryption" in these files is a toy.
3. **Compare secrets in constant time** (ch18), so an attacker can't learn anything from how long the comparison takes.
4. **Wipe keys when you're done with them** (ch15, ch22), and don't let them be copied.
5. **Build with the sanitizers on** while developing: `-fsanitize=address,undefined`. They catch memory bugs that would otherwise turn into security holes.

## Tips, since you're used to AI writing your code

- **Type the examples yourself.** Reading code and writing code are different skills.
- **Predict, then run.** Guess the output before you run it. When you're wrong, that's where you learn.
- **Break it on purpose.** The practice exercises have you cause errors. Reading compiler errors is half of C++.
- **Ask AI for hints, not answers.** Try: *"Don't write the code. What concept am I missing?"*
- **Explain it back.** In an interview, you'll need to explain every line of your VPN. Start practicing now.
