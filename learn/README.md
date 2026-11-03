# C++ for Networking: learncpp Chapters 10–22

This folder teaches the C++ you need to build a **secure VPN**. It follows [learncpp.com](https://www.learncpp.com) chapters 10 to 22, every lesson in every chapter, and explains each one with a networking example.

The plan for the whole six months is in **[ROADMAP.md](ROADMAP.md)**.

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

Files 1 and 2 are C++ tools you'll use everywhere. Files 3 and 4 are the networking itself. If you only want sockets right now, files 3 then 4 are the path.

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
