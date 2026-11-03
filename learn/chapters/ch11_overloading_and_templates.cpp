// =============================================================================
// CHAPTER 11 — Function Overloading and Function Templates
// learncpp.com/cpp-tutorial  (lessons 11.1 – 11.10)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   One function NAME can have many versions: overloads you write by hand,
//   or templates where the compiler writes the versions for you.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN reads and writes header fields of different sizes:
//
//       [ type: 1 byte ][ length: 2 bytes ][ peer: 4 bytes ][ counter: 8 bytes ]
//
//   Without templates you'd write readU16, readU32, readU64... four copies
//   of the same loop, and a bug fixed in one stays alive in the others.
//   With a template you write readBigEndian<T> ONCE.
//   Overloading and '= delete' also let you make the WRONG call impossible,
//   e.g. passing 'true' where a buffer length was expected.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → example code.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch11_overloading_and_templates.cpp -o ch11 && ./ch11
// =============================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>   // std::in_range
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;      // a packet, a payload, a ciphertext
using Counter = std::uint64_t;        // per-packet counter (replay protection)
using PeerId = std::uint32_t;

// =============================================================================
// 11.1  Introduction to function overloading
// =============================================================================
// IN PLAIN WORDS
//   "Overloading" = two or more functions with the SAME name but DIFFERENT
//   parameters. The compiler looks at what you pass and picks the right one.
//
//   Like one "send" button in a chat app: it works for text AND for files.
//   You press the same button; the app does the right thing for each.
//
// WHY A VPN CARES
//   You send text (control messages) and raw bytes (tunnelled IP packets).
//   'sendPayload(text)' and 'sendPayload(bytes)' read better than
//   'sendPayloadText' and 'sendPayloadBytes'.
//
// RULE
//   Overload when the functions do the SAME job for different input types.
//   If they do different jobs, give them different names.

// Both "send" (simulated: we just return how many bytes would go out).
std::size_t sendPayload(std::string_view text)
{
    std::cout << "  sending text \"" << text << "\"\n";
    return text.size();
}

std::size_t sendPayload(const Bytes& bytes)   // '&' = no copy (chapter 12)
{
    std::cout << "  sending " << bytes.size() << " raw bytes\n";
    return bytes.size();
}

void lesson_11_1()
{
    std::cout << "--- 11.1 overloading ---\n";
    Bytes ipPacket{0x45, 0x00, 0x00, 0x54};
    std::size_t a = sendPayload("HELLO");   // picks the string_view version
    std::size_t b = sendPayload(ipPacket);  // picks the Bytes version
    std::cout << "  total = " << a + b << " bytes\n\n";   // 5 + 4 = 9
}

// =============================================================================
// 11.2  Function overload differentiation
// =============================================================================
// IN PLAIN WORDS
//   How does the compiler tell overloads apart? Only by the PARAMETERS:
//     - a different NUMBER of parameters, or
//     - different TYPES of parameters.
//   The RETURN TYPE does not count. Neither do parameter NAMES.
//
// WHY A VPN CARES
//   You might want 'parsePort' to return an int in one place and a uint16_t
//   in another. You can't do that with overloading alone; you need two names
//   (or a template, see 11.6).
//
// RULE
//   Overloads must differ in parameter count or parameter types.

// Different NUMBER of parameters → OK.
Counter makeNonceSeed(Counter counter) { return counter; }
Counter makeNonceSeed(Counter counter, PeerId peer)
{
    return (static_cast<Counter>(peer) << 32) ^ counter;   // just a demo mix
}

// Different RETURN type only → ERROR. The compiler can't tell which you meant.
//   int           parsePort(std::string_view s);
//   std::uint16_t parsePort(std::string_view s);   // ERROR: differs only in return type

void lesson_11_2()
{
    std::cout << "--- 11.2 overload differentiation ---\n";
    std::cout << "  makeNonceSeed(5)    = " << makeNonceSeed(5) << "\n";
    std::cout << "  makeNonceSeed(5, 1) = " << makeNonceSeed(5, 1)
              << "  (that's 1<<32 ^ 5 = 4294967301)\n\n";
}

