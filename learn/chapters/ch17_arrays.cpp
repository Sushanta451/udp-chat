// =============================================================================
// CHAPTER 17 — Fixed-size arrays: std::array and C-style arrays
// learncpp.com/cpp-tutorial  (lessons 17.1 – 17.13)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   When the size of an array is known while compiling (a 32-byte key, a
//   12-byte nonce), use std::array; when the OS hands you a raw C-style
//   array, know exactly how it can bite you.
//
// HOW THIS CONNECTS TO A VPN
//   Crypto sizes never change: a ChaCha20 key is ALWAYS 32 bytes, a nonce
//   ALWAYS 12. Putting that size in the TYPE means the compiler catches
//   "wrong-size key" for you. Meanwhile the socket API (inet_ntop, sockaddr)
//   is written in C and uses raw char arrays and pointers. Most classic
//   buffer-overflow bugs live right there.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → a function
//   that shows it. main() runs every lesson so you can see the output.
//
// BUILD + RUN  (macOS/Linux only: uses the socket headers)
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch17_arrays.cpp -o ch17 && ./ch17
// =============================================================================

#include <arpa/inet.h>    // inet_ntop, inet_pton
#include <netinet/in.h>   // in_addr, INET_ADDRSTRLEN, INET6_ADDRSTRLEN
#include <sys/socket.h>   // AF_INET

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>         // std::snprintf
#include <cstring>        // std::strlen
#include <functional>     // std::reference_wrapper, std::ref
#include <iostream>
#include <stdexcept>      // std::out_of_range
#include <string_view>
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// =============================================================================
// 17.1  Introduction to std::array
// =============================================================================
// IN PLAIN WORDS
//   std::array<Type, N> is a fixed row of N values. N must be known when
//   compiling (a "constexpr" value). It can't grow or shrink.
//   It can even be constexpr itself, which std::vector can't.
//
// WHY A VPN CARES
//   'using Key = std::array<Byte, 32>;' means a Key is 32 bytes, always.
//   A function taking 'const Key&' can't be handed a 16-byte key by mistake.
//
// RULE
//   Size known at compile time → std::array. Size changes → std::vector.
using Key = std::array<Byte, 32>;     // ChaCha20 key size
using Nonce = std::array<Byte, 12>;   // ChaCha20-Poly1305 nonce size
// A "nonce" = "number used once". Reusing one with the same key breaks the crypto.

void lesson_17_1()
{
    std::cout << "--- 17.1 std::array ---\n";
    Key key{};       // 32 zero bytes. (Real keys come from a crypto RNG, never zeros!)
    Nonce nonce{};   // 12 zero bytes
    constexpr std::array<int, 3> kPorts{51820, 51821, 51822};   // constexpr works
    std::cout << "key " << key.size() << " bytes, nonce " << nonce.size()
              << " bytes, first port " << kPorts[0] << "\n\n";
}

// =============================================================================
// 17.2  std::array length and indexing
// =============================================================================
// IN PLAIN WORDS
//   a.size() gives the length (unsigned std::size_t, same issue as ch 16).
//   Three ways to index:
//     a[i]          no check. Bad index = undefined behavior (UB).
//     a.at(i)       checks while RUNNING. Bad index throws std::out_of_range.
//     std::get<i>(a) checks while COMPILING. i must be a constant.
//
// WHY A VPN CARES
//   Index comes from a packet (untrusted)? Check it, or use .at().
//   Index is a fixed constant? std::get catches mistakes before you run.
//
// RULE
//   Constant index → std::get. Untrusted index → check first (or .at()).
void lesson_17_2()
{
    std::cout << "--- 17.2 length + indexing ---\n";
    Nonce nonce{};
    std::get<11>(nonce) = 1;        // OK: 11 < 12
    // std::get<12>(nonce) = 1;     // COMPILE ERROR: index out of range
    std::size_t fromPacket = 40;    // pretend an attacker sent this index
    try
    {
        nonce.at(fromPacket) = 1;   // throws instead of corrupting memory
    }
    catch (const std::out_of_range&)
    {
        std::cout << "at(40) on a 12-byte nonce → out_of_range caught\n";
    }
    std::cout << "last nonce byte = " << static_cast<int>(nonce[11]) << "\n\n";
}

