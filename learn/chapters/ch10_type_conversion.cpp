// =============================================================================
// CHAPTER 10 — Type Conversion, Type Aliases, and Type Deduction
// learncpp.com/cpp-tutorial  (lessons 10.1 – 10.9)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   Every value in C++ has a TYPE, and when you mix types, C++ quietly
//   converts them. Sometimes that loses data. In network code, lost data
//   means corrupted packets, so you must know exactly when it happens.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN packet is a row of bytes with fixed-size fields:
//
//       [ type: 1 byte ][ length: 2 bytes ][ counter: 8 bytes ][ data ... ]
//
//   If you put a 4-byte int into a 2-byte field by accident, the top two
//   bytes get chopped off. No error. The other side just reads garbage.
//   This chapter is about spotting that before it happens.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch10_type_conversion.cpp -o ch10 && ./ch10
// =============================================================================

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <utility>   // std::cmp_less
#include <vector>

// =============================================================================
// 10.1  Implicit type conversion
// =============================================================================
// IN PLAIN WORDS
//   "Implicit" means "without you asking". If a function wants a double and
//   you give it an int, C++ converts the int to a double for you.
//
//   Think of it like a power adapter that plugs itself in. Handy, but you
//   don't always notice it's there.
//
// WHY A VPN CARES
//   Most surprise bugs in packet code are implicit conversions nobody saw.
//
// RULE
//   Know it happens. Turn on -Wconversion so the compiler TELLS you.
void lesson_10_1()
{
    std::cout << "--- 10.1 implicit conversion ---\n";
    double seconds = 5;        // int 5 → double 5.0, silently. Harmless here.
    std::uint8_t ttl = 64;     // int 64 → uint8_t. Fits, so the compiler is fine.
    std::cout << "seconds=" << seconds << " ttl=" << static_cast<int>(ttl) << "\n\n";
}

// =============================================================================
// 10.2  Floating-point and integral promotion
// =============================================================================
// IN PLAIN WORDS
//   Small integer types (char, uint8_t, uint16_t, bool) get "promoted" to
//   int before ANY math. The CPU likes int-sized numbers.
//
//   So uint8_t + uint8_t is NOT a uint8_t. It's an int.
//
// WHY A VPN CARES
//   Byte math is everywhere: building a 2-byte length from two bytes, XORing
//   bytes for encryption. The result is an int, and putting it back into a
//   byte is a narrowing conversion (10.4) that -Wconversion will flag.
//
// RULE
//   After math on small types, cast the result back on purpose.
void lesson_10_2()
{
    std::cout << "--- 10.2 promotion ---\n";
    std::uint8_t a = 200;
    std::uint8_t b = 100;
    auto sum = a + b;   // a and b become int first, so sum is an int
    std::cout << "200 + 100 with uint8_t operands = " << sum
              << " (an int, so no wrap to 44)\n";

    // XOR two bytes, like a (toy) cipher does. The ^ result is an int.
    std::uint8_t key = 0x5A;
    std::uint8_t plain = 'H';
    auto encrypted = static_cast<std::uint8_t>(plain ^ key);   // cast back to a byte
    std::cout << "'H' ^ 0x5A = " << static_cast<int>(encrypted) << "\n\n";
}

// =============================================================================
// 10.3  Numeric conversions
// =============================================================================
// IN PLAIN WORDS
//   Converting between DIFFERENT number types (not just promoting) is a
//   "numeric conversion": int → short, double → int, int → unsigned...
//   Some are always safe (small → big). Some can lose data:
//     - too-big numbers get chopped (wrap around)
//     - decimals get cut off (3.9 → 3, NOT rounded)
//     - negative → unsigned becomes a huge number
//
// WHY A VPN CARES
//   A packet length of -1 (an error code) turned into an unsigned size
//   becomes 18446744073709551615. Then "copy length bytes" copies forever.
//   That's a real class of security bugs.
//
// RULE
//   Check the range BEFORE converting to a smaller or unsigned type.
void lesson_10_3()
{
    std::cout << "--- 10.3 numeric conversions ---\n";
    int bigLength = 70000;
    auto chopped = static_cast<std::uint16_t>(bigLength);
    std::cout << "70000 into a 2-byte length field → " << chopped << " (wrapped!)\n";

    double rtt = 3.9;   // round-trip time in ms
    std::cout << "3.9 ms as int → " << static_cast<int>(rtt) << " (cut, not rounded)\n";

    int errorCode = -1;   // like recvfrom() returning -1 on error
    auto asSize = static_cast<std::size_t>(errorCode);
    std::cout << "-1 as size_t → " << asSize << "  ← DANGER\n\n";
}

// =============================================================================
// 10.4  Narrowing conversions, list initialization, and constexpr initializers
// =============================================================================
// IN PLAIN WORDS
//   "Narrowing" = converting to a type that might not fit the value.
//   Brace initialization {} REFUSES narrowing. It's a compile error.
//   One exception: if the value is a compile-time constant that fits, {} allows it.
//
// WHY A VPN CARES
//   Braces turn "silently corrupted header" into "won't compile".
//
// RULE
//   Initialize with {} by default.
void lesson_10_4()
{
    std::cout << "--- 10.4 narrowing + {} ---\n";
    // std::uint16_t port{70000};   // ERROR: 70000 doesn't fit in 2 bytes
    // int n{3.5};                  // ERROR: would lose the .5

    constexpr int kTunMtu = 1420;   // a compile-time constant (WireGuard's default MTU)
    std::uint16_t mtu{kTunMtu};     // OK: constant AND it fits, so no narrowing
    std::cout << "mtu=" << mtu << "\n\n";
}

