// =============================================================================
// CHAPTER 12 — Compound Types: References and Pointers
// learncpp.com/cpp-tutorial  (lessons 12.1 – 12.15)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   A reference is a second NAME for an existing object, a pointer is a
//   variable that holds an object's ADDRESS, and both let you work on the
//   real thing instead of a copy.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN moves a LOT of packets, each up to ~1500 bytes. Copying every
//   packet into every function would waste time. So you pass them by
//   reference. And the operating system's socket API (recvfrom, sendto,
//   getsockname) is a C API: it hands you results through POINTERS.
//
//   It's also where the scariest bugs live: a pointer to memory that's gone
//   ("dangling"), or a nullptr you forgot to check. Packets come from
//   strangers on the internet. Untrusted data must NEVER crash your program.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → example code.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch12_references_and_pointers.cpp -o ch12 && ./ch12
// =============================================================================

#include <algorithm>   // std::min
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// =============================================================================
// 12.1  Introduction to compound data types
// =============================================================================
// IN PLAIN WORDS
//   "Fundamental" types are built in: int, double, bool, char...
//   "Compound" types are BUILT FROM other types:
//     functions, arrays, pointers (int*), references (int&),
//     enums, classes/structs (std::vector, std::string are classes).
//
// WHY A VPN CARES
//   Nearly every interesting type in a VPN is compound: a packet (Bytes),
//   a key (std::array<Byte, 32>), a socket address (a struct).
//
// RULE
//   Nothing to memorize yet. This chapter covers two compound types:
//   references and pointers.
void lesson_12_1()
{
    std::cout << "--- 12.1 compound types ---\n";
    Byte b = 0x45;                     // fundamental
    Bytes packet{0x45, 0x00};          // compound (a class holding Bytes)
    Byte* where = &b;                  // compound (a pointer to Byte)
    std::cout << "  sizeof(Byte)=" << sizeof(b) << " packet holds " << packet.size()
              << " bytes, *where=0x" << std::hex << static_cast<int>(*where) << std::dec
              << "\n\n";
}

// =============================================================================
// 12.2  Value categories (lvalues and rvalues)
// =============================================================================
// IN PLAIN WORDS
//   Every expression is either:
//     lvalue = something with a NAME and an ADDRESS you can point to.
//              It sticks around. e.g. a variable 'packetLen'.
//     rvalue = a temporary VALUE with no lasting home.
//              e.g. 5, packetLen + 1, or what a function returns by value.
//   Quick test: can you write &thing ? Then it's an lvalue.
//
// WHY A VPN CARES
//   The next lessons say "a non-const reference can only bind to an lvalue".
//   You need these words to read those rules (and compiler errors).
//
// RULE
//   lvalue = has an address. rvalue = temporary. That's enough for now.
Bytes buildPing() { return Bytes{0x01, 0x02}; }   // the result is an rvalue

void lesson_12_2()
{
    std::cout << "--- 12.2 lvalues and rvalues ---\n";
    int packetLen = 1400;   // 'packetLen' is an lvalue
    int* p = &packetLen;    // OK: lvalues have addresses
    // int* q = &(packetLen + 1);   // ERROR: 'packetLen + 1' is an rvalue, no address
    // Bytes* r = &buildPing();     // ERROR: the returned temporary is an rvalue
    std::cout << "  packetLen=" << *p << ", buildPing() size=" << buildPing().size() << "\n\n";
}

// =============================================================================
// 12.3  Lvalue references
// =============================================================================
// IN PLAIN WORDS
//   'Bytes& ref = packet;' makes 'ref' another NAME for 'packet'.
//   Not a copy. Same object. Change one, you changed "both" (there's only one).
//   Rules:
//     - must be initialized when created
//     - can't be changed later to refer to something else ("can't reseat")
//     - a non-const reference can only bind to a modifiable lvalue
//
// WHY A VPN CARES
//   'Bytes& out = sendBuffer;' lets you build a packet in place, no copies.
//
// RULE
//   A reference is an alias. Treat 'ref' exactly like the original.
void lesson_12_3()
{
    std::cout << "--- 12.3 lvalue references ---\n";
    Bytes sendBuffer{0x01};
    Bytes& out = sendBuffer;   // alias
    out.push_back(0x02);       // changes sendBuffer
    std::cout << "  sendBuffer.size()=" << sendBuffer.size()
              << " same object? " << std::boolalpha << (&out == &sendBuffer) << "\n";
    // Bytes& bad = buildPing();   // ERROR: can't bind non-const ref to an rvalue
    std::cout << "\n";
}