// =============================================================================
// 11.3  Function overload resolution and ambiguous matches
// =============================================================================
// IN PLAIN WORDS
//   "Overload resolution" = the compiler's process for choosing an overload.
//   It tries these steps IN ORDER and stops at the first step that finds a match:
//     1. Exact match           (int → int)
//     2. Promotion             (char/bool/uint8_t → int,  float → double)
//     3. Numeric conversion    (int → double, long → int, ...)
//     4. User-defined conversion (e.g. const char* → std::string)
//     5. Ellipsis (...)        (old C-style, rare)
//
//   "Ambiguous match" = two overloads tie at the same step. The compiler
//   refuses to guess and gives an error.
//
// WHY A VPN CARES
//   A Byte (uint8_t) header field passed to 'logField' goes to the INT
//   version, not some "byte" version, because promotion (step 2) wins.
//   Surprises like that print the wrong thing in your debug logs.
//
// RULE
//   If a call is ambiguous, fix it with static_cast at the call site so the
//   argument becomes an exact match.
void logField(int value)    { std::cout << "  logField(int)    got " << value << "\n"; }
void logField(double value) { std::cout << "  logField(double) got " << value << "\n"; }

void lesson_11_3()
{
    std::cout << "--- 11.3 overload resolution ---\n";
    logField(1420);        // step 1: exact match with int
    logField(0.5);         // step 1: exact match with double
    Byte ttl = 64;
    logField(ttl);         // step 2: uint8_t PROMOTES to int
    logField(2.5f);        // step 2: float PROMOTES to double

    // logField(5L);       // ERROR: ambiguous. long → int and long → double are
                           // BOTH step-3 conversions. It's a tie.
    long bytesRead = 5;
    logField(static_cast<int>(bytesRead));   // fixed: now an exact match
    std::cout << "\n";
}

// =============================================================================
// 11.4  Deleting functions
// =============================================================================
// IN PLAIN WORDS
//   Writing '= delete' after a function says: "calling this is a compile error."
//   Why would you want that? To BLOCK a bad call that would otherwise
//   silently convert and compile.
//
//   Example: reserveBuffer(std::size_t length). Someone writes
//   reserveBuffer(true). bool → size_t converts to 1. It compiles! 1-byte buffer!
//   Delete the bool overload and that mistake can't compile any more.
//
//   Note: a deleted function still takes part in overload resolution. If it's
//   the best match, you get an error instead of a sneaky conversion.
//
// WHY A VPN CARES
//   Security code should make wrong calls impossible, not just "unlikely".
//
// RULE
//   Use '= delete' to forbid conversions that would be a bug.
void reserveBuffer(std::size_t length)
{
    std::cout << "  reserving " << length << " bytes\n";
}
void reserveBuffer(bool) = delete;   // reserveBuffer(true)  → won't compile
void reserveBuffer(char) = delete;   // reserveBuffer('A')   → won't compile

// Stronger: allow ONLY an exact std::uint16_t. A template (11.6) that matches
// every other type exactly, and is deleted, catches everything else.
void setListenPort(std::uint16_t port) { std::cout << "  listening on " << port << "\n"; }
template <typename T>
void setListenPort(T) = delete;

void lesson_11_4()
{
    std::cout << "--- 11.4 deleting functions ---\n";
    reserveBuffer(std::size_t{1500});         // fine: exact match
    // reserveBuffer(1500);                   // ERROR: ambiguous! int → size_t,
                                              // int → bool, int → char are ALL
                                              // step-3 conversions (11.3). A tie.
                                              // Deleting overloads makes callers
                                              // be exact. That's the point.
    // reserveBuffer(true);                  // ERROR: use of deleted function
    // reserveBuffer('A');                    // ERROR: use of deleted function

    setListenPort(std::uint16_t{51820});      // fine: exact type
    // setListenPort(51820);                  // ERROR: int matches the deleted template
    // setListenPort(70000);                  // ERROR: and this one would have wrapped!
    std::cout << "\n";
}