// =============================================================================
// 17.3  Passing and returning std::array
// =============================================================================
// IN PLAIN WORDS
//   Pass by const reference (no copy): 'void f(const Key& k)'.
//   The SIZE is part of the type, so std::array<Byte, 16> is a different
//   type from std::array<Byte, 32>. Wrong size → compile error.
//   Returning a std::array by value is fine for small arrays. Note: unlike a
//   vector, a std::array's elements live INSIDE it, so a move still copies
//   every element. 32 bytes is nothing; 1 MB would be a lot.
//
// WHY A VPN CARES
//   The type system enforces key sizes. One less thing to get wrong.
//
// RULE
//   const& in, return by value if small.
Nonce makeNonce(Counter counter)   // nonce built from the packet counter
{
    Nonce n{};   // first 4 bytes zero, last 8 = counter (low byte first)
    for (std::size_t i = 0; i < 8; ++i)
        n[4 + i] = static_cast<Byte>(counter >> (8 * i));
    return n;
}

bool isAllZero(const Key& key)   // a zero key = "not set yet"
{
    for (Byte b : key)
        if (b != 0)
            return false;
    return true;
}

void lesson_17_3()
{
    std::cout << "--- 17.3 passing + returning ---\n";
    Nonce n = makeNonce(7);
    Key key{};
    // std::array<Byte, 16> shortKey{};  isAllZero(shortKey);   // COMPILE ERROR: wrong type
    std::cout << "nonce[4]=" << static_cast<int>(n[4]) << ", key unset? " << std::boolalpha
              << isAllZero(key) << "\n\n";
}

// =============================================================================
// 17.4  std::array of class types, and brace elision
// =============================================================================
// IN PLAIN WORDS
//   An array can hold structs. To initialize, each struct gets its own {}.
//   std::array is really a struct with a C-style array inside, so the
//   "full" form needs an extra outer pair of braces:
//       std::array<Peer, 2> t{{ {1, ...}, {2, ...} }};   // always works
//   "Brace elision" = C++ letting you leave out braces when it's not
//   ambiguous. For arrays of structs it often ISN'T allowed with the short
//   form, so if you get a strange error, add the extra braces.
//
// WHY A VPN CARES
//   A small fixed peer table: who you talk to, on which port.
//
// RULE
//   Arrays of structs: use double outer braces {{ ... }}.
struct Peer
{
    PeerId id{};
    std::uint16_t port{};
    int latencyMs{};
};

void lesson_17_4()
{
    std::cout << "--- 17.4 array of structs + brace elision ---\n";
    std::array<Peer, 3> peers{{
        {1, 51820, 40},
        {2, 51821, 15},
        {3, 51822, 90},
    }};
    // std::array<Peer, 3> bad{ {1, 51820, 40}, {2, 51821, 15}, {3, 51822, 90} };   // ERROR
    for (const auto& p : peers)
        std::cout << "peer " << p.id << " port " << p.port << " latency " << p.latencyMs
                  << "ms\n";
    std::cout << "\n";
}