// =============================================================================
// 12.4  Lvalue references to const
// =============================================================================
// IN PLAIN WORDS
//   'const Bytes& view = packet;' is a READ-ONLY alias. You can look, not touch.
//   Bonus: a const reference CAN bind to an rvalue (a temporary). C++ then
//   keeps the temporary alive as long as the reference lives
//   ("lifetime extension").
//
// WHY A VPN CARES
//   Code that only INSPECTS a packet (logging, length checks) should get a
//   read-only view. Then it physically can't corrupt the packet.
//
// RULE
//   If you don't need to modify it, make the reference const.
void lesson_12_4()
{
    std::cout << "--- 12.4 references to const ---\n";
    Bytes packet{0x45, 0x00, 0x00, 0x54};
    const Bytes& view = packet;
    // view.push_back(0x00);           // ERROR: view is read-only
    const Bytes& ping = buildPing();   // OK: temporary kept alive while 'ping' lives
    std::cout << "  view.size()=" << view.size() << " ping.size()=" << ping.size() << "\n\n";
}

// =============================================================================
// 12.5  Pass by lvalue reference
// =============================================================================
// IN PLAIN WORDS
//   'void f(Bytes& packet)' → the function works on the CALLER'S object.
//   No copy, and changes are visible to the caller.
//
// WHY A VPN CARES
//   "Append the counter to this packet" should change THE packet, not a copy
//   that gets thrown away.
//
// RULE
//   Pass by non-const reference when the function's JOB is to modify the argument.
void appendCounter(Bytes& packet, Counter counter)
{
    for (int shift = 56; shift >= 0; shift -= 8)   // 8 bytes, big-endian
        packet.push_back(static_cast<Byte>(counter >> shift));
}

void appendCounterBroken(Bytes packet, Counter counter)   // BY VALUE: gets a copy
{
    appendCounter(packet, counter);   // modifies the copy, which then disappears
}

void lesson_12_5()
{
    std::cout << "--- 12.5 pass by reference ---\n";
    Bytes packet{0x04};   // type byte
    appendCounterBroken(packet, 7);
    std::cout << "  after by-value version:     " << packet.size() << " bytes (unchanged!)\n";
    appendCounter(packet, 7);
    std::cout << "  after by-reference version: " << packet.size()
              << " bytes, last byte = " << static_cast<int>(packet.back()) << "\n\n";   // 9, 7
}

// =============================================================================
// 12.6  Pass by const lvalue reference
// =============================================================================
// IN PLAIN WORDS
//   'void f(const Bytes& packet)' → no copy AND no changes allowed.
//   This is the default way to pass anything bigger than a few bytes.
//
//   When to pass by VALUE instead: cheap types. int, double, Byte, PeerId,
//   pointers, std::string_view. Copying those is as cheap as a reference.
//
// WHY A VPN CARES
//   A 1400-byte packet passed by value to 5 functions = 7000 bytes copied,
//   for every packet, millions of times. By const& it's zero.
//
// RULE
//   Big or unknown-size types: const&. Small built-in types: by value.
std::size_t countZeroBytes(const Bytes& packet, const Bytes& original)
{
    std::cout << "  is it the caller's object (no copy)? " << std::boolalpha
              << (&packet == &original) << "\n";
    std::size_t zeros = 0;
    for (Byte b : packet)
        if (b == 0)
            ++zeros;
    return zeros;
}

void lesson_12_6()
{
    std::cout << "--- 12.6 pass by const reference ---\n";
    Bytes packet(1400, 0);
    packet[0] = 0x45;
    std::size_t zeros = countZeroBytes(packet, packet);
    std::cout << "  zero bytes: " << zeros << "\n\n";   // 1399
}