// =============================================================================
// 11.5  Default arguments
// =============================================================================
// IN PLAIN WORDS
//   A "default argument" is a value a parameter gets if the caller leaves it out.
//     bool waitForPacket(int timeoutMs = 5000, int retries = 3);
//     waitForPacket();          // timeoutMs=5000, retries=3
//     waitForPacket(1000);      // timeoutMs=1000, retries=3
//     waitForPacket(1000, 0);   // timeoutMs=1000, retries=0
//
//   Rules:
//     - Defaults go on the RIGHTMOST parameters. You can't skip a middle one.
//     - Put the default in the DECLARATION (usually the header), not again
//       in the definition. Writing it twice is an error.
//     - Defaults can cause ambiguity: f(int) and f(int, int = 0) both match f(5).
//
// WHY A VPN CARES
//   Most calls want a normal timeout. Tests want a short one. Defaults keep
//   the common call short and still let tests override.
//
// RULE
//   Use defaults for "almost always this value". Keep them in one place.
bool waitForPacket(int timeoutMs = 5000, int retries = 3);   // declaration with defaults

bool waitForPacket(int timeoutMs, int retries)              // definition: NO defaults here
{
    std::cout << "  wait " << timeoutMs << " ms, " << retries << " retries\n";
    return false;   // simulated: nothing arrived
}

void lesson_11_5()
{
    std::cout << "--- 11.5 default arguments ---\n";
    waitForPacket();          // 5000, 3
    waitForPacket(1000);      // 1000, 3
    waitForPacket(1000, 0);   // 1000, 0
    std::cout << "\n";
}

// =============================================================================
// 11.6  Function templates
// =============================================================================
// IN PLAIN WORDS
//   A "template" is a pattern for a function. You write it once with a
//   placeholder type T. The compiler fills in T to make real functions.
//
//     template <typename T>      ← "T is some type; I'll tell you later"
//     T biggest(T a, T b) { return (a > b) ? a : b; }
//
//   "Big-endian" (also "network byte order") = most significant byte FIRST.
//   The number 0x1234 goes on the wire as 0x12, then 0x34. All internet
//   protocol headers use it, so you'll read and write it constantly.
//
// WHY A VPN CARES
//   readBigEndian<T> works for 2-, 4-, and 8-byte fields with ONE loop.
//   One loop to test. One loop to fix.
//
// RULE
//   When several functions are identical except for the type, write a template.

// Read sizeof(T) bytes starting at 'offset', most significant byte first.
// The CALLER must check offset + sizeof(T) <= buf.size() first.
// (Network data is untrusted. Chapter 12 shows std::optional for this.)
template <typename T>
T readBigEndian(const Bytes& buf, std::size_t offset)
{
    T value{};
    for (std::size_t i = 0; i < sizeof(T); ++i)
        value = static_cast<T>((value << 8) | static_cast<T>(buf[offset + i]));
    return value;
}

// Append 'value' to 'out' as sizeof(T) bytes, most significant byte first.
template <typename T>
void writeBigEndian(Bytes& out, T value)
{
    for (std::size_t i = sizeof(T); i > 0; --i)
        out.push_back(static_cast<Byte>(value >> (8 * (i - 1))));
}

void lesson_11_6()
{
    std::cout << "--- 11.6 function templates ---\n";
    Bytes header;
    writeBigEndian<std::uint16_t>(header, 0x1234);   // length field: 2 bytes
    std::cout << "  wrote 0x1234 as bytes:" << std::hex;
    for (Byte b : header)
        std::cout << " 0x" << static_cast<int>(b);   // 0x12 0x34
    std::cout << std::dec << "\n";

    auto length = readBigEndian<std::uint16_t>(header, 0);
    std::cout << "  read it back: " << length << " (0x1234 = 4660)\n\n";
}

