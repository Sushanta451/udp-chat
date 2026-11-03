// =============================================================================
// CHAPTER F — Constexpr Functions
// learncpp.com/cpp-tutorial  (lessons F.1 – F.4)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   A constexpr function is a normal function that the compiler is ALSO
//   allowed to run while compiling, so its answer gets baked into the program
//   and can be checked before the program ever runs.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN is full of numbers that never change: header sizes, the MTU, the
//   tag size, a protocol "magic number", lookup tables for checksums.
//   If you compute them at COMPILE time:
//     - a wrong value is a compile error (static_assert), not a dropped packet,
//     - the work costs zero time while packets are flowing.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → example code.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion chF_constexpr_functions.cpp -o chF && ./chF
// =============================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// A word you'll see everywhere below:
//   "constant expression" = an expression the compiler can fully work out
//   while compiling. 2 + 3 is one. A value typed in by the user is not.

// =============================================================================
// F.1  Constexpr functions
// =============================================================================
// IN PLAIN WORDS
//   Put 'constexpr' in front of a function:
//     constexpr int twice(int x) { return 2 * x; }
//   Now if you call it with compile-time values, in a place that NEEDS a
//   compile-time value, the compiler runs it during compilation.
//
//   "static_assert(condition, "message")" = a check done by the COMPILER.
//   If the condition is false, the program doesn't compile.
//
// WHY A VPN CARES
//   The tunnel MTU is "link MTU minus all the headers we add". Get it wrong
//   and big packets get fragmented or dropped. With constexpr + static_assert
//   the math is checked every single time you build.
//
//   Our packet on the wire (WireGuard-style):
//     [ IPv6 header 40 ][ UDP header 8 ][ our header 16 ][ encrypted data ][ tag 16 ]
//   "tag" = the 16-byte authentication code the cipher adds (Poly1305).
//
// RULE
//   If a function's answer only depends on its inputs and could be known at
//   compile time, make it constexpr.
constexpr std::size_t kIpv6Header = 40;
constexpr std::size_t kUdpHeader = 8;
constexpr std::size_t kOurHeader = 16;   // type 4 + receiver id 4 + counter 8
constexpr std::size_t kTagSize = 16;     // Poly1305 tag

constexpr std::size_t tunnelOverhead()
{
    return kIpv6Header + kUdpHeader + kOurHeader + kTagSize;
}

constexpr std::size_t tunnelMtu(std::size_t linkMtu)
{
    return linkMtu - tunnelOverhead();
}

// These run INSIDE THE COMPILER. If one is false, the build fails.
static_assert(tunnelOverhead() == 80, "overhead math is wrong");
static_assert(tunnelMtu(1500) == 1420, "should match WireGuard's default MTU");

void lesson_F_1()
{
    std::cout << "--- F.1 constexpr functions ---\n";
    constexpr std::size_t mtu = tunnelMtu(1500);   // computed while compiling
    std::cout << "  overhead = " << tunnelOverhead() << " bytes\n";
    std::cout << "  tunnel MTU for a 1500-byte link = " << mtu << "\n\n";   // 1420
}

// =============================================================================
// F.2  Constexpr functions (part 2): compile time OR run time
// =============================================================================
// IN PLAIN WORDS
//   'constexpr' on a function means "CAN run at compile time", not "MUST".
//   The same function also works at run time with normal variables.
//
//   When is it GUARANTEED to run at compile time? When the result is used
//   somewhere that REQUIRES a constant expression:
//     - initializing a constexpr variable
//     - a static_assert
//     - an array size, e.g. std::array<Byte, N>
//     - a non-type template argument (chapter 11.9)
//   Anywhere else (e.g. just printing the result) the compiler MAY do it at
//   compile time, or may not. That's its choice.
//
//   Two more facts from this lesson:
//   - PARAMETERS of a constexpr function are NOT constexpr. Inside the
//     function, 'n' is just a normal variable, because the same function must
//     also work at run time. So no static_assert(n > 0) inside it (see
//     paddedSize in F.3).
//   - Constexpr functions are implicitly "inline". That means you can (and
//     should) put the whole function in a header: every .cpp that includes it
//     gets the same copy, and the linker doesn't complain. The compiler needs
//     the full body anyway, to run it at compile time.
//
// WHY A VPN CARES
//   One IPv4 checksum function can be TESTED at compile time with
//   static_assert, and USED at run time on real packets. One function, two jobs.
//
//   "Checksum" = a small number computed from data to catch ACCIDENTAL damage
//   (a flipped bit on the wire). It is NOT security: an attacker just
//   recomputes it. Real VPNs protect packets with an authentication tag
//   from a vetted library (e.g. ChaCha20-Poly1305 in libsodium / WireGuard).
//
// RULE
//   Need a guaranteed compile-time result? Store it in a constexpr variable
//   or check it in static_assert. Put constexpr functions in headers.
using Ipv4Header = std::array<Byte, 20>;