// =============================================================================
// 12.7  Introduction to pointers
// =============================================================================
// IN PLAIN WORDS
//   Every object lives at an ADDRESS in memory (a number, like a house number).
//     &x    "address-of": gives the address of x
//     Byte* a variable that holds the address of a Byte (a "pointer to Byte")
//     *p    "dereference": go to that address and use the object there
//   Unlike a reference, a pointer CAN be changed to point somewhere else,
//   and CAN point to nothing (12.8).
//
// WHY A VPN CARES
//   The OS socket functions are C functions. C has no references, only
//   pointers. recvfrom wants a pointer to YOUR buffer to write into.
//
// RULE
//   & gets an address, * follows one. Read 'Byte* p' as "p points to a Byte".
void lesson_12_7()
{
    std::cout << "--- 12.7 pointers ---\n";
    Byte ttl = 64;
    Byte* p = &ttl;                 // p holds ttl's address
    *p = 63;                        // follow the address, change the Byte there
    std::cout << "  ttl is now " << static_cast<int>(ttl) << "\n";   // 63

    Byte hops = 5;
    p = &hops;                      // pointers CAN be re-pointed (references can't)
    std::cout << "  *p is now " << static_cast<int>(*p)
              << ", a pointer is " << sizeof(p) << " bytes on a 64-bit machine\n\n";
}

// =============================================================================
// 12.8  Null pointers
// =============================================================================
// IN PLAIN WORDS
//   'nullptr' = a pointer that points at NOTHING.
//   Dereferencing nullptr (*p when p is nullptr) is undefined behavior:
//   usually a crash. "Undefined behavior" (UB) = C++ makes no promises at all.
//
// WHY A VPN CARES
//   "Find the peer this packet is from" might find nobody. Returning nullptr
//   for "not found" is common. Forgetting the check = a stranger on the
//   internet can crash your VPN with one unknown packet.
//
// RULE
//   Initialize pointers (to nullptr if nothing else). Check before you '*'.
const Byte* findByte(const Bytes& packet, Byte wanted)
{
    for (const Byte& b : packet)
        if (b == wanted)
            return &b;            // address of the matching byte inside the packet
    return nullptr;               // not found
}

void lesson_12_8()
{
    std::cout << "--- 12.8 null pointers ---\n";
    Bytes packet{0x01, 0x7F, 0x02};
    const Byte* hit = findByte(packet, 0x7F);
    const Byte* miss = findByte(packet, 0xAA);
    if (hit)                                // same as: hit != nullptr
        std::cout << "  found 0x7F: *hit = " << static_cast<int>(*hit) << "\n";   // 127
    if (!miss)
        std::cout << "  0xAA not found (got nullptr), NOT dereferencing it\n";
    // std::cout << *miss;                  // UB: dereferencing nullptr
    std::cout << "\n";
}

// =============================================================================
// 12.9  Pointers and const
// =============================================================================
// IN PLAIN WORDS
//   Read pointer types RIGHT TO LEFT:
//     const Byte* p   "p is a pointer to a const Byte"
//                     → can't change the Byte through p; CAN re-point p
//     Byte* const p   "p is a const pointer to a Byte"
//                     → CAN change the Byte; can't re-point p
//     const Byte* const p   → neither
//
// WHY A VPN CARES
//   Key material should be handed out as 'const Byte*': a read-only view.
//   A function that just USES the key (to encrypt) then can't overwrite it.
//
// RULE
//   Pointer to const for read-only access. Most code wants 'const T*'.
void lesson_12_9()
{
    std::cout << "--- 12.9 pointers and const ---\n";
    // TEST bytes only, NOT a real key. Real keys come from a secure random source.
    std::array<Byte, 4> key{0xDE, 0xAD, 0xBE, 0xEF};

    const Byte* readOnly = key.data();   // .data() = pointer to the first element
    // *readOnly = 0;                    // ERROR: can't modify through pointer-to-const
    readOnly = &key[1];                  // OK: re-pointing is allowed
    std::cout << "  readOnly now sees 0x" << std::hex << static_cast<int>(*readOnly) << "\n";

    Byte* const fixed = &key[0];
    *fixed = 0x00;                       // OK: modify the Byte
    // fixed = &key[1];                  // ERROR: can't re-point a const pointer
    std::cout << "  key[0] after write = 0x" << static_cast<int>(key[0]) << std::dec << "\n\n";
}