// =============================================================================
// 10.5  Arithmetic conversions
// =============================================================================
// IN PLAIN WORDS
//   When an operator has two DIFFERENT types (int + double, int < unsigned),
//   C++ converts both to one "common type" first.
//   The nasty case: signed vs unsigned. The signed one becomes unsigned.
//   So -1 < 1u is FALSE, because -1 turns into a huge unsigned number.
//
// WHY A VPN CARES
//   "if (bytesReceived < headerSize)" where one is signed and one unsigned
//   can give the wrong answer and let a short (malicious) packet through.
//
// RULE
//   Don't compare signed with unsigned. Use std::cmp_less (C++20) or make
//   both the same type first.
void lesson_10_5()
{
    std::cout << "--- 10.5 arithmetic conversions ---\n";
    long received = -1;                // recvfrom failed
    std::size_t headerSize = 16;
    // received < headerSize  →  -1 becomes huge  →  false  →  "packet looks big enough"!
    bool wrong = static_cast<std::size_t>(received) < headerSize;   // what C++ does for you
    bool right = std::cmp_less(received, headerSize);               // C++20: math-correct
    std::cout << std::boolalpha << "naive check says too short? " << wrong
              << "   cmp_less says too short? " << right << "\n\n";
}

// =============================================================================
// 10.6  Explicit type conversion (casting) and static_cast
// =============================================================================
// IN PLAIN WORDS
//   "Explicit" = you ask for it. static_cast<NewType>(value) means
//   "convert this, I know what I'm doing". It's searchable and obvious.
//
//   Other casts you'll meet in network code:
//     reinterpret_cast  treat raw bytes as another type (sockaddr*). Risky.
//     const_cast        remove const. Almost never right.
//     (int)x            C-style. Avoid: it can secretly do any of the above.
//
// RULE
//   Use static_cast. If you need reinterpret_cast, slow down and double-check.
void lesson_10_6()
{
    std::cout << "--- 10.6 static_cast ---\n";
    std::uint32_t counter = 0x11223344;
    // Pull out the top byte of a 4-byte number:
    auto topByte = static_cast<std::uint8_t>(counter >> 24);
    std::cout << "top byte of 0x11223344 = 0x" << std::hex << static_cast<int>(topByte)
              << std::dec << "\n\n";
}

// =============================================================================
// 10.7  Typedefs and type aliases
// =============================================================================
// IN PLAIN WORDS
//   'using NewName = ExistingType;' gives a type a nickname.
//   It does NOT make a new type. It's just a better name.
//   (typedef is the old way to write the same thing.)
//
// WHY A VPN CARES
//   "std::array<std::uint8_t, 32>" says nothing. "Key" says everything.
//   Aliases make security code readable, and readable code gets reviewed.
//
// RULE
//   Use aliases for long types and for types with a MEANING.
using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;      // a packet, a payload, a ciphertext
using Counter = std::uint64_t;        // per-packet counter (used for replay protection)
using PeerId = std::uint32_t;

void lesson_10_7()
{
    std::cout << "--- 10.7 type aliases ---\n";
    Bytes payload{0x45, 0x00, 0x00, 0x54};   // first bytes of an IPv4 packet
    Counter sent = 1;
    PeerId alice = 7;
    std::cout << "payload size " << payload.size() << ", counter " << sent << ", peer "
              << alice << "\n\n";
}

// =============================================================================
// 10.8  Type deduction for objects using the auto keyword
// =============================================================================
// IN PLAIN WORDS
//   'auto x = something;' → the compiler picks x's type from 'something'.
//   Gotcha: auto drops const and & (references). 'auto x = bigVector;' COPIES.
//
// RULE
//   Use auto when the type is obvious from the right side (a cast, make_unique,
//   an iterator). Write 'const auto&' to avoid copies in loops.
void lesson_10_8()
{
    std::cout << "--- 10.8 auto ---\n";
    Bytes packet(1400, 0);
    auto size = packet.size();             // std::size_t
    auto first = packet.front();           // Byte (a copy of one byte; cheap)
    const auto& sameBuffer = packet;       // no copy of 1400 bytes
    std::cout << "size=" << size << " first=" << static_cast<int>(first)
              << " sameBuffer.size()=" << sameBuffer.size() << "\n\n";
}

// =============================================================================
// 10.9  Type deduction for functions
// =============================================================================
// IN PLAIN WORDS
//   A function can say 'auto' as its return type and let the compiler figure
//   it out from the return statement.
//
// RULE
//   Fine for short helpers. For anything in a header that other people call,
//   write the real return type: it's documentation.
auto headerSize() { return std::size_t{16}; }   // deduced: std::size_t

// Trailing return type: same thing, written at the end. You'll see it in
// templates. 'auto f() -> int' means "returns int".
auto isValidPort(int port) -> bool { return port > 0 && port <= 65535; }

void lesson_10_9()
{
    std::cout << "--- 10.9 auto return types ---\n";
    std::cout << "headerSize()=" << headerSize() << " isValidPort(51820)=" << std::boolalpha
              << isValidPort(51820) << "\n\n";
}

int main()
{
    lesson_10_1();
    lesson_10_2();
    lesson_10_3();
    lesson_10_4();
    lesson_10_5();
    lesson_10_6();
    lesson_10_7();
    lesson_10_8();
    lesson_10_9();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment 'std::uint16_t port{70000};' in 10.4. Read the error.
//   2. Write 'std::uint16_t readLength(Byte hi, Byte lo)' that builds a
//      2-byte number from two bytes: (hi << 8) | lo. Compile with
//      -Wconversion. Fix the warning with static_cast (10.2 tells you why).
//   3. In 10.5, change std::cmp_less to a plain < and turn on -Wextra.
//      What warning do you get?
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why is uint8_t + uint8_t an int?
//   - Why is -1 < 1u false?
//   - What's the difference between 'using Key = ...' and a brand-new type?
// =============================================================================