// The standard IPv4 header checksum (RFC 791): add up 16-bit words, fold the
// carries back in, flip all the bits.
constexpr std::uint16_t ipv4Checksum(const Ipv4Header& h)
{
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < h.size(); i += 2)
        sum += static_cast<std::uint32_t>((h[i] << 8) | h[i + 1]);
    while (sum > 0xFFFF)
        sum = (sum & 0xFFFF) + (sum >> 16);   // fold carries back in
    return static_cast<std::uint16_t>(~sum);
}

// A real example header, with the checksum field (bytes 10-11) set to 0.
// From 192.168.0.1 to 192.168.0.199, UDP. Its correct checksum is 0xB861.
constexpr Ipv4Header kSampleHeader{0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40,
                                   0x00, 0x40, 0x11, 0x00, 0x00, 0xC0, 0xA8,
                                   0x00, 0x01, 0xC0, 0xA8, 0x00, 0xC7};

// A unit test that runs in the compiler:
static_assert(ipv4Checksum(kSampleHeader) == 0xB861, "checksum function is broken");

void lesson_F_2()
{
    std::cout << "--- F.2 compile time OR run time ---\n";

    // Run time: pretend this header just arrived from the network.
    Ipv4Header received = kSampleHeader;   // NOT constexpr: a normal variable
    received[10] = 0xB8;                   // sender filled in the checksum
    received[11] = 0x61;
    // Trick: checksum over a header that INCLUDES a correct checksum gives 0.
    std::cout << "  checksum of good packet = " << ipv4Checksum(received)
              << " (0 means OK)\n";

    received[8] = 0x3F;   // TTL changed on the wire, checksum not updated
    std::cout << "  checksum of damaged packet = " << ipv4Checksum(received)
              << " (not 0 → drop it)\n";

    // Same function with a run-time input:
    std::size_t linkMtu = 9000;   // e.g. read from the network card (jumbo frames)
    std::cout << "  tunnelMtu(9000) at run time = " << tunnelMtu(linkMtu) << "\n\n";   // 8920
}

// =============================================================================
// F.3  Constexpr functions (part 3) and consteval
// =============================================================================
// IN PLAIN WORDS
//   1) Reminder from F.2: parameters are NOT constexpr, even in a function
//      that only ever runs at compile time.
//
//   2) 'consteval' = "this function MUST run at compile time."
//      Calling it with a run-time value is a compile error.
//      A consteval function is called an "immediate function".
//
//   3) Trick: a tiny consteval helper can force ANY constexpr call to happen
//      at compile time, without making a named constexpr variable.
//
//   (Advanced, for later: std::is_constant_evaluated() lets a constexpr
//   function check "am I running in the compiler right now?")
//
// WHY A VPN CARES
//   The protocol "magic number" (the first bytes that say "this is our
//   protocol, version 1") must be the same on every machine forever.
//   consteval guarantees it's baked in at compile time, never computed from
//   some run-time value by accident.
//
// RULE
//   constexpr = "can run at compile time". consteval = "must".
//   Use consteval for values that are part of the protocol definition.

// Rounds n up to a multiple of 16 (some ciphers pad to blocks of 16).
constexpr std::size_t paddedSize(std::size_t n)
{
    // static_assert(n > 0);   // ERROR: 'n' is not a constant expression.
    //                         // Parameters are never constexpr, even here.
    return (n + 15) / 16 * 16;
}

constexpr std::uint32_t asU32(char c)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(c));
}

// Packs 4 letters into one 4-byte number: 'V','P','N','1' → 0x56504E31.
consteval std::uint32_t makeMagic(char a, char b, char c, char d)
{
    return (asU32(a) << 24) | (asU32(b) << 16) | (asU32(c) << 8) | asU32(d);
}

constexpr std::uint32_t kProtocolMagic = makeMagic('V', 'P', 'N', '1');
static_assert(kProtocolMagic == 0x56504E31);

// The helper from point 3: its parameter value must be a compile-time
// constant, so whatever you pass in gets computed by the compiler.
consteval auto compileTime(auto value) { return value; }