// =============================================================================
// 12.10  Pass by address
// =============================================================================
// IN PLAIN WORDS
//   'void f(Byte* data, std::size_t len)' → the caller passes an ADDRESS.
//   Like pass by reference: no copy, and the function can modify the object.
//   Differences: the caller writes &x, and the pointer might be nullptr,
//   so the function must check.
//
//   data[i] on a pointer means "the i-th Byte after the one data points at".
//   (More on that in the arrays chapter.)
//
// WHY A VPN CARES
//   This "pointer + length" pair is how C APIs pass buffers:
//     ssize_t send(int sockfd, const void* buf, size_t len, int flags);
//   You will write wrappers around exactly this.
//
//   Wiping keys: real code uses sodium_memzero() from libsodium, because a
//   plain loop like the one below can be removed by the optimizer if the
//   buffer is never read again.
//
// RULE
//   In C++ code you write, prefer references. Use pointers when talking to
//   C APIs or when "nothing" (nullptr) is a valid input.
void wipe(Byte* data, std::size_t len)
{
    if (!data)               // nullptr? do nothing instead of crashing
        return;
    for (std::size_t i = 0; i < len; ++i)
        data[i] = 0;
}

void lesson_12_10()
{
    std::cout << "--- 12.10 pass by address ---\n";
    std::array<Byte, 4> sessionKey{1, 2, 3, 4};   // TEST bytes, NOT a real key
    wipe(sessionKey.data(), sessionKey.size());
    wipe(nullptr, 100);                            // safe: function checks
    std::cout << "  key after wipe: " << static_cast<int>(sessionKey[0])
              << static_cast<int>(sessionKey[1]) << static_cast<int>(sessionKey[2])
              << static_cast<int>(sessionKey[3]) << "\n\n";   // 0000
}

// =============================================================================
// 12.11  Pass by address (part 2)
// =============================================================================
// IN PLAIN WORDS
//   1) "Optional" arguments: a pointer parameter can mean "maybe".
//      Pass an address to give a value, nullptr to say "none".
//   2) The pointer ITSELF is passed by value (copied). If the function
//      re-points its copy, the caller's pointer doesn't change.
//      To re-point the caller's pointer, pass a reference to it: Byte*&
//   So in a sense, C++ only has pass by value and pass by reference;
//   "pass by address" is really "pass a pointer by value".
//
// WHY A VPN CARES
//   sendTo(data, &peer) to send to a specific peer, sendTo(data, nullptr)
//   to use the default peer.
//
// RULE
//   For optional arguments, also consider std::optional (12.15) or an overload.
void sendTo(const Bytes& data, const PeerId* peer)
{
    if (peer)
        std::cout << "  sending " << data.size() << " bytes to peer " << *peer << "\n";
    else
        std::cout << "  sending " << data.size() << " bytes to the DEFAULT peer\n";
}

void repointCopy(const Byte* p, const Byte* other) { p = other; (void)p; }   // caller unaffected
void repointReal(const Byte*& p, const Byte* other) { p = other; }           // caller changed

void lesson_12_11()
{
    std::cout << "--- 12.11 pass by address part 2 ---\n";
    Bytes data{1, 2, 3};
    PeerId bob = 42;
    sendTo(data, &bob);
    sendTo(data, nullptr);

    const Byte* cursor = &data[0];
    repointCopy(cursor, &data[2]);
    std::cout << "  after repointCopy: *cursor = " << static_cast<int>(*cursor) << "\n";   // 1
    repointReal(cursor, &data[2]);
    std::cout << "  after repointReal: *cursor = " << static_cast<int>(*cursor) << "\n\n"; // 3
}

// =============================================================================
// 12.12  Return by reference and return by address
// =============================================================================
// IN PLAIN WORDS
//   A function can return a reference (or pointer) instead of a copy.
//   DANGER: the object must still exist after the function returns.
//   Local variables die when the function ends. A reference to one is
//   "dangling" = it refers to memory that's gone. Using it is UB.
//
//   Safe to return by reference: static locals, globals, or something the
//   CALLER passed in by reference.
//
//   Gotcha: 'auto x = f();' COPIES even if f returns a reference (10.8).
//
// WHY A VPN CARES
//   "Return a reference to the receive buffer" sounds efficient. If that
//   buffer was a local, you've built a use-after-free: a classic exploit.
//
// RULE
//   Never return a reference or pointer to a local variable.

// const Bytes& makeHeaderBroken()
// {
//     Bytes local{0x01, 0x00};
//     return local;             // DANGLING: 'local' dies right here.
// }                             // (clang warns: reference to stack memory returned)

const Bytes& handshakeTemplate()
{
    static const Bytes kTemplate{0x01, 0x00, 0x00, 0x00};   // static: lives until program ends
    return kTemplate;                                       // safe
}