// =============================================================================
// 11.7  Function template instantiation
// =============================================================================
// IN PLAIN WORDS
//   "Instantiation" = the compiler making a real function from the template.
//   readBigEndian<std::uint32_t>(...) makes a uint32_t version.
//   readBigEndian<std::uint64_t>(...) makes ANOTHER, separate version.
//   Only the versions you actually use get made.
//
//   "Template argument deduction" = you skip the <T> and the compiler
//   guesses T from the argument you passed:
//     writeBigEndian(out, std::uint16_t{7});   // T deduced as std::uint16_t
//
// WHY A VPN CARES
//   Deduction is a TRAP for wire formats. writeBigEndian(out, 1420) deduces
//   T = int, which is 4 bytes. Your 2-byte length field just became 4 bytes
//   and every field after it is in the wrong place.
//
// RULE
//   For anything that touches the wire, write the type explicitly:
//   writeBigEndian<std::uint16_t>(...). Don't let deduction pick sizes.
void lesson_11_7()
{
    std::cout << "--- 11.7 template instantiation ---\n";
    Bytes good;
    writeBigEndian<std::uint16_t>(good, 1420);   // explicit: 2 bytes
    Bytes oops;
    writeBigEndian(oops, 1420);                  // deduced T = int: 4 bytes!
    std::cout << "  explicit <uint16_t> wrote " << good.size() << " bytes\n";
    std::cout << "  deduced  <int>      wrote " << oops.size() << " bytes  ← wrong size\n";

    // Two more instantiations, for a 4-byte peer id and an 8-byte counter:
    Bytes packet;
    writeBigEndian<PeerId>(packet, 7);
    writeBigEndian<Counter>(packet, 1000);
    std::cout << "  peer=" << readBigEndian<PeerId>(packet, 0)
              << " counter=" << readBigEndian<Counter>(packet, 4)
              << " (packet is " << packet.size() << " bytes)\n\n";   // 4 + 8 = 12
}

// =============================================================================
// 11.8  Function templates with multiple template types
// =============================================================================
// IN PLAIN WORDS
//   With ONE type T, biggest(1400, 1420.5) fails: is T int or double?
//   Fix: use TWO placeholder types, T and U.
//
//   C++20 shortcut ("abbreviated function template"): write 'auto' as the
//   parameter type. Each 'auto' parameter becomes its own template type.
//     auto biggest(auto a, auto b)   ==   template <typename T, typename U> ...
//
// WHY A VPN CARES
//   A safe "will this value fit in that type?" check needs two types: the
//   type you HAVE and the type you WANT. That's a two-type template.
//
// RULE
//   Use extra template types when parameters really can differ.
auto biggest(auto a, auto b)
{
    return (a > b) ? a : b;   // int vs double → result is double
}

// To = the type you want. From = the type you have (deduced).
// std::in_range<To>(v) (C++20) is true if v fits in To without changing.
template <typename To, typename From>
bool fitsIn(From value)
{
    return std::in_range<To>(value);
}

void lesson_11_8()
{
    std::cout << "--- 11.8 multiple template types ---\n";
    std::cout << "  biggest(1400, 1420.5) = " << biggest(1400, 1420.5) << "\n";
    std::cout << std::boolalpha;
    std::cout << "  1420  fits in uint16_t? " << fitsIn<std::uint16_t>(1420) << "\n";
    std::cout << "  70000 fits in uint16_t? " << fitsIn<std::uint16_t>(70000) << "\n";
    std::cout << "  -1    fits in size_t?   " << fitsIn<std::size_t>(-1) << "\n\n";
}

// =============================================================================
// 11.9  Non-type template parameters
// =============================================================================
// IN PLAIN WORDS
//   A template parameter can be a VALUE, not just a type:
//     template <std::size_t N>
//   N must be known at compile time. The compiler makes one version per N.
//   This is how std::array<Byte, 32> works: 32 is a non-type parameter.
//
// WHY A VPN CARES
//   Keys and nonces have FIXED sizes:
//     ChaCha20-Poly1305 key   = 32 bytes
//     ChaCha20-Poly1305 nonce = 12 bytes
//   With N as a template parameter, the size is part of the TYPE.
//   A 12-byte nonce can't be passed where a 32-byte key is expected.
//   And static_assert can reject a too-short key before the program exists.
//
// RULE
//   When a size is fixed by the protocol, make it a compile-time N.
template <std::size_t N>
using FixedBytes = std::array<Byte, N>;

