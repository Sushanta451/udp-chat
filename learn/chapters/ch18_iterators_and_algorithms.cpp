// =============================================================================
// CHAPTER 18 — Iterators and Algorithms
// learncpp.com/cpp-tutorial  (lessons 18.1 – 18.4)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   Don't hand-write loops for searching, counting, sorting, and copying:
//   the standard library already has tested versions, and iterators are
//   how you point them at your data.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN constantly asks questions about lists: "which peer has id 7?",
//   "how many packets were dropped?", "which peer is fastest?". It also
//   compares secret values (authentication tags). That last one hides a
//   trap: a normal compare can leak secrets through TIMING (18.3).
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → a function
//   that shows it. main() runs every lesson so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -O2 -Wall -Wextra -Wconversion ch18_iterators_and_algorithms.cpp -o ch18 && ./ch18
//   (-O2 = turn on optimizations. Timing numbers without it are meaningless.)
// =============================================================================

#include <algorithm>   // std::sort, std::find_if, std::count_if, std::copy, std::fill, std::equal
#include <chrono>      // std::chrono::steady_clock
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>     // std::swap
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

struct Peer
{
    PeerId id{};
    int latencyMs{};
    bool online{};
};

void printPeers(const std::vector<Peer>& peers)
{
    for (const auto& p : peers)
        std::cout << "  peer " << p.id << ": " << p.latencyMs << "ms\n";
}

// =============================================================================
// 18.1  Sorting an array using selection sort
// =============================================================================
// IN PLAIN WORDS
//   "Sorting" = putting things in order. Selection sort is the simplest way:
//     1. Look through everything, find the SMALLEST.
//     2. Swap it into position 0.
//     3. Repeat for position 1, 2, ... (ignoring the already-sorted front).
//   It's slow on big lists (roughly n*n steps), but easy to understand.
//
// WHY A VPN CARES
//   When a client can connect to several servers, it picks the one with the
//   lowest latency (round-trip time). Sorting by latency gives you that.
//
// RULE
//   Write selection sort once to understand it. Then use std::sort forever.
void selectionSortByLatency(std::vector<Peer>& peers)
{
    for (std::size_t start = 0; start < peers.size(); ++start)
    {
        std::size_t smallest = start;
        for (std::size_t i = start + 1; i < peers.size(); ++i)
            if (peers[i].latencyMs < peers[smallest].latencyMs)
                smallest = i;
        std::swap(peers[start], peers[smallest]);
    }
}

void lesson_18_1()
{
    std::cout << "--- 18.1 selection sort ---\n";
    std::vector<Peer> peers{{1, 40, true}, {2, 15, true}, {3, 90, false}, {4, 22, true}};
    selectionSortByLatency(peers);
    printPeers(peers);
    std::cout << "fastest server: peer " << peers.front().id << "\n\n";
}

// =============================================================================
// 18.2  Introduction to iterators
// =============================================================================
// IN PLAIN WORDS
//   An "iterator" is an object that points at one element of a container
//   and knows how to step to the next one. Think of it as a bookmark.
//     v.begin()   points at the first element
//     v.end()     points ONE PAST the last element (a stop sign, not data!)
//     *it         the element it points at
//     ++it        move to the next element
//   Every container has them, so one style of loop works for vector,
//   array, list, ... Range-for (ch 16) is built on begin() and end().
//
//   "Iterator invalidation": some operations make old iterators stop being
//   valid. For a vector, anything that can reallocate (push_back, insert,
//   resize) can invalidate ALL iterators into it. Using one after that is UB.
//
// WHY A VPN CARES
//   You walk a packet buffer with an iterator while also appending to it
//   → your iterator may point at freed memory.
//
// RULE
//   Never dereference end(). Don't keep iterators across a push_back.
void lesson_18_2()
{
    std::cout << "--- 18.2 iterators ---\n";
    Bytes packet{0x04, 0x00, 0x00, 0x00, 0xDE, 0xAD};
    std::cout << "bytes via iterator:";
    for (auto it = packet.begin(); it != packet.end(); ++it)
        std::cout << ' ' << static_cast<int>(*it);
    std::cout << "\nbytes from begin to end: " << (packet.end() - packet.begin()) << "\n";

    // auto first = packet.begin();
    // packet.push_back(0xFF);      // may reallocate...
    // std::cout << *first;         // ...UB: 'first' may now dangle
    std::cout << "\n";
}

// =============================================================================
// 18.3  Introduction to standard library algorithms
// =============================================================================
// IN PLAIN WORDS
//   <algorithm> has ready-made loops. You give them a range (begin, end)
//   and sometimes a small function saying what to look for. That small
//   function is often a "lambda": an unnamed function written inline,
//   like  [](const Peer& p) { return p.online; }   (more in ch 20).
//
//     std::find_if   first element matching a test (or end() if none)
//     std::count_if  how many match
//     std::sort      sort (fast: about n*log(n) steps)
//     std::copy      copy one range into another
//     std::fill      set every element to one value
//
// SECURITY: COMPARING SECRETS — "TIMING ATTACKS"
//   Every encrypted VPN packet carries a "tag" (a MAC, Message
//   Authentication Code): 16 bytes that prove the packet wasn't tampered
//   with. The receiver computes the expected tag and compares.
//
//   std::equal or memcmp STOP at the first wrong byte. So a guess with the
//   first byte right takes a tiny bit LONGER to reject than one with it
//   wrong. An attacker who measures that time can guess the tag one byte
//   at a time. That's a "timing attack" (a "side channel": leaking info
//   through HOW LONG something takes rather than what it returns).
//
//   Fix: a "constant-time" compare. Look at EVERY byte, no matter what,
//   and combine all the differences with OR. Only check the result at the end.
//
// RULE
//   Prefer algorithms over hand-written loops. Compare secrets in constant
//   time. In real code use the crypto library's compare (e.g. libsodium's
//   sodium_memcmp), because compilers can "optimize" hand-written versions.
bool constantTimeEqual(const Bytes& a, const Bytes& b)
{
    if (a.size() != b.size())   // tag LENGTH isn't secret, so this early exit is OK
        return false;
    Byte diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff = static_cast<Byte>(diff | (a[i] ^ b[i]));   // never stops early
    return diff == 0;
}

