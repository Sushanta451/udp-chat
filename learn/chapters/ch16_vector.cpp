// =============================================================================
// CHAPTER 16 — Dynamic arrays: std::vector
// learncpp.com/cpp-tutorial  (lessons 16.1 – 16.12)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   std::vector is a row of same-type values that can grow and shrink, and
//   it's the tool you'll use for almost every packet buffer you ever write.
//
// HOW THIS CONNECTS TO A VPN
//   A packet is just a row of bytes:  Bytes = std::vector<std::uint8_t>.
//   You receive bytes into a vector, read the header by INDEX, decrypt into
//   another vector, and queue vectors to send. Most packet bugs are INDEX
//   bugs: reading byte 20 of a 12-byte packet. An attacker controls the
//   packet, so an index bug is a security bug.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → a function
//   that shows it. main() runs every lesson so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch16_vector.cpp -o ch16 && ./ch16
// =============================================================================

#include <bitset>
#include <cstddef>   // std::size_t, std::ptrdiff_t
#include <cstdint>
#include <iostream>
#include <iterator>  // std::ssize
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;   // a packet, a payload, a ciphertext
using Counter = std::uint64_t;     // per-packet counter (used for replay protection)
using PeerId = std::uint32_t;

// =============================================================================
// 16.1  Introduction to containers and arrays
// =============================================================================
// IN PLAIN WORDS
//   A "container" is an object that holds many values of the same type.
//   The values inside are called "elements".
//   An "array" is a container that stores its elements side by side in
//   memory, one after another, so you can jump to any one by its number.
//   That number is the "index", and it starts at 0, not 1.
//
//   C++ has three arrays:
//     std::vector   size can change while the program runs  (this chapter)
//     std::array    size fixed when you compile             (chapter 17)
//     C-style array the old, raw kind from C                (chapter 17)
//
// WHY A VPN CARES
//   Without arrays you'd need 1500 variables to hold one packet. With one
//   you write  Bytes packet;  and you're done.
//
// RULE
//   Need a row of values? Reach for std::vector first.
void lesson_16_1()
{
    std::cout << "--- 16.1 containers and arrays ---\n";
    Bytes packet{0x45, 0x00, 0x00, 0x1c};   // 4 elements, indexes 0..3
    std::cout << "packet has " << packet.size() << " bytes; first byte (index 0) = 0x"
              << std::hex << static_cast<int>(packet[0]) << std::dec << "\n\n";
}

// =============================================================================
// 16.2  Introduction to std::vector and list constructors
// =============================================================================
// IN PLAIN WORDS
//   You can make a vector in a few ways:
//     Bytes a{1, 2, 3};   // braces + values: a holds 1, 2, 3
//     Bytes b(1500);      // parentheses + a number: 1500 bytes, all 0
//     Bytes c{};          // empty
//
//   THE TRAP: braces mean "here are the ELEMENTS".
//     Bytes buf(1500);   → 1500 elements, each 0       (a real packet buffer)
//     Bytes buf{1500};   → tries to make ONE element with value 1500.
//                          1500 doesn't fit in a byte → compile error (luckily).
//     std::vector<int> v{1500}; → ONE element, value 1500. Compiles. Silent bug.
//
//   A "list constructor" is the special constructor that takes a {list}.
//   When you use braces, C++ prefers the list constructor if it can.
//
// WHY A VPN CARES
//   You meant "a 1500-byte receive buffer" and got "a 1-byte buffer".
//   recvfrom() then reads into a buffer that's far too small.
//
// RULE
//   To give a vector a SIZE, use parentheses: Bytes buf(1500).
//   To give it VALUES, use braces: Bytes hdr{0x01, 0x00}.
void lesson_16_2()
{
    std::cout << "--- 16.2 vector + list constructors ---\n";
    constexpr std::size_t kMtu = 1500;   // max bytes in one Ethernet packet
    Bytes buf(kMtu);                     // 1500 zero bytes
    std::vector<int> oops{1500};         // ONE int, value 1500!
    // Bytes wontCompile{1500};          // ERROR: 1500 doesn't fit in a Byte (narrowing)
    std::cout << "Bytes buf(1500).size()            = " << buf.size() << "\n";
    std::cout << "std::vector<int> oops{1500}.size() = " << oops.size()
              << "  <- not what you wanted\n\n";
}