// =============================================================================
// 17.5  Arrays of references via std::reference_wrapper
// =============================================================================
// IN PLAIN WORDS
//   You CAN'T make an array of references (Peer& isn't an object you can store).
//   std::reference_wrapper<Peer> is a small object that acts like a reference
//   and CAN go in an array or vector. std::ref(x) makes one.
//   Call .get() to reach the real object.
//
// WHY A VPN CARES
//   "Active peers" is a subset of the peer table. Store references, not
//   copies, so updating an active peer updates the real table.
//
// RULE
//   Need a list that points at existing objects? reference_wrapper.
//   The real objects must outlive the list.
void lesson_17_5()
{
    std::cout << "--- 17.5 reference_wrapper ---\n";
    std::array<Peer, 3> table{{{1, 51820, 40}, {2, 51821, 15}, {3, 51822, 90}}};
    // std::array<Peer&, 2> active{...};   // COMPILE ERROR: no arrays of references
    std::array<std::reference_wrapper<Peer>, 2> active{std::ref(table[0]), std::ref(table[2])};
    active[1].get().latencyMs = 25;   // changes table[2], not a copy
    std::cout << "table[2].latencyMs is now " << table[2].latencyMs << "\n\n";
}

// =============================================================================
// 17.6  std::array and enumerations
// =============================================================================
// IN PLAIN WORDS
//   Same trick as 16.9: enum values as indexes, a 'max_' count at the end
//   as the size. With std::array the size check can be a static_assert on
//   the array itself.
//   Bonus: an array of names lets you print an enum.
//
// WHY A VPN CARES
//   One counter per message type, and readable logs ("data" not "4").
//
// RULE
//   static_assert(std::size(names) == max_count) so they can't drift apart.
namespace MessageType
{
enum Type
{
    handshake_init,
    handshake_resp,
    cookie,
    data,
    max_types,
};
}

constexpr std::array<std::string_view, MessageType::max_types> kTypeNames{
    "handshake_init", "handshake_resp", "cookie", "data"};
static_assert(kTypeNames.size() == MessageType::max_types);

void lesson_17_6()
{
    std::cout << "--- 17.6 std::array + enums ---\n";
    std::array<Counter, MessageType::max_types> perType{};
    perType[MessageType::data] += 3;
    perType[MessageType::handshake_init] += 1;
    for (std::size_t t = 0; t < perType.size(); ++t)
        std::cout << kTypeNames[t] << ": " << perType[t] << "\n";
    std::cout << "\n";
}

// =============================================================================
// 17.7  Introduction to C-style arrays
// =============================================================================
// IN PLAIN WORDS
//   The old array from C:  char ip[16];
//   No .size(), no .at(), can't be copied with =, can't be returned.
//   Use std::size(arr) to get the length (works only where the array is
//   still a real array, see 17.8).
//
// WHY A VPN CARES
//   The socket API is C. inet_ntop wants a char buffer, and the headers give
//   you constants for how big it must be:
//     INET_ADDRSTRLEN  = 16  (fits "255.255.255.255" + the end marker)
//     INET6_ADDRSTRLEN = 46  (fits the longest IPv6 text)
//
// RULE
//   Use C-style arrays only to talk to C APIs. Size them with the API's
//   constant, never a guess.
void lesson_17_7()
{
    std::cout << "--- 17.7 C-style arrays ---\n";
    in_addr addr{};
    inet_pton(AF_INET, "10.0.0.2", &addr);   // text → binary address
    char ip[INET6_ADDRSTRLEN]{};             // big enough for IPv4 OR IPv6
    if (inet_ntop(AF_INET, &addr, ip, sizeof(ip)) != nullptr)   // binary → text
        std::cout << "inet_ntop wrote \"" << ip << "\" into a " << std::size(ip)
                  << "-char buffer\n\n";
}

// =============================================================================
// 17.8  C-style array decay
// =============================================================================
// IN PLAIN WORDS
//   Pass a C-style array to a function and it "decays": it turns into a
//   pointer to its first element. The SIZE IS LOST. Inside the function,
//   sizeof gives the size of a pointer (8), not the array.
//
// WHY A VPN CARES
//   This is the classic buffer overflow: a function gets 'char* buf', has no
//   idea how big buf is, and writes too much. That's why every C socket
//   function also takes a length: recvfrom(sock, buf, LEN, ...).
//
// RULE
//   When a C-style array must go to a function, ALWAYS pass its length too.
//   (Better: std::array or std::span, which keep the size.)
void decayed(const char* buf)   // a 'const char buf[46]' parameter would ALSO just be a pointer
{
    std::cout << "inside function: sizeof(buf) = " << sizeof(buf) << " (just a pointer!)\n";
}