// Return by address: nullptr means "no such peer".
const Bytes* sessionKeyFor(PeerId peer)
{
    static const Bytes kKeyForPeer7{0xAA, 0xBB};   // TEST bytes, NOT a real key
    if (peer == 7)
        return &kKeyForPeer7;
    return nullptr;
}

void lesson_12_12()
{
    std::cout << "--- 12.12 return by reference / address ---\n";
    const Bytes& tmpl = handshakeTemplate();   // no copy
    auto copy = handshakeTemplate();           // auto drops & → this IS a copy
    std::cout << "  template size " << tmpl.size() << ", same object? " << std::boolalpha
              << (&tmpl == &handshakeTemplate()) << ", copy same object? "
              << (&copy == &handshakeTemplate()) << "\n";

    for (PeerId peer : {PeerId{7}, PeerId{9}})
    {
        const Bytes* key = sessionKeyFor(peer);
        if (key)
            std::cout << "  peer " << peer << " has a " << key->size() << "-byte key\n";
        else
            std::cout << "  peer " << peer << " unknown → drop packet\n";
    }
    // 'key->size()' is shorthand for '(*key).size()'.
    std::cout << "\n";
}

// =============================================================================
// 12.13  In and out parameters
// =============================================================================
// IN PLAIN WORDS
//   "In" parameter  = the function only READS it (by value or const&).
//   "Out" parameter = the function WRITES a result into it (non-const & or *).
//
//   The real POSIX recvfrom uses out-parameters:
//     ssize_t recvfrom(int sockfd,
//                      void* buf, size_t len,             // OUT: bytes land here
//                      int flags,
//                      struct sockaddr* src_addr,         // OUT: who sent it
//                      socklen_t* addrlen);               // IN+OUT: size of src_addr
//   It returns the byte count, or -1 on error. 'addrlen' is "in-out": you
//   pass in the size of your sockaddr; it writes back how much it used.
//   getsockname(sockfd, sockaddr*, socklen_t*) works the same way.
//
//   Below is a SIMULATED recvfrom, so this file runs without a network.
//
// WHY A VPN CARES
//   You'll call recvfrom in your main loop. Knowing which args are in and
//   which are out tells you which ones you must set up before the call.
//
// RULE
//   In your own code, prefer RETURNING results (or a std::optional) over out
//   params. Out params are hard to read at the call site.
//   When you must use them (C APIs), use pointers, so &x at the call site
//   shows "this gets written".
long fakeRecvfrom(Byte* buf, std::size_t len, std::uint32_t* fromIp, std::uint16_t* fromPort)
{
    const std::array<Byte, 5> arrived{'H', 'E', 'L', 'L', 'O'};
    std::size_t n = std::min(len, arrived.size());   // never write past the buffer!
    for (std::size_t i = 0; i < n; ++i)
        buf[i] = arrived[i];
    if (fromIp)
        *fromIp = 0xC0A80002;   // 192.168.0.2
    if (fromPort)
        *fromPort = 51820;
    return static_cast<long>(n);
}

void lesson_12_13()
{
    std::cout << "--- 12.13 in and out parameters ---\n";
    std::array<Byte, 1500> buffer{};
    std::uint32_t ip = 0;
    std::uint16_t port = 0;
    long got = fakeRecvfrom(buffer.data(), buffer.size(), &ip, &port);
    if (got < 0)
    {
        std::cout << "  recv failed\n\n";
        return;
    }
    std::cout << "  got " << got << " bytes from " << (ip >> 24) << '.' << ((ip >> 16) & 0xFF)
              << '.' << ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF) << ':' << port << "\n\n";
}

// =============================================================================
// 12.14  Type deduction with pointers, references, and const
// =============================================================================
// IN PLAIN WORDS
//   Plain 'auto' DROPS references and "top-level" const:
//     const Bytes& r = packet;   auto a = r;   // a is Bytes: a full COPY
//   To keep them, say so:
//     auto& b = r;          // const Bytes&  (const kept because it's needed)
//     const auto& c = r;    // const Bytes&
//   Pointers are KEPT by auto: 'auto p = &x;' is a pointer.
//   'auto*' says "this must be a pointer" (clearer to readers).
//
//   "Top-level const" = the variable itself is const (Byte* const p).
//   "Low-level const" = the thing pointed to is const (const Byte* p).
//   auto drops top-level const, keeps low-level const.
//
// WHY A VPN CARES
//   'auto pkt = queue.front();' silently copies a whole packet. In a hot
//   loop that's a big slowdown nobody notices.
//
// RULE
//   Write 'const auto&' for big things you only read. Write 'auto*' for pointers.
void lesson_12_14()
{
    std::cout << "--- 12.14 type deduction with refs/pointers ---\n";
    Bytes packet{1, 2, 3};
    const Bytes& ref = packet;

    auto copied = ref;         // Bytes (copy!)
    copied.push_back(4);       // changes only the copy
    copied.push_back(5);
    const auto& viewed = ref;  // const Bytes& (no copy)
    auto* ptr = &packet;       // Bytes*

    ptr->push_back(9);         // changes 'packet'
    std::cout << "  packet=" << packet.size() << " copied=" << copied.size()
              << " viewed=" << viewed.size() << "\n\n";   // 4, 5, 4
}