using Key = FixedBytes<32>;
using Nonce = FixedBytes<12>;

// Makes N bytes 0, 1, 2, ... — TEST DATA ONLY, NOT A REAL KEY.
// Real keys come from a secure random source (e.g. libsodium's randombytes_buf).
template <std::size_t N>
FixedBytes<N> makeTestBytes()
{
    static_assert(N >= 12, "too short for a key or nonce");   // checked at compile time
    FixedBytes<N> out{};
    for (std::size_t i = 0; i < N; ++i)
        out[i] = static_cast<Byte>(i);
    return out;
}

template <std::size_t N>
void printSize(const FixedBytes<N>&)
{
    std::cout << "  this is a " << N << "-byte value\n";
}

void useKey(const Key&) { std::cout << "  useKey got a 32-byte key\n"; }

void lesson_11_9()
{
    std::cout << "--- 11.9 non-type template parameters ---\n";
    Key key = makeTestBytes<32>();
    Nonce nonce = makeTestBytes<12>();
    printSize(key);     // N deduced as 32
    printSize(nonce);   // N deduced as 12
    useKey(key);
    // useKey(nonce);               // ERROR: FixedBytes<12> is not FixedBytes<32>
    // auto weak = makeTestBytes<8>();   // ERROR: static_assert "too short..."
    std::cout << "  last key byte = " << static_cast<int>(key[31]) << "\n\n";   // 31
}

// =============================================================================
// 11.10  Using function templates in multiple files
// =============================================================================
// IN PLAIN WORDS
//   Normal functions: declaration in a .h, body in a .cpp. The linker joins them.
//   Templates are different. The compiler needs the FULL template body at the
//   moment it instantiates it (11.7), in the same .cpp file that uses it.
//   If the body hides in another .cpp, you get a LINKER error
//   ("undefined symbol readBigEndian<unsigned short>...").
//
//   So: put the WHOLE template in the header.
//
//     // wire.h
//     #pragma once
//     #include <cstdint>
//     #include <vector>
//     template <typename T>
//     T readBigEndian(const std::vector<std::uint8_t>& buf, std::size_t offset)
//     { ...whole body here... }
//
//     // tunnel.cpp and handshake.cpp both: #include "wire.h"
//
//   Isn't that a "defined twice" error? No. Templates are exempt from the
//   one-definition rule in the same way inline functions are, as long as every
//   copy is identical (which it is, because it's the same header).
//
// WHY A VPN CARES
//   Your wire-format helpers (readBigEndian, writeBigEndian) will live in one
//   header used by the tunnel, the handshake, and the tests.
//
// RULE
//   Templates go fully in headers. Non-template functions: declare in .h,
//   define in .cpp.
void lesson_11_10()
{
    std::cout << "--- 11.10 templates in multiple files ---\n";
    std::cout << "  templates live fully in headers (see comments above)\n\n";
}

int main()
{
    lesson_11_1();
    lesson_11_2();
    lesson_11_3();
    lesson_11_4();
    lesson_11_5();
    lesson_11_6();
    lesson_11_7();
    lesson_11_8();
    lesson_11_9();
    lesson_11_10();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment 'reserveBuffer(true);' in 11.4. Read the error. Then remove
//      BOTH '= delete' lines and see that it compiles and reserves 1 byte.
//   2. Add a bounds check to readBigEndian: if offset + sizeof(T) > buf.size(),
//      return 0 and print "short packet". Test it with a 1-byte buffer and
//      readBigEndian<std::uint16_t>. (Chapter 12 shows a better way: std::optional.)
//   3. Write 'template <std::size_t N> bool allZero(const FixedBytes<N>& b)'.
//      A key that is all zero is a classic "forgot to initialize it" bug.
//      Test it on FixedBytes<32>{} and on makeTestBytes<32>().
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why can't two overloads differ only by return type?
//   - Why does writeBigEndian(out, 1420) write 4 bytes instead of 2?
//   - Why must a template's body be in the header, not in a .cpp?
// =============================================================================