void lesson_18_3()
{
    std::cout << "--- 18.3 standard algorithms ---\n";
    std::vector<Peer> peers{{1, 40, true}, {2, 15, true}, {3, 90, false}, {4, 22, true}};

    auto found = std::find_if(peers.begin(), peers.end(),
                              [](const Peer& p) { return p.id == 3; });
    if (found != peers.end())   // ALWAYS check before using *found
        std::cout << "find_if id 3 → latency " << found->latencyMs << "ms\n";

    auto online = std::count_if(peers.begin(), peers.end(),
                                [](const Peer& p) { return p.online; });
    std::cout << "count_if online = " << online << "\n";

    std::sort(peers.begin(), peers.end(),
              [](const Peer& a, const Peer& b) { return a.latencyMs < b.latencyMs; });
    std::cout << "std::sort by latency (same result as 18.1, one line):\n";
    printPeers(peers);

    Bytes payload{'p', 'i', 'n', 'g'};
    Bytes packet(8, 0);   // 4-byte header + 4-byte payload
    std::copy(payload.begin(), payload.end(), packet.begin() + 4);
    std::cout << "after copy, packet[4] = '" << packet[4] << "'\n";

    std::fill(packet.begin(), packet.end(), Byte{0});   // wipe the buffer
    std::cout << "after fill, packet[4] = " << static_cast<int>(packet[4]) << "\n";
    // (To wipe real SECRETS, use sodium_memzero: a compiler may skip a fill
    //  on memory that's never read again.)

    Bytes expectedTag{0x11, 0x22, 0x33, 0x44};   // 4 bytes to keep it short (real: 16)
    Bytes forgedTag{0x11, 0x22, 0x33, 0x45};
    std::cout << std::boolalpha;
    std::cout << "std::equal (LEAKS timing): "
              << std::equal(expectedTag.begin(), expectedTag.end(), forgedTag.begin(),
                            forgedTag.end())
              << "\n";
    std::cout << "constantTimeEqual:         " << constantTimeEqual(expectedTag, forgedTag)
              << "\n";
    std::cout << "constantTimeEqual (same):  " << constantTimeEqual(expectedTag, expectedTag)
              << "\n\n";
}

// =============================================================================
// 18.4  Timing your code
// =============================================================================
// IN PLAIN WORDS
//   To know if code is fast, MEASURE it. std::chrono::steady_clock is a
//   clock that only goes forward (the wall clock can jump if the system
//   time changes). Pattern:
//     auto start = steady_clock::now();
//     ...work...
//     auto elapsed = steady_clock::now() - start;
//
//   Tips:
//     - Build with -O2. Unoptimized (debug) builds can be 10x slower.
//     - Run several times; the first run is often slower.
//     - USE the result (print it), or the optimizer may delete the work.
//
// WHY A VPN CARES
//   A VPN's job is throughput: how many packets per second you can handle.
//   Guessing about speed is usually wrong. Measure.
//
// RULE
//   steady_clock for timing. -O2 for numbers you believe.
std::uint32_t toyChecksum(const Bytes& packet)   // TOY, NOT secure. Just busy work.
{
    std::uint32_t sum = 0;
    for (Byte b : packet)
        sum = sum * 31 + b;
    return sum;
}

void lesson_18_4()
{
    std::cout << "--- 18.4 timing ---\n";
    constexpr int kPackets = 1'000'000;
    Bytes packet(64, 0xAB);   // a small 64-byte packet

    auto start = std::chrono::steady_clock::now();
    std::uint32_t total = 0;
    for (int i = 0; i < kPackets; ++i)
    {
        packet[0] = static_cast<Byte>(i);   // change it so each "packet" differs
        total ^= toyChecksum(packet);
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "checksummed " << kPackets << " packets in " << ms << " ms"
              << " (result " << total << ", printed so the work isn't optimized away)\n";
    std::cout << "your number will differ: depends on machine and -O level\n\n";
}

int main()
{
    lesson_18_1();
    lesson_18_2();
    lesson_18_3();
    lesson_18_4();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Change selectionSortByLatency to sort SLOWEST first. Then do the same
//      with std::sort by changing only the lambda.
//   2. Use std::find_if to find the first OFFLINE peer. Print its id, or
//      "none" if find_if returns end().
//   3. Build 18.4 without -O2, then with -O2. Write down both times.
//      How big is the difference?
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why is it a bug to do *v.end()?
//   - Why does std::equal on an auth tag leak information, and how does
//     OR-ing XORs fix it?
//   - Why use steady_clock instead of the system (wall) clock for timing?
// =============================================================================