void lesson_17_8()
{
    std::cout << "--- 17.8 array decay ---\n";
    char ip[INET6_ADDRSTRLEN]{};
    std::cout << "in caller:       sizeof(ip)  = " << sizeof(ip) << "\n";
    decayed(ip);
    std::cout << "\n";
}

// =============================================================================
// 17.9  Pointer arithmetic and subscripting
// =============================================================================
// IN PLAIN WORDS
//   If p points at element 0, then p + 3 points at element 3.
//   In fact p[3] is just a shorter way to write *(p + 3).
//   Nothing checks that p + 3 is still inside the array.
//
// WHY A VPN CARES
//   C-style parsers walk a header with a pointer: read 1 byte, move 1;
//   read 2, move 2. Move past the end and you read someone else's memory.
//
// RULE
//   Compute 'end' once, and check 'p + n <= end' before every read.
void lesson_17_9()
{
    std::cout << "--- 17.9 pointer arithmetic ---\n";
    const Byte packet[]{4, 0, 0, 0, 0x2A, 0, 0, 0};   // type=4, then counter
    const Byte* p = packet;
    const Byte* end = packet + std::size(packet);   // one PAST the last byte

    Byte type = *p;   // same as p[0]
    p += 4;           // skip type + 3 padding bytes
    if (p + 1 <= end) // room for 1 more byte?
        std::cout << "type=" << static_cast<int>(type) << " first counter byte="
                  << static_cast<int>(*p) << " (same as packet[4]=" << static_cast<int>(packet[4])
                  << ")\n";
    std::cout << "bytes left: " << (end - p) << "\n\n";
}

// =============================================================================
// 17.10  C-style strings
// =============================================================================
// IN PLAIN WORDS
//   A C-style string is a char array ending in '\0' (the "NUL terminator",
//   a zero byte). That zero is the ONLY way C knows where the text stops.
//   "hi" takes 3 chars: 'h', 'i', '\0'.
//
//   strcpy(dst, src) copies until it finds '\0'. It never checks dst's size.
//   If src is longer, it writes past the end: buffer overflow.
//
//   Safer: std::snprintf(dst, sizeof(dst), "%s", src) — never writes more
//   than sizeof(dst), and ALWAYS adds the '\0'. It cuts long text short.
//
// WHY A VPN CARES
//   A peer's hostname arrives from the network. strcpy it into a fixed
//   buffer and an attacker overwrites your stack. Forgetting the '\0'
//   makes the next printf read past the end, leaking memory.
//
// RULE
//   Never strcpy/strcat untrusted text. Always leave room for '\0'.
//   In C++ code, prefer std::string; use char arrays only at the C boundary.
void lesson_17_10()
{
    std::cout << "--- 17.10 C-style strings ---\n";
    const char* fromNetwork = "a-very-long-hostname-from-an-attacker.example";
    char name[16]{};
    // std::strcpy(name, fromNetwork);   // OVERFLOW: 45 chars into 16. Don't!
    std::snprintf(name, sizeof(name), "%s", fromNetwork);   // safe: cut to 15 + '\0'
    std::cout << "safe copy: \"" << name << "\" (strlen " << std::strlen(name) << ")\n\n";
}

// =============================================================================
// 17.11  C-style string symbolic constants
// =============================================================================
// IN PLAIN WORDS
//   Two ways to name a constant C-string:
//     constexpr char kA[] = "wg0";         an array (its own copy of the text)
//     constexpr const char* kB = "wg0";    a pointer to a string literal
//   Both are fine. In modern C++, std::string_view is usually nicer.
//   Never try to modify a string literal: it's read-only memory.
//
// WHY A VPN CARES
//   Interface names, default endpoints, protocol labels.
//
// RULE
//   For named string constants, prefer constexpr std::string_view.
constexpr char kIfName[] = "wg0";
constexpr const char* kDefaultHost = "vpn.example.com";
constexpr std::string_view kProtocolLabel{"toy-vpn-v1"};