// =============================================================================
// 16.3  std::vector and the unsigned length and subscript problem
// =============================================================================
// IN PLAIN WORDS
//   v.size() returns std::size_t, which is UNSIGNED (can't be negative).
//   v[i] also wants an unsigned index. But most of your math uses int, which
//   IS signed. Mixing them causes conversion warnings and nasty bugs (ch 10).
//
//   Two ways to get the length:
//     v.size()        unsigned (std::size_t)
//     std::ssize(v)   SIGNED   (std::ptrdiff_t)   ← C++20
//
//   Two ways to index:
//     v[i]      fast, NO checking. Bad index = undefined behavior (UB).
//               "Undefined behavior" means anything can happen: a crash,
//               garbage, or quietly reading someone else's memory.
//     v.at(i)   checks the index. Bad index throws an exception (an error
//               you can catch). Slightly slower.
//
// WHY A VPN CARES
//   Packets come from strangers. Before you read header byte 11, CHECK the
//   packet has at least 12 bytes. Otherwise a 3-byte packet makes you read
//   past the end. That's how "out-of-bounds read" CVEs happen.
//
// RULE
//   Bounds-check untrusted data BEFORE you index it. Then [] is fine.
constexpr std::size_t kHeaderSize = 12;   // toy header: 1 type + 3 pad + 8 counter

bool parseType(const Bytes& packet, Byte& outType)
{
    if (packet.size() < kHeaderSize)   // check FIRST
        return false;
    outType = packet[0];   // safe now: we know index 0..11 exist
    return true;
}

void lesson_16_3()
{
    std::cout << "--- 16.3 unsigned length + subscripts ---\n";
    Bytes good(kHeaderSize, 0);
    good[0] = 4;
    Bytes evil{4, 0, 0};   // only 3 bytes: an attacker's short packet

    Byte type{};
    std::cout << std::boolalpha;
    std::cout << "good packet parsed? " << parseType(good, type) << " (type " << static_cast<int>(type)
              << ")\n";
    std::cout << "evil packet parsed? " << parseType(evil, type) << "\n";
    std::cout << "evil.size()=" << evil.size() << "  std::ssize(evil)=" << std::ssize(evil) << "\n";
    // evil[11];      // UB: reads memory you don't own. No error message!
    // evil.at(11);   // throws std::out_of_range. Safer, but checking first is better.
    std::cout << "\n";
}

// =============================================================================
// 16.4  Passing std::vector
// =============================================================================
// IN PLAIN WORDS
//   Passing a vector BY VALUE copies every element. For a 1500-byte packet,
//   that's 1500 bytes copied for nothing.
//   Pass by const reference instead: 'const Bytes&'. No copy, and the
//   function promises not to change it.
//
// WHY A VPN CARES
//   A VPN handles thousands of packets per second. Needless copies waste
//   time, and every copy of secret data is one more place it can leak from.
//
// RULE
//   Read-only vector parameter → const Bytes&.
//   Function must change it    → Bytes& (non-const reference).
std::size_t countZeroBytes(const Bytes& packet)   // no copy
{
    std::size_t zeros = 0;
    for (Byte b : packet)
        if (b == 0)
            ++zeros;
    return zeros;
}

void lesson_16_4()
{
    std::cout << "--- 16.4 passing vectors ---\n";
    Bytes packet{0x45, 0x00, 0x00, 0x1c, 0x00};
    std::cout << "zero bytes in packet: " << countZeroBytes(packet) << "\n\n";
}

// =============================================================================
// 16.5  Returning std::vector, and an introduction to move semantics
// =============================================================================
// IN PLAIN WORDS
//   Returning a vector from a function is CHEAP. You don't need tricks.
//   Why? A vector's elements live in a separate chunk of memory, and the
//   vector just holds a pointer to it. When the local vector is about to
//   die anyway, C++ "moves" it: it hands over the pointer instead of
//   copying the elements. That's "move semantics".
//
//   Analogy: you're moving house. Copying = build a new house and copy every
//   item. Moving = hand over the keys.
//
// WHY A VPN CARES
//   'Bytes encrypt(const Bytes& plain)' returning a new ciphertext is clean
//   and fast. No need for ugly "output parameter" code.
//
// RULE
//   Return vectors by value. Let move semantics do the work.
Bytes buildHeader(Byte type, Counter counter)
{
    Bytes header(kHeaderSize, 0);
    header[0] = type;
    for (std::size_t i = 0; i < 8; ++i)   // write counter, low byte first
        header[4 + i] = static_cast<Byte>(counter >> (8 * i));
    return header;   // moved out, not copied
}