void lesson_F_3()
{
    std::cout << "--- F.3 consteval ---\n";
    std::cout << "  magic = 0x" << std::hex << kProtocolMagic << std::dec
              << " = " << kProtocolMagic << "\n";   // 0x56504e31 = 1448103473

    // char letter = 'X';
    // makeMagic(letter, 'P', 'N', '1');   // ERROR: 'letter' isn't a constant,
    //                                     // and consteval MUST run at compile time.

    std::cout << "  paddedSize(1400) forced at compile time = "
              << compileTime(paddedSize(1400)) << "\n";   // 1408
    std::size_t runtimeLen = 33;
    std::cout << "  paddedSize(33) at run time = " << paddedSize(runtimeLen) << "\n\n";   // 48
}

// =============================================================================
// F.4  Constexpr functions (part 4)
// =============================================================================
// IN PLAIN WORDS
//   More things constexpr functions can do:
//     - have NON-const local variables and CHANGE them (loops, counters...)
//     - pass their parameters and locals on to OTHER constexpr functions
//     - build and return whole arrays → compile-time LOOKUP TABLES
//   Calling a NON-constexpr function (like std::cout << ...) is only allowed
//   on a path that runs at RUN time. If the compiler has to evaluate that
//   path at compile time, it's an error.
//
//   WHEN should a function be constexpr?
//     When it's a "pure function": same inputs → same output, every time, and
//     it doesn't touch anything outside itself (no network, files, clock,
//     globals). Checksums, size math, and parsing a constant string are pure.
//     sendto() and "read the current time" are not.
//   Why not make EVERYTHING constexpr? It's a promise to callers: removing
//   it later can break their code. And compile-time code is harder to debug
//   (no breakpoints, no printing). So use it where it actually helps.
//
// WHY A VPN CARES
//   CRC32 (a common checksum for detecting accidental damage) uses a
//   256-entry table. Build it at COMPILE time: no startup cost, no chance
//   of forgetting to initialize it, and static_assert tests it.
//   (Again: CRC32 catches accidents, NOT attackers. It is not crypto.)
//
// RULE
//   Make small pure functions constexpr. Build constant tables at compile time.

// Builds the standard CRC32 lookup table (polynomial 0xEDB88320).
constexpr std::array<std::uint32_t, 256> makeCrcTable()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n)
    {
        std::uint32_t c = n;   // a local variable we keep changing: allowed
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[n] = c;
    }
    return table;
}

constexpr auto kCrcTable = makeCrcTable();   // 256 numbers, computed by the compiler

// Calls into the table built by another constexpr function.
constexpr std::uint32_t crc32(std::string_view data)
{
    std::uint32_t c = 0xFFFFFFFFu;
    for (char ch : data)
    {
        std::uint32_t index = (c ^ asU32(ch)) & 0xFFu;
        c = kCrcTable[index] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

// "123456789" → 0xCBF43926 is THE standard test value for CRC32.
static_assert(crc32("123456789") == 0xCBF43926, "CRC32 is broken");

// A constexpr function result as an ARRAY SIZE (needs a constant expression).
constexpr std::size_t kMaxPlaintext = tunnelMtu(1500);
using PacketBuffer = std::array<Byte, kMaxPlaintext>;

void lesson_F_4()
{
    std::cout << "--- F.4 constexpr functions part 4 ---\n";
    std::cout << "  kCrcTable[1] = 0x" << std::hex << kCrcTable[1] << "\n";   // 0x77073096
    std::cout << "  crc32(\"123456789\") = 0x" << crc32("123456789") << std::dec << "\n";

    // Run time: CRC of a message that wasn't known while compiling.
    std::string_view message = "HELLO";
    std::cout << "  crc32(\"HELLO\") = 0x" << std::hex << crc32(message) << std::dec << "\n";

    PacketBuffer buffer{};   // size fixed at compile time by tunnelMtu(1500)
    std::cout << "  PacketBuffer holds " << buffer.size() << " bytes\n\n";   // 1420
}

int main()
{
    lesson_F_1();
    lesson_F_2();
    lesson_F_3();
    lesson_F_4();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Change kTagSize to 12 and rebuild. Which static_assert fires? Read the
//      message. That's a bug caught before the program even exists.
//   2. Write 'constexpr bool isValidPort(int p)' (1..65535). Test it with
//      three static_asserts: 51820 → true, 0 → false, 70000 → false.
//   3. Make 'consteval std::uint16_t makeVersion(int major, int minor)' that
//      packs major into the top byte and minor into the bottom byte.
//      static_assert that makeVersion(1, 2) == 0x0102. Then try calling it
//      with a non-constexpr int and read the error.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - A constexpr function is called in std::cout << f(5). Is it guaranteed to
//     run at compile time? Why or why not?
//   - Why can't you static_assert on a parameter inside a constexpr function?
//   - Why is a CRC32 or IPv4 checksum NOT enough to stop an attacker?
// =============================================================================