void lesson_17_11()
{
    std::cout << "--- 17.11 string constants ---\n";
    std::cout << kIfName << " (" << sizeof(kIfName) << " bytes incl. '\\0'), " << kDefaultHost
              << ", " << kProtocolLabel << " (" << kProtocolLabel.size() << " chars)\n\n";
}

// =============================================================================
// 17.12  Multidimensional C-style arrays
// =============================================================================
// IN PLAIN WORDS
//   An array of arrays: int grid[4][4] = 4 rows, each of 4 ints.
//   grid[row][col]. In memory it's 16 ints in a row, row 0 first.
//
// WHY A VPN CARES
//   Intuition only: AES (a real cipher) thinks of 16 bytes as a 4x4 grid
//   it shuffles around. We just PRINT a grid here. No crypto. Never write
//   your own cipher; real VPNs use vetted libraries like libsodium.
//
// RULE
//   Loop rows on the outside, columns on the inside (matches memory order).
void lesson_17_12()
{
    std::cout << "--- 17.12 2-D C-style array ---\n";
    Byte grid[4][4]{};
    Byte value = 0;
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t col = 0; col < 4; ++col)
            grid[row][col] = value++;
    for (const auto& row : grid)
    {
        for (Byte b : row)
            std::cout << static_cast<int>(b) << '\t';
        std::cout << '\n';
    }
    std::cout << "\n";
}

// =============================================================================
// 17.13  Multidimensional std::array
// =============================================================================
// IN PLAIN WORDS
//   std::array<std::array<T, Cols>, Rows>. Clunky to write, so give it an alias.
//   Unlike C-style, it keeps its size, can be copied, passed, and returned.
//
// WHY A VPN CARES
//   A tiny routing table: each row is {network, prefix length, peer}.
//   (Real routing uses smarter structures; this is just the idea.)
//
// RULE
//   Use an alias for 2-D std::array. Prefer it over a 2-D C-style array.
using RouteTable = std::array<std::array<std::uint32_t, 3>, 2>;   // 2 rows x 3 columns

void lesson_17_13()
{
    std::cout << "--- 17.13 2-D std::array ---\n";
    // columns: last octet of 10.0.0.x network, prefix length, peer id
    RouteTable routes{{
        {{1, 24, 1}},
        {{2, 32, 3}},
    }};
    for (const auto& r : routes)
        std::cout << "10.0.0." << r[0] << "/" << r[1] << " → peer " << r[2] << "\n";
    std::cout << "rows=" << routes.size() << " cols=" << routes[0].size() << "\n\n";
}

int main()
{
    lesson_17_1();
    lesson_17_2();
    lesson_17_3();
    lesson_17_4();
    lesson_17_5();
    lesson_17_6();
    lesson_17_7();
    lesson_17_8();
    lesson_17_9();
    lesson_17_10();
    lesson_17_11();
    lesson_17_12();
    lesson_17_13();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment 'std::get<12>(nonce) = 1;' in 17.2. Read the error. Why is
//      catching this at compile time better than .at()?
//   2. Write 'bool peerIp(const in_addr& a, char* out, std::size_t outLen)'
//      that wraps inet_ntop and returns false if it fails. Call it with an
//      8-char buffer. What happens? (Check inet_ntop's return value.)
//   3. In 17.10, uncomment the strcpy line and build with
//      -fsanitize=address. Read the "stack-buffer-overflow" report.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why is std::array<Byte, 32> safer than passing a Byte* for a key?
//   - What does "array decay" lose, and why does that cause overflows?
//   - Why must a C-string buffer be one bigger than the longest text?
// =============================================================================