void lesson_16_5()
{
    std::cout << "--- 16.5 returning vectors (move) ---\n";
    Bytes h = buildHeader(4, 258);   // 258 = 0x0102 → bytes 02 01 00 ...
    std::cout << "header size " << h.size() << ", h[4]=" << static_cast<int>(h[4])
              << ", h[5]=" << static_cast<int>(h[5]) << "\n\n";
}

// =============================================================================
// 16.6  Arrays and loops
// =============================================================================
// IN PLAIN WORDS
//   Arrays and loops go together: the loop variable walks the indexes.
//     for (std::size_t i = 0; i < v.size(); ++i)  ... v[i] ...
//   Note '<', not '<='. With '<=' you touch v[v.size()], one past the end.
//   That's an "off-by-one" bug.
//
// WHY A VPN CARES
//   Off-by-one on a packet buffer = reading or writing one byte you don't own.
//   One byte is enough for some exploits.
//
// RULE
//   Loop with 'i < size', never 'i <= size'.
void lesson_16_6()
{
    std::cout << "--- 16.6 arrays + loops ---\n";
    Bytes payload{'p', 'i', 'n', 'g'};
    unsigned sum = 0;   // toy checksum: just add the bytes. TOY, NOT secure.
    for (std::size_t i = 0; i < payload.size(); ++i)   // '<' !
        sum += payload[i];
    std::cout << "toy checksum of \"ping\" = " << sum << "\n\n";
}

// =============================================================================
// 16.7  Arrays, loops, and sign challenge solutions
// =============================================================================
// IN PLAIN WORDS
//   Looping BACKWARDS with an unsigned index is a classic trap:
//
//     for (std::size_t i = v.size() - 1; i >= 0; --i)   // INFINITE LOOP!
//
//   An unsigned number is ALWAYS >= 0, so the condition never fails.
//   When i is 0 and you do --i, it wraps to 18446744073709551615 and you
//   read far out of bounds. (Also: if v is empty, v.size() - 1 is ALREADY huge.)
//
//   Fixes (pick one):
//     a) Use a signed index: for (auto i = std::ssize(v) - 1; i >= 0; --i)
//        then index with v[static_cast<std::size_t>(i)].
//     b) Unsigned trick: for (std::size_t i = v.size(); i-- > 0; )
//        (test first, THEN decrement).
//     c) Don't use indexes at all: range-for (16.8) or reverse iterators (ch 18).
//
// WHY A VPN CARES
//   Endian conversion and trimming padding both walk backwards over bytes.
//
// RULE
//   Avoid index math when you can. When you can't, pick ONE fix and stick to it.
void lesson_16_7()
{
    std::cout << "--- 16.7 sign challenge ---\n";
    Bytes bytes{1, 2, 3};
    // for (std::size_t i = bytes.size() - 1; i >= 0; --i) {}   // never ends. Don't run!

    std::cout << "backwards (signed): ";
    for (auto i = std::ssize(bytes) - 1; i >= 0; --i)
        std::cout << static_cast<int>(bytes[static_cast<std::size_t>(i)]) << ' ';

    std::cout << "\nbackwards (i-- > 0): ";
    for (std::size_t i = bytes.size(); i-- > 0;)
        std::cout << static_cast<int>(bytes[i]) << ' ';

    Bytes empty{};
    std::cout << "\nempty.size() - 1 = " << empty.size() - 1 << "  <- why the naive loop explodes\n\n";
}

// =============================================================================
// 16.8  Range-based for loops (for-each)
// =============================================================================
// IN PLAIN WORDS
//   'for (Byte b : packet)' visits every element. No index, no off-by-one,
//   no signed/unsigned mess.
//     for (Byte b : v)          copy of each element (fine for small types)
//     for (const auto& x : v)   look at each element, no copy (big types)
//     for (auto& x : v)         CHANGE each element in place
//
// WHY A VPN CARES
//   Most byte loops just need "every byte". for-each can't go out of bounds.
//
// RULE
//   Default to range-for. Use an index only when you need the index.
void lesson_16_8()
{
    std::cout << "--- 16.8 range-based for ---\n";
    Bytes data{'h', 'i'};
    const Byte toyKey = 0x20;
    for (auto& b : data)   // TOY XOR "cipher". NOT secure. Real VPNs use ChaCha20-Poly1305.
        b = static_cast<Byte>(b ^ toyKey);
    std::cout << "after toy XOR: ";
    for (Byte b : data)
        std::cout << b;   // Byte is a char type, so it prints as a letter
    std::cout << "  (lowercase → uppercase, because 0x20 flips case)\n\n";
}

