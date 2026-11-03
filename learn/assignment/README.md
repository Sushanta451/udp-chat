# Assignment: Build the Chat's Core

You'll write the two pieces of `udpchat` that don't need a network: the **packet format** and the **sequence tracker** that detects lost, late, and duplicate messages. Tests tell you when each piece works. Then you connect them to real sockets and get a working chat.

**Why this matters for your VPN:** both pieces come back later in real form.
- The packet format becomes the VPN's wire format: a header with a counter, then the encrypted data.
- The sequence tracker becomes **replay protection**. An attacker can record one of your encrypted packets and send it again later. It still decrypts fine, so the VPN has to notice "I've seen counter #5 already" and drop it. That's the `Duplicate` case you're about to write. WireGuard and IPsec do exactly this.

The interfaces (`.h` files) are already written. You fill in the `.cpp` files.

## Files

| File | You... |
|---|---|
| [packet.h](packet.h) | read it. It defines `Packet`, `encode`, `decode`, and the wire format. |
| [packet.cpp](packet.cpp) | **implement Part A** (TODOs A1–A7). |
| [sequence_tracker.h](sequence_tracker.h) | read the rules and worked example; **add private members** (TODO B0). |
| [sequence_tracker.cpp](sequence_tracker.cpp) | **implement Part B** (TODOs B1–B5). |
| [tests.cpp](tests.cpp) | don't edit. 19 tests. |

## Running the tests

```sh
cmake -S learn -B learn/build          # once
cmake --build learn/build && ./learn/build/assignment_tests
```

Run just one part:

```sh
./learn/build/assignment_tests packet
./learn/build/assignment_tests tracker
```

At the start, 2/19 pass (the ones that happen to match the placeholder code). You're done when it says `19/19 tests passed`.

Once everything passes, run the tests again with the sanitizers on, to catch memory bugs the tests can't see:

```sh
cmake -S learn -B learn/build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build learn/build-asan && ./learn/build-asan/assignment_tests
```

## Part A: the packet (chapters 10, 11, 12.15, 16, 17)

Implement `encode` and `decode` in [packet.cpp](packet.cpp).

`Packet{300, "hi"}` must become the bytes `00 00 01 2c 68 69`: four bytes of sequence number, biggest byte first, then the text.

Things the tests check that are easy to miss:

- `decode` must **never crash** on bad input. A packet with 3 bytes, 0 bytes, or 5000 bytes returns `std::nullopt`.
- A header byte like `0x80` must not turn negative. Do your shifting in `std::uint32_t`.
- `decode` receives a `span` that might be a *slice* of a bigger buffer (that's how `recvfrom` works). Only use the bytes the span gives you.
- Text longer than `kMaxText` makes `encode` throw `std::length_error`.

<details>
<summary>Hint: I'm stuck on encode</summary>

`static_cast<std::uint8_t>(p.seq >> 24)` is the first byte. Then `>> 16`, `>> 8`, `>> 0`. A loop with `shift = 24; shift >= 0; shift -= 8` does all four. For the text, `out.insert(out.end(), p.text.begin(), p.text.end())`.
</details>

<details>
<summary>Hint: I'm stuck on decode</summary>

Start with `std::uint32_t seq = 0;`. For each of the first four bytes: `seq = (seq << 8) | bytes[i];`. The text is `bytes.subspan(kHeaderSize)`, and a `std::string` can be built from any begin/end pair.
</details>

## Part B: the sequence tracker (chapters 13, 14, 16, 10)

Read the comment at the top of [sequence_tracker.h](sequence_tracker.h) **twice** before coding. It defines the five outcomes and walks through an example.

The real design question is **TODO B0: what does the tracker need to remember?** Spend five minutes on that before writing any code. Try the worked example on paper.

<details>
<summary>Hint: what to store</summary>

You don't need a set of every number ever seen. If you know `highest_` and the set of numbers below it that are still missing, then "have I seen X?" is: `X <= highest_ && X is not in missing`. A `std::set<std::uint32_t>` keeps the missing numbers sorted for free. Add a counter for `receivedCount`.
</details>

<details>
<summary>Hint: the unsigned trap</summary>

Write `seq == highest_ + 1`, not `seq - 1 == highest_`, and `seq > highest_ + 1`, not `seq - highest_ > 1`. Chapter 10 (10.3 and 10.5) explains why subtracting unsigned numbers can surprise you. (Your version may still pass, but get in the habit.)
</details>

## Part C: make it a real chat

Once 19/19 pass, you have everything needed for the real program. This part has no tests. You'll know it works when two terminals can talk.

1. **Move your code into the project.** Copy `packet.*` and `sequence_tracker.*` into `src/`. In the root [CMakeLists.txt](../../CMakeLists.txt), make a `chatcore` library and link it into `udpchat` ([net2_program_structure.cpp](../networking/net2_program_structure.cpp) shows the four lines).
2. **Parse the flags:** `--port 9000 --peer 127.0.0.1:9001 --drop 10`. Use `parsePort` and `parsePeer` from net1 and `Options` from net2, plus argc/argv from ch20.4. Bad input prints usage and returns 1.
3. **Open the socket:** the RAII `Socket` class from net4 (and ch22), `bind` to `--port` on any address (`INADDR_ANY`). Turn `--peer` into a `sockaddr` with `resolve()` from [net3_beej_ch3_ch5_sockets.cpp](../networking/net3_beej_ch3_ch5_sockets.cpp) (getaddrinfo + RAII).
4. **The loop:** `poll` on `STDIN_FILENO` **and** the socket (net4 shows the pseudocode).
   - Keyboard ready: `std::getline`. `/quit` stops the loop. Otherwise `encode` with the next seq, roll the `DropSimulator` (net2), and `sendto` unless the roll says drop.
   - Socket ready: `recvfrom`, then `decode` (ignore bad packets), then `tracker.onReceive`. Print `[peer] #N text`, plus `[warn] gap: #4 missing`, `[warn] late: #4`, or `[warn] duplicate: #5 already received`.
5. **On quit:** print a summary, for example `received 47, missing 3: #4 #19 #33`.
6. **Try it:** run alice and bob from the main [README](../../README.md), add `--drop 30` on one side, and watch the warnings.

### Extra credit (and a preview of the VPN)

- **Replay window:** replace the `std::set` with a fixed-size sliding window, a `std::bitset<64>` plus the highest number seen. Anything older than 64 behind is rejected. This uses constant memory no matter how many packets get lost, which matters when an attacker can make you "lose" millions. It's Phase 2 of the [roadmap](../ROADMAP.md).

- Support IPv6 peers (`[::1]:9001`). `sockaddr_storage` and `to_string()` from net3 already handle the receive side.
- The sender only knows a message was lost if the receiver says so. Design a small "ACK" packet type. What has to change in the wire format?
- Write tests for your `--peer` parser in `tests/`, and update [tests/CMakeLists.txt](../../tests/CMakeLists.txt) so `ctest` runs them.
