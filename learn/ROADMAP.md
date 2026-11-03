# Roadmap: From Zero to a Secure VPN (Oct 2026 → Apr 2027)

**The goal:** by April 2027, have a small VPN written in C++ that you understand line by line, can demo, and can defend in an interview.

**The plan in one line:** learn C++ → finish the chat → add encryption → add a TUN device → add a real handshake → polish.

Each phase builds on the one before. Each one ends with something that **works** and that you could show someone.

---

## Phase 1: C++ foundations (Oct 9 → Nov 15, about 5 weeks)

**Do:**
- Chapters 10–22 in [README.md](README.md), in order. About 2–4 days each.
- The three bonus networking files.
- Assignment Parts A and B (the packet format and the sequence tracker).

**You're done when:**
- `assignment_tests` says `19/19 tests passed`, and is still clean when built with `-fsanitize=address,undefined`.
- Without notes, you can explain: `unique_ptr` vs a raw pointer, why `uint8_t + uint8_t` is an `int`, why you never `memcpy` a struct onto the wire, and what RAII means.

**If you have extra time:** learncpp chapters 24–25 (inheritance, virtual functions) and 27 (exceptions).

---

## Phase 2: Finish the chat, and make it solid (Nov 15 → Dec 15)

**Do:**
- Assignment Part C: the working two-terminal chat with `--drop`.
- Split the code into files with a `chatcore` library (net2 shows how), and run the tests in CI.
- **Replace the tracker's `std::set` with a sliding-window bitmap** (`std::bitset<64>` plus the highest counter seen). That is *exactly* how WireGuard and IPsec reject replayed packets. Search for "RFC 6479" to read the design.
- **Fuzz `decode()`.** A fuzzer throws millions of random byte strings at your parser to find crashes. Use clang's `-fsanitize=fuzzer` (libFuzzer) on Linux. Fix everything it finds.
- Learn the basics: IP header, UDP header, MTU, and NAT. ([Beej's Guide](https://beej.us/guide/bgnet/) plus the Wikipedia pages are enough.)

**You're done when:** two terminals chat, the warnings are correct with `--drop 30`, the fuzzer runs for 10 minutes without a crash, and CI is green.

**Interview story:** *"My parser takes hostile input, so I fuzzed it, and here's the bug the fuzzer found."*

---

## Phase 3: Encryption (Dec 15 → Jan 20)

**Learn the ideas first (no code yet):**
- **Symmetric encryption:** both sides share one secret key.
- **AEAD** (Authenticated Encryption with Associated Data) encrypts the data **and** detects any tampering. You'll use **ChaCha20-Poly1305**, the same cipher WireGuard uses.
- **Nonce:** a number used once per message. **Reusing a nonce with the same key breaks the encryption completely.** Your packet counter becomes the nonce, which is why the counter must only ever go up (ch14).
- **Key exchange (X25519):** two sides agree on a secret over a public network without ever sending it.

**Do:**
- Install [libsodium](https://doc.libsodium.org/) and link it with CMake.
- Encrypt the chat: a pre-shared key from a file, each message sealed with `crypto_aead_chacha20poly1305_ietf_encrypt`, the counter used as the nonce, and anything that fails to decrypt dropped.
- Keep keys in a move-only `SecretKey` class that wipes itself (`sodium_memzero`). See ch15 and ch22.

**You're done when:** Wireshark shows only random-looking bytes on the wire, and a flipped bit in transit gets the packet rejected.

**Never:** invent your own cipher, use XOR "encryption", or reuse a nonce.

---

## Phase 4: The tunnel, a real VPN (Jan 20 → Feb 25)

**Use Linux for this phase.** Use the VM you already have, from the `192.168.64.6` example in [main.cpp](../src/main.cpp). Linux's `/dev/net/tun` is much simpler than macOS's `utun`.

**Do:**
- Open a **TUN device**. It's a fake network card: when your computer sends to `10.8.0.2`, the packet shows up in your program as raw bytes.
- The loop: `poll()` on the TUN fd **and** the UDP socket (net4 has the shape).
  - TUN → encrypt → UDP `sendto` the peer.
  - UDP `recvfrom` → check replay → decrypt → write to TUN.
- Set up the addresses and routes with `ip addr` / `ip route`, using a shell script at first.
- Handle the MTU. Encryption adds bytes, so the tunnel's MTU must be smaller than the real one (WireGuard uses 1420).

**You're done when:** `ping 10.8.0.2` works **through your tunnel**, and `iperf3` measures real throughput.

**This is the moment it becomes a VPN.**

---

## Phase 5: A real handshake (Feb 25 → Mar 25)

Pre-shared keys don't scale, and they have no *forward secrecy*: if a key leaks, old traffic can be read. Fix that.

**Do:**
- Read the [WireGuard whitepaper](https://www.wireguard.com/papers/wireguard.pdf). It's short and readable. Focus on the handshake and the timers.
- Give each peer a static X25519 key pair. Exchange **ephemeral** keys in a two-message handshake, and derive the session keys with a KDF (`crypto_kx` in libsodium is a simple starting point; the *Noise IK* pattern is what WireGuard does).
- Rekey every N minutes or N messages. Send keepalives. Handle a peer whose IP address changes (*roaming*).
- Write a **threat model**: who are you protecting against, what can they do, and what's out of scope. One page is enough.

**You're done when:** the tunnel comes up with only public keys in the config, survives a peer changing networks, and the keys rotate.

---

## Phase 6: Polish and present (Mar 25 → Apr 2027)

Recruiters and interviewers will look at this. Make it easy for them.

- **README:** what it does, an architecture diagram, how to run it in two minutes, the threat model, and benchmarks.
- **Honesty section:** "This is a learning project, not audited, don't use it to protect real traffic." Saying this shows maturity, not weakness.
- **Tests and CI:** unit tests, a fuzzer job, and sanitizer builds.
- **Benchmarks:** throughput with `iperf3` compared to no VPN, and where the time goes (`perf`).
- **A 2-minute demo video**, plus a short blog post: "What I learned building a VPN in C++."
- **Interview prep:** for every file, be ready to answer "why did you do it this way?"

---

## Weekly rhythm (about 10–15 hours a week)

| When | What |
|---|---|
| 3 weekday evenings | learn: read, run, do the exercises |
| 1 weekend block (3–4 h) | build: the current phase's project work |
| 30 min on Sunday | write 5 bullet points: what you learned, what's confusing. These become your blog post and your interview stories. |

## When you get stuck

1. Reread the error message slowly. Then search for the exact text.
2. Make the smallest program that shows the problem.
3. Ask AI to **explain**, not to fix: *"Why does this error happen?"*
4. If you're still stuck after an hour, move on and come back tomorrow. You'll often see the problem right away.

## What you'll be able to say in April

> "I built a VPN in C++20 on Linux. It captures packets from a TUN device and encrypts them with ChaCha20-Poly1305 via libsodium. It does an X25519 handshake with forward secrecy, rejects replays with a sliding window, and has fuzzed parsers and sanitizer-clean CI. Here's my threat model, and here's what I'd do differently."

That's a strong new-grad project. Let's build it.