// =============================================================================
// 12.15  std::optional
// =============================================================================
// IN PLAIN WORDS
//   std::optional<T> = a box that holds either a T, or NOTHING (std::nullopt).
//   Perfect return type for "this might fail":
//     if (result)         → did it work?
//     *result             → the value (only if it worked!)
//     result.value_or(x)  → the value, or x if empty
//   *result on an EMPTY optional is UB. result.value() throws instead.
//
// WHY A VPN CARES
//   Every packet is UNTRUSTED. It may be too short, or have a bad type, or
//   be garbage from an attacker. A parser that returns std::optional forces
//   the caller to handle "malformed", instead of reading past the end of the
//   buffer (crash or worse).
//
// RULE
//   Parsers of network data return std::optional (or similar). Never assume
//   a packet is well-formed. Check the length BEFORE reading.

// Our packet: [ type: 1 byte ][ receiver PeerId: 4 bytes, big-endian ] ...
constexpr Byte kTypeData = 0x04;

std::optional<PeerId> parseReceiver(const Bytes& packet)
{
    if (packet.size() < 5)          // too short → don't read past the end
        return std::nullopt;
    if (packet[0] != kTypeData)     // unknown type → reject
        return std::nullopt;
    PeerId id = 0;
    for (std::size_t i = 1; i <= 4; ++i)
        id = (id << 8) | packet[i];
    return id;                      // wraps the PeerId in an optional automatically
}

void lesson_12_15()
{
    std::cout << "--- 12.15 std::optional ---\n";
    const std::array<Bytes, 3> incoming{
        Bytes{0x04, 0x00, 0x00, 0x00, 0x07},   // good: receiver 7
        Bytes{0x04, 0x00},                     // too short
        Bytes{0x99, 0x00, 0x00, 0x00, 0x07},   // bad type
    };
    for (const Bytes& packet : incoming)
    {
        std::optional<PeerId> receiver = parseReceiver(packet);
        if (receiver)
            std::cout << "  packet for peer " << *receiver << "\n";
        else
            std::cout << "  malformed packet (" << packet.size() << " bytes) → dropped\n";
    }
    std::cout << "  value_or on bad packet: " << parseReceiver(Bytes{}).value_or(0) << "\n\n";
}

int main()
{
    lesson_12_1();
    lesson_12_2();
    lesson_12_3();
    lesson_12_4();
    lesson_12_5();
    lesson_12_6();
    lesson_12_7();
    lesson_12_8();
    lesson_12_9();
    lesson_12_10();
    lesson_12_11();
    lesson_12_12();
    lesson_12_13();
    lesson_12_14();
    lesson_12_15();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment makeHeaderBroken() in 12.12 and compile. Read clang's
//      warning. Then explain in one sentence why it's a security bug.
//   2. Write 'std::optional<std::uint16_t> readLength(const Bytes& p,
//      std::size_t offset)' that returns the 2-byte big-endian value at
//      offset, or std::nullopt if offset + 2 > p.size(). Test with a 1-byte
//      packet, a 3-byte packet, and offset 1000.
//   3. Change fakeRecvfrom to take 'std::uint16_t& fromPort' instead of a
//      pointer. Look at the call site. Which version makes it more obvious
//      that 'port' gets written? (Hint: 12.13 RULE.)
//
// CHECK YOURSELF (answer out loud before moving on)
//   - What's the difference between 'const Byte* p' and 'Byte* const p'?
//   - Why does 'auto x = handshakeTemplate();' copy, even though the function
//     returns a reference?
//   - A packet arrives that's 2 bytes long. What stops parseReceiver from
//     reading packet[4]? What would happen without that check?
// =============================================================================