// =============================================================================
// 16.9  Array indexing and length using enumerators
// =============================================================================
// IN PLAIN WORDS
//   An enum gives names to numbers. If the names are 0, 1, 2, ... you can
//   use them as array indexes: stats[Stat::dropped] reads better than stats[2].
//   Add a last "count" enumerator. It equals the number of real entries,
//   so it's also the right array size.
//
//   We use an UNSCOPED enum inside a namespace: unscoped enums convert to
//   integers automatically (so they work as indexes), and the namespace
//   stops the names leaking everywhere.
//
// WHY A VPN CARES
//   A VPN tracks counters per category: packets received, sent, dropped,
//   replayed. One table, named slots.
//
// RULE
//   Put a 'max_stats' style count at the end. static_assert that the table
//   size matches, so adding a new stat can't silently break it.
namespace Stat
{
enum Type
{
    received,
    sent,
    dropped,
    replayed,
    max_stats,   // = 4, the count
};
}

void lesson_16_9()
{
    std::cout << "--- 16.9 indexing with enumerators ---\n";
    std::vector<Counter> stats(Stat::max_stats);   // 4 zeros
    static_assert(Stat::max_stats == 4);           // update if you add a stat!
    stats[Stat::received] += 10;
    stats[Stat::dropped] += 2;
    std::cout << "received=" << stats[Stat::received] << " dropped=" << stats[Stat::dropped]
              << " table size=" << stats.size() << "\n\n";
}

// =============================================================================
// 16.10  std::vector resizing and capacity
// =============================================================================
// IN PLAIN WORDS
//   size     = how many elements you're USING.
//   capacity = how many elements fit in the memory already grabbed.
//   When size would pass capacity, the vector "reallocates": grabs a bigger
//   block, moves everything over, frees the old block.
//
//     v.resize(n)   change the SIZE (adds zeros or chops off the end)
//     v.reserve(n)  grab memory for n elements up front; size doesn't change
//
//   DANGER: after a reallocation, any pointer or reference into the OLD
//   block is "dangling" (points at freed memory). Using it is UB.
//
// WHY A VPN CARES
//   You keep a pointer to the payload inside a buffer, then append a MAC tag
//   (a short authentication code) with push_back. The buffer reallocates.
//   Your payload pointer now points at freed memory: a use-after-free bug.
//
// RULE
//   reserve() the max size up front for packet buffers. Never hold a pointer
//   into a vector across something that can grow it.
void lesson_16_10()
{
    std::cout << "--- 16.10 resize + capacity ---\n";
    Bytes buf{};
    buf.reserve(1500);   // grab the memory once
    std::cout << "after reserve: size=" << buf.size() << " capacity>=1500? " << std::boolalpha
              << (buf.capacity() >= 1500) << "\n";
    buf.resize(20);   // 20 zero bytes now in use
    const Byte* before = buf.data();
    for (int i = 0; i < 16; ++i)
        buf.push_back(0xAA);   // fits in capacity → no reallocation
    std::cout << "size=" << buf.size() << ", pointer still valid? " << (before == buf.data())
              << "\n";
    // Without reserve(), a push_back could move the data. 'before' would dangle.
    buf.resize(4);   // shrink: capacity stays, size drops
    std::cout << "after resize(4): size=" << buf.size() << "\n\n";
}

// =============================================================================
// 16.11  std::vector and stack behavior
// =============================================================================
// IN PLAIN WORDS
//   A "stack" is last-in, first-out, like a pile of plates.
//     push_back(x)   put x on top
//     back()         look at the top
//     pop_back()     remove the top (returns nothing!)
//   emplace_back(...) builds the new element right in place (can skip a copy).
//   Calling back() or pop_back() on an EMPTY vector is UB, so check empty().
//
// WHY A VPN CARES
//   A simple outgoing queue: push packets on, pop them off to send.
//   (A real queue is first-in-first-out; std::deque does that. A stack is
//   fine for this toy.)
//
// RULE
//   Check !v.empty() before back() or pop_back().
void lesson_16_11()
{
    std::cout << "--- 16.11 stack behavior ---\n";
    std::vector<Bytes> outgoing{};
    outgoing.push_back(Bytes{'A'});
    outgoing.push_back(Bytes{'B'});
    outgoing.emplace_back(3, Byte{'C'});   // builds Bytes(3, 'C') = "CCC" in place

    while (!outgoing.empty())
    {
        const Bytes& top = outgoing.back();
        std::cout << "send " << top.size() << "-byte packet starting '" << top[0] << "'\n";
        outgoing.pop_back();   // 'top' dangles after this line; don't use it again
    }
    std::cout << "\n";
}

// =============================================================================
// 16.12  std::vector<bool>
// =============================================================================
// IN PLAIN WORDS
//   std::vector<bool> is special and weird. To save space, it packs 8 bools
//   into one byte. So it isn't a real array of bool: you can't take a bool*
//   into it, and v[i] returns a "proxy" object, not a bool&. Generic code
//   that works for every other vector can break on vector<bool>.
//
//   If the number of bits is fixed, use std::bitset<N> instead.
//
// WHY A VPN CARES — the anti-replay window
//   An attacker can record a valid encrypted packet and send it again later
//   (a "replay attack"). Encryption alone doesn't stop this: the copy is
//   still valid! So every packet carries a counter that goes up by one.
//   The receiver remembers which recent counters it already accepted, as a
//   row of bits: bit k = "counter (highest - k) already seen".
//     - counter too old (fell off the window) → drop
//     - bit already set                       → drop (replay!)
//     - otherwise                             → accept and set the bit
//   WireGuard and IPsec both do this. The window is a fixed number of bits,
//   so std::bitset is the natural fit.
//
// RULE
//   Avoid std::vector<bool>. Use std::bitset<N> for a fixed set of flags.
class ReplayWindow   // TOY version for learning. Real code: follow RFC 6479.
{
public:
    bool accept(Counter c)
    {
        if (!m_any || c > m_highest)   // newer than anything seen: slide forward
        {
            Counter shift = m_any ? c - m_highest : kBits;
            m_seen = (shift >= kBits) ? std::bitset<kBits>{} : (m_seen << shift);
            m_seen.set(0);
            m_highest = c;
            m_any = true;
            return true;
        }
        Counter age = m_highest - c;   // how far behind the newest
        if (age >= kBits)
            return false;   // too old
        if (m_seen.test(age))
            return false;   // already seen → replay
        m_seen.set(age);
        return true;
    }

private:
    static constexpr std::size_t kBits = 64;
    std::bitset<kBits> m_seen{};
    Counter m_highest{0};
    bool m_any{false};
};

void lesson_16_12()
{
    std::cout << "--- 16.12 vector<bool> vs bitset (anti-replay) ---\n";
    std::vector<bool> flags(10);
    // bool* p = &flags[0];   // ERROR: flags[0] is a proxy, not a real bool
    std::cout << "vector<bool> of 10 flags works, but it's odd inside.\n";

    ReplayWindow win{};
    std::cout << std::boolalpha;
    std::cout << "accept 1?   " << win.accept(1) << "\n";
    std::cout << "accept 3?   " << win.accept(3) << "\n";
    std::cout << "accept 2?   " << win.accept(2) << "  (late, but new)\n";
    std::cout << "accept 3?   " << win.accept(3) << "  (REPLAY → dropped)\n";
    std::cout << "accept 100? " << win.accept(100) << "\n";
    std::cout << "accept 5?   " << win.accept(5) << "  (too old → dropped)\n\n";
}

int main()
{
    lesson_16_1();
    lesson_16_2();
    lesson_16_3();
    lesson_16_4();
    lesson_16_5();
    lesson_16_6();
    lesson_16_7();
    lesson_16_8();
    lesson_16_9();
    lesson_16_10();
    lesson_16_11();
    lesson_16_12();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment 'Bytes wontCompile{1500};' in 16.2. Read the error. Then
//      change std::vector<int> oops{1500} to use () and print its size.
//   2. Write 'bool readCounter(const Bytes& pkt, Counter& out)' that reads the
//      8 counter bytes at index 4..11 (low byte first). Check the size FIRST.
//      Test it with a 12-byte and a 5-byte packet.
//   3. Add a 'decrypt_failed' entry to Stat (16.9) BEFORE max_stats. What
//      does the static_assert do? Fix it.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - What's the difference between Bytes buf(1500) and Bytes buf{1500}?
//   - Why does 'for (std::size_t i = v.size() - 1; i >= 0; --i)' never stop?
//   - Why can a push_back break a pointer you saved into the vector?
// =============================================================================
