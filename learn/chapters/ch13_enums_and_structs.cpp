// =============================================================================
// CHAPTER 13 — Compound Types: Enums and Structs
// learncpp.com/cpp-tutorial  (lessons 13.1 – 13.15)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   You can invent your OWN types: an enum gives names to a small set of
//   choices, and a struct glues several values together into one thing.
//
// HOW THIS CONNECTS TO A VPN
//   Every VPN packet starts with a header. In this course our header is:
//
//       [ type: 1 byte ][ length: 2 bytes ][ counter: 8 bytes ][ data ... ]
//
//   The "type" byte is a perfect enum (HandshakeInit, Data, Keepalive...).
//   The whole header is a perfect struct (PacketHeader).
//   But the network is hostile: an attacker can send ANY byte. So this
//   chapter also shows how to turn untrusted bytes into enums and structs
//   SAFELY, and why you must never just memcpy a struct onto the wire.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch13_enums_and_structs.cpp -o ch13 && ./ch13
// =============================================================================

#include <array>
#include <cstddef>      // std::size_t, offsetof
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>      // std::istringstream (pretend keyboard input in 13.5)
#include <string>
#include <string_view>
#include <type_traits>  // std::is_same_v (used to PROVE what CTAD picked in 13.14)
#include <utility>      // std::pair
#include <vector>

// Same nicknames as chapter 10.
using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// =============================================================================
// 13.1  Introduction to program-defined (user-defined) types
// =============================================================================
// IN PLAIN WORDS
//   int, double, bool... are built into C++. A "program-defined type" is one
//   YOU create: an enum, a struct, a class. After you define it, you use it
//   just like int.
//
//   The definition is only a blueprint. It makes no object and uses no memory
//   until you create a variable of that type.
//
// WHY A VPN CARES
//   'int type, int length, long counter' are three loose numbers that can be
//   mixed up. 'PacketHeader header' is ONE thing with a clear meaning.
//
// RULE
//   Name your types with a Capital letter (PacketHeader, MessageType).
//   Define a type before you use it (usually in a header file, later).
void lesson_13_1()
{
    std::cout << "--- 13.1 program-defined types ---\n";
    std::cout << "built-in: int, bool, double.  ours: TunnelState, MessageType, PacketHeader\n\n";
}

// =============================================================================
// 13.2  Unscoped enumerations
// =============================================================================
// IN PLAIN WORDS
//   An "enumeration" (enum) is a type whose values are a short list of names.
//   Each name is an "enumerator".
//
//       enum TunnelState { state_down, state_handshaking, state_up };
//
//   "Unscoped" means the names leak into the surrounding scope: you write
//   'state_up', not 'TunnelState::state_up'. That's why we prefix them with
//   'state_' — otherwise two enums could both have an 'up' and clash.
//
// WHY A VPN CARES
//   A tunnel is always in exactly one state. A name like state_up is much
//   clearer than "2", and the compiler stops you from assigning a random word.
//
// RULE
//   Prefer scoped enums (13.6). If you use unscoped ones, prefix the names.
enum TunnelState
{
    state_down,          // 0
    state_handshaking,   // 1
    state_up,            // 2
};

void lesson_13_2()
{
    std::cout << "--- 13.2 unscoped enums ---\n";
    TunnelState state{state_down};
    std::cout << "start: down? " << std::boolalpha << (state == state_down) << "\n";
    state = state_handshaking;   // a new handshake begins
    state = state_up;            // handshake finished
    std::cout << "now: up? " << (state == state_up) << "\n\n";
}

// =============================================================================
// 13.3  Unscoped enumerator integral conversions
// =============================================================================
// IN PLAIN WORDS
//   Under the hood each enumerator is just an integer (0, 1, 2...).
//   - enum → int: happens IMPLICITLY (by itself). Easy to do by accident.
//   - int → enum: does NOT happen by itself. You need static_cast.
//
//   You can also choose the "underlying type" (the integer type it is
//   stored as):  enum Flag : std::uint8_t { ... };  → exactly 1 byte.
//
// WHY A VPN CARES
//   The type byte arrives from the network as a plain number. Turning that
//   number into an enum with static_cast is easy... and dangerous, because
//   the number might not match ANY enumerator (see 13.6 for the safe way).
//
// RULE
//   Fix the underlying type when the enum lives in a packet. Cast int → enum
//   only after you checked the number is valid.
enum WireFlag : std::uint8_t   // stored in exactly 1 byte
{
    flag_none = 0,
    flag_ack = 1,
};

void lesson_13_3()
{
    std::cout << "--- 13.3 enum <-> integer ---\n";
    int asNumber = state_up;   // enum → int, silently. Compiles with no warning!
    std::cout << "state_up as a number = " << asNumber << "\n";

    // TunnelState s = 1;                        // ERROR: int won't turn into an enum by itself
    auto s = static_cast<TunnelState>(1);        // OK: you asked for it
    std::cout << "static_cast<TunnelState>(1) is handshaking? " << std::boolalpha
              << (s == state_handshaking) << "\n";

    WireFlag f{1};   // OK since C++17: brace-init from a number, because the base type is fixed
    std::cout << "sizeof(WireFlag) = " << sizeof(WireFlag) << " byte, ack? " << (f == flag_ack)
              << "\n\n";
}

// =============================================================================
// 13.4  Converting an enumeration to and from a string
// =============================================================================
// IN PLAIN WORDS
//   C++ can't print an enumerator's NAME by itself. 'std::cout << state_up'
//   prints 2. To get "up" you write a function with a switch.
//   Going the other way (text → enum) can FAIL (the text may be "banana"),
//   so return std::optional: either a value, or "nothing".
//
// WHY A VPN CARES
//   Logs should say "state=up", not "state=2". And config files contain text
//   like "up", which you parse into an enum and must reject if invalid.
//
// RULE
//   enum → string: switch with a 'default' that returns "???".
//   string → enum: return std::optional and handle the "nothing" case.
constexpr std::string_view getName(TunnelState s)
{
    switch (s)
    {
    case state_down:        return "down";
    case state_handshaking: return "handshaking";
    case state_up:          return "up";
    default:                return "???";
    }
}

std::optional<TunnelState> parseTunnelState(std::string_view text)
{
    if (text == "down") return state_down;
    if (text == "handshaking") return state_handshaking;
    if (text == "up") return state_up;
    return std::nullopt;   // "nothing": the text wasn't a valid state
}

void lesson_13_4()
{
    std::cout << "--- 13.4 enum <-> string ---\n";
    std::cout << "getName(state_handshaking) = " << getName(state_handshaking) << "\n";
    for (std::string_view text : {"up", "banana"})
    {
        auto parsed = parseTunnelState(text);
        if (parsed)
            std::cout << "'" << text << "' → " << getName(*parsed) << "\n";
        else
            std::cout << "'" << text << "' → rejected\n";
    }
    std::cout << '\n';
}

// =============================================================================
// 13.5  Introduction to overloading the I/O operators
// =============================================================================
// IN PLAIN WORDS
//   "Overloading" an operator = teaching it what to do with YOUR type.
//   operator<< is the function that runs when you write  std::cout << x.
//   We write one for TunnelState, so 'std::cout << state' prints "up".
//   It returns the stream so you can keep chaining: std::cout << a << b.
//
//   operator>> is the input version (std::cin >> x). If the input is bad,
//   we put the stream into a "failed" state, just like reading "abc" into an int.
//
// WHY A VPN CARES
//   You will print states, message types and headers in logs constantly.
//   One operator<< and every log line becomes readable.
//
// RULE
//   operator<< takes (std::ostream&, const YourType&) and returns the stream.
std::ostream& operator<<(std::ostream& out, TunnelState s)
{
    return out << getName(s);
}

std::istream& operator>>(std::istream& in, TunnelState& s)
{
    std::string word{};
    in >> word;
    if (auto parsed = parseTunnelState(word))
        s = *parsed;
    else
        in.setstate(std::ios_base::failbit);   // tell the caller: bad input
    return in;
}

void lesson_13_5()
{
    std::cout << "--- 13.5 operator<< and operator>> ---\n";
    TunnelState s{state_up};
    std::cout << "state=" << s << "\n";   // uses OUR operator<<

    std::istringstream fakeInput{"handshaking banana"};   // pretend the user typed this
    TunnelState fromUser{state_down};
    fakeInput >> fromUser;
    std::cout << "read 1: " << fromUser << " ok=" << std::boolalpha << !fakeInput.fail() << "\n";
    fakeInput >> fromUser;
    std::cout << "read 2: ok=" << !fakeInput.fail() << " (\"banana\" rejected)\n\n";
}

// =============================================================================
// 13.6  Scoped enumerations (enum classes)
// =============================================================================
// IN PLAIN WORDS
//   'enum class' fixes both problems of unscoped enums:
//     1. Names stay inside: you must write MessageType::Data.
//     2. NO implicit conversion to int. Comparing to a number won't compile.
//   To get the number out, you static_cast on purpose.
//   (C++23 adds std::to_underlying for that; in C++20 we use static_cast.)
//
// WHY A VPN CARES
//   This is our packet type byte (modeled on WireGuard, simplified: real
//   WireGuard uses 1=init, 2=response, 3=cookie reply, 4=data).
//
//   SECURITY: an attacker can send ANY byte, e.g. 200. 'static_cast<MessageType>(200)'
//   compiles fine and gives a MessageType that matches NO name. If code then
//   assumes "it must be one of the four", it misbehaves. So: CHECK FIRST,
//   then cast. That's what messageTypeFromByte() does.
//
// RULE
//   Use enum class by default. Never cast an untrusted number straight into
//   an enum — validate it first.
enum class MessageType : std::uint8_t
{
    HandshakeInit = 1,
    HandshakeResponse,   // 2 (one more than the previous)
    Data,                // 3
    Keepalive,           // 4
};

constexpr std::string_view getName(MessageType t)
{
    using enum MessageType;   // C++20: lets us write 'Data' instead of 'MessageType::Data' here
    switch (t)
    {
    case HandshakeInit:     return "HandshakeInit";
    case HandshakeResponse: return "HandshakeResponse";
    case Data:              return "Data";
    case Keepalive:         return "Keepalive";
    default:                return "???";
    }
}

std::ostream& operator<<(std::ostream& out, MessageType t) { return out << getName(t); }

constexpr Byte toByte(MessageType t) { return static_cast<Byte>(t); }

// The SAFE way to turn a byte from the network into a MessageType.
std::optional<MessageType> messageTypeFromByte(Byte b)
{
    if (b < toByte(MessageType::HandshakeInit) || b > toByte(MessageType::Keepalive))
        return std::nullopt;            // not a known type → reject the packet
    return static_cast<MessageType>(b); // checked, so this cast is now safe
}

void lesson_13_6()
{
    std::cout << "--- 13.6 enum class ---\n";
    MessageType t{MessageType::Data};
    // if (t == 3) {}                        // ERROR: no implicit conversion. Good!
    std::cout << t << " is byte " << static_cast<int>(toByte(t)) << "\n";

    Bytes typeBytesFromNetwork{3, 4, 200};   // 200 = attacker garbage
    for (Byte b : typeBytesFromNetwork)
    {
        auto naive = static_cast<MessageType>(b);   // compiles, but may be nonsense
        auto safe = messageTypeFromByte(b);
        std::cout << "byte " << static_cast<int>(b) << ": naive cast → " << naive
                  << ", safe parse → " << (safe ? getName(*safe) : "REJECTED") << "\n";
    }
    std::cout << '\n';
}

// =============================================================================
// 13.7  Introduction to structs, members, and member selection
// =============================================================================
// IN PLAIN WORDS
//   A struct bundles several variables ("members") into one type.
//   You reach a member with a dot:  header.length   ("member selection").
//
// WHY A VPN CARES
//   A header has a type, a length and a counter. They belong together, so
//   they travel together as one PacketHeader.
//
// RULE
//   Group data that belongs together into a struct. Always initialize it ({}).
struct PacketHeader
{
    MessageType type;
    std::uint16_t length;   // payload length in bytes
    Counter counter;        // per-packet number, for replay protection
};

void lesson_13_7()
{
    std::cout << "--- 13.7 structs ---\n";
    PacketHeader header{};   // {} = every member set to zero. Never leave it as garbage.
    header.type = MessageType::Data;
    header.length = 64;
    header.counter = 1;
    std::cout << "type=" << header.type << " length=" << header.length
              << " counter=" << header.counter << "\n\n";
}

// =============================================================================
// 13.8  Struct aggregate initialization
// =============================================================================
// IN PLAIN WORDS
//   A simple struct like PacketHeader is an "aggregate": just public data,
//   no constructors. You can fill it in one go with braces, in member order:
//       PacketHeader h{MessageType::Data, 64, 1};
//   C++20 adds "designated initializers", where you write the names:
//       PacketHeader h{.type = MessageType::Data, .length = 64, .counter = 1};
//   Braces still refuse narrowing (chapter 10.4).
//
// WHY A VPN CARES
//   Designated init makes it obvious which number is the length and which is
//   the counter. Swapping them by accident is a real bug.
//
// RULE
//   Use designated initializers when members could be confused.
//   They must be written in the same order as the struct declares them.
void lesson_13_8()
{
    std::cout << "--- 13.8 aggregate init ---\n";
    PacketHeader a{MessageType::Data, 64, 1};
    PacketHeader b{.type = MessageType::Keepalive, .length = 0, .counter = 2};
    const PacketHeader c{.type = MessageType::HandshakeInit, .length = 148, .counter = 0};
    // c.counter = 5;                                    // ERROR: c is const
    // PacketHeader d{MessageType::Data, 70000, 3};      // ERROR: 70000 doesn't fit in uint16_t
    // PacketHeader e{.length = 64, .type = MessageType::Data, .counter = 3};
    //   ^ wrong order: not valid C++. GCC rejects it; clang only warns (-Wreorder-init-list).
    // PacketHeader f{MessageType::Data};   // compiles: missing members become 0, but -Wextra
    //                                      // warns "missing field initializer". Be explicit.

    a = {MessageType::Data, 128, 3};   // assign a whole new set of values at once
    std::cout << "a: " << a.type << "/" << a.length << "/" << a.counter << "\n";
    std::cout << "b: " << b.type << "/" << b.length << "/" << b.counter << "\n";
    std::cout << "c: " << c.type << "/" << c.length << "/" << c.counter << "\n\n";
}

// =============================================================================
// 13.9  Default member initialization
// =============================================================================
// IN PLAIN WORDS
//   You can give each member a default value right in the struct:
//       std::uint16_t listenPort{51820};
//   Any member you don't set yourself gets that default instead of garbage.
//
// WHY A VPN CARES
//   Config structs have sensible defaults (WireGuard listens on 51820, uses
//   MTU 1420, keepalive every 25 s is common). The user overrides only what
//   they need, and nothing is ever left uninitialized.
//
// RULE
//   Give every member a default. Uninitialized memory is a classic leak/bug source.
struct TunnelConfig
{
    std::uint16_t listenPort{51820};
    std::uint16_t mtu{1420};
    int keepaliveSeconds{25};
};

void lesson_13_9()
{
    std::cout << "--- 13.9 default member init ---\n";
    TunnelConfig defaults{};                  // all defaults
    TunnelConfig custom{.listenPort = 4500};  // change one, keep the rest
    std::cout << "defaults: port " << defaults.listenPort << ", mtu " << defaults.mtu
              << ", keepalive " << defaults.keepaliveSeconds << "s\n";
    std::cout << "custom:   port " << custom.listenPort << ", mtu " << custom.mtu
              << ", keepalive " << custom.keepaliveSeconds << "s\n\n";
}

// =============================================================================
// 13.10  Passing and returning structs
// =============================================================================
// IN PLAIN WORDS
//   - Pass a struct INTO a function by const reference (const T&): no copy,
//     and the function can't change it.
//   - Return a struct OUT of a function by value. It's cheap (the compiler
//     builds it right where it's needed).
//   - 'return {a, b, c};' builds the return value from braces.
//
// RULE
//   In: const PacketHeader&.   Out: PacketHeader (by value).
void printHeader(const PacketHeader& h)
{
    std::cout << "[" << h.type << " len=" << h.length << " ctr=" << h.counter << "]\n";
}

PacketHeader makeKeepalive(Counter counter)
{
    return {MessageType::Keepalive, 0, counter};   // keepalives carry no payload
}

void lesson_13_10()
{
    std::cout << "--- 13.10 passing/returning structs ---\n";
    printHeader(makeKeepalive(42));
    printHeader({MessageType::Data, 512, 43});   // a temporary struct, built on the spot
    std::cout << '\n';
}

// =============================================================================
// 13.11  Struct miscellany  (nesting, size, PADDING, and the wire)
// =============================================================================
// IN PLAIN WORDS
//   1. Structs can contain other structs (PeerEntry contains an Endpoint).
//   2. sizeof(struct) is often BIGGER than the sum of its members.
//      The CPU likes each value at an address that's a multiple of its size
//      ("alignment"): an 8-byte counter wants an address divisible by 8.
//      So the compiler inserts hidden unused bytes called "padding".
//
//      PacketHeader on this machine (macOS, arm64):
//        offset 0  type     (1 byte)
//        offset 1  -- padding (1 byte) --
//        offset 2  length   (2 bytes)
//        offset 4  -- padding (4 bytes) --
//        offset 8  counter  (8 bytes)
//        total: 16 bytes, but only 11 bytes are real data.
//      Other compilers/CPUs CAN lay it out differently.
//
// WHY A VPN CARES  (important!)
//   Tempting shortcut:  memcpy(&wireBuffer, &header, sizeof header);  DON'T.
//     - Padding: you'd send 5 junk bytes. Those bytes may hold leftover
//       memory — maybe pieces of a key. That's an information leak.
//     - Layout: the other side's compiler may pad differently.
//     - Endianness: the order of bytes inside a number. x86 and ARM store
//       the LOWEST byte first ("little-endian"); the network convention is
//       highest byte first ("big-endian" / network byte order).
//   So you SERIALIZE (turn into bytes) field by field, in an order you chose,
//   and PARSE (turn bytes back) field by field, checking everything.
//
// RULE
//   A struct is for your program. The wire format is a separate, written-down
//   list of bytes. Convert between them by hand.
struct Endpoint
{
    std::array<Byte, 4> ipv4{};
    std::uint16_t port{51820};
};

struct PeerEntry
{
    PeerId id{};
    Endpoint endpoint{};   // a struct inside a struct
};

constexpr std::size_t kWireHeaderSize = 1 + 2 + 8;   // 11: what we actually send

Bytes serializeHeader(const PacketHeader& h)
{
    Bytes out{};
    out.reserve(kWireHeaderSize);
    out.push_back(toByte(h.type));
    out.push_back(static_cast<Byte>(h.length >> 8));     // big-endian: high byte first
    out.push_back(static_cast<Byte>(h.length & 0xFF));
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<Byte>(h.counter >> shift));
    return out;
}

std::optional<PacketHeader> parseHeader(const Bytes& in)
{
    if (in.size() < kWireHeaderSize) return std::nullopt;   // too short: reject
    auto type = messageTypeFromByte(in[0]);                // validate the enum (13.6)
    if (!type) return std::nullopt;
    auto length = static_cast<std::uint16_t>((in[1] << 8) | in[2]);
    Counter counter{0};
    for (std::size_t i = 3; i < kWireHeaderSize; ++i)
        counter = (counter << 8) | in[i];
    return PacketHeader{*type, length, counter};
}

void lesson_13_11()
{
    std::cout << "--- 13.11 nesting, padding, serialization ---\n";
    PeerEntry peer{7, {{10, 0, 0, 2}, 51820}};
    std::cout << "peer " << peer.id << " at 10.0.0." << static_cast<int>(peer.endpoint.ipv4[3])
              << ":" << peer.endpoint.port << "\n";

    std::cout << "sum of field sizes = " << sizeof(MessageType) + sizeof(std::uint16_t) + sizeof(Counter)
              << ", sizeof(PacketHeader) = " << sizeof(PacketHeader) << "\n";
    std::cout << "offsets: type " << offsetof(PacketHeader, type) << ", length "
              << offsetof(PacketHeader, length) << ", counter " << offsetof(PacketHeader, counter)
              << "\n";

    PacketHeader h{MessageType::Data, 0x0102, 5};
    Bytes wire = serializeHeader(h);
    std::cout << "on the wire (" << wire.size() << " bytes):";
    for (Byte b : wire) std::cout << ' ' << static_cast<int>(b);
    std::cout << "\n";

    if (auto back = parseHeader(wire))
    {
        std::cout << "parsed back: ";
        printHeader(*back);
    }
    Bytes evil{200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};   // bad type byte
    std::cout << "evil packet parses? " << std::boolalpha << parseHeader(evil).has_value() << "\n\n";
}

// =============================================================================
// 13.12  Member selection with pointers and references
// =============================================================================
// IN PLAIN WORDS
//   - With a reference, use a dot, same as the object:     ref.counter
//   - With a pointer, use an arrow:                         ptr->counter
//     ptr->counter is just a shorter way to write (*ptr).counter.
//   - Arrows and dots chain:  peerPtr->endpoint.port
//
// WHY A VPN CARES
//   C networking APIs hand you pointers all the time (sockaddr*, buffers).
//   And a pointer can be null: check it before using ->.
//
// RULE
//   Prefer references. If you take a pointer, check for nullptr first.
void bumpCounter(PacketHeader* hdr)
{
    if (hdr == nullptr) return;   // never use -> on a null pointer
    ++hdr->counter;
}

void bumpCounterRef(PacketHeader& hdr) { ++hdr.counter; }

void lesson_13_12()
{
    std::cout << "--- 13.12 -> and . ---\n";
    PacketHeader h{MessageType::Data, 64, 10};
    bumpCounter(&h);       // 11
    bumpCounterRef(h);     // 12
    bumpCounter(nullptr);  // safely ignored
    std::cout << "counter after two bumps = " << h.counter << "\n";

    PeerEntry peer{9, {{192, 168, 1, 5}, 51820}};
    PeerEntry* p = &peer;
    p->endpoint.port = 51821;   // arrow to reach the member, then dot into the nested struct
    std::cout << "peer " << p->id << " port now " << peer.endpoint.port << "\n\n";
}

// =============================================================================
// 13.13  Class templates
// =============================================================================
// IN PLAIN WORDS
//   A function template (chapter 11) writes one function for many types.
//   A CLASS template does the same for a struct/class:
//       template <typename T> struct Pair { T first{}; T second{}; };
//   Pair<Counter> and Pair<std::uint16_t> are two different real types the
//   compiler stamps out for you. T is a placeholder for a type.
//
// WHY A VPN CARES
//   Lots of VPN data comes in pairs of the same type: (bytes sent, bytes
//   received), (min port, max port), (old key, new key) during a rekey.
//
// RULE
//   Reach for a template when the SAME shape repeats with different types.
//   (The standard library already has std::pair; this is to learn how.)
template <typename T>
struct Pair
{
    T first{};
    T second{};
};

template <typename T>
constexpr T larger(const Pair<T>& p)   // works for any Pair<T>
{
    return (p.first > p.second) ? p.first : p.second;
}

void lesson_13_13()
{
    std::cout << "--- 13.13 class templates ---\n";
    Pair<Counter> traffic{1'200'000, 950'000};   // bytes sent, bytes received
    Pair<std::uint16_t> portRange{51820, 51830};
    std::cout << "traffic sent=" << traffic.first << " recv=" << traffic.second
              << " larger=" << larger(traffic) << "\n";
    std::cout << "ports " << portRange.first << "-" << portRange.second << "\n\n";
}

// =============================================================================
// 13.14  Class template argument deduction (CTAD) and deduction guides
// =============================================================================
// IN PLAIN WORDS
//   CTAD = the compiler figures out T from the values you give:
//       Pair p{1, 2};   → Pair<int>,  no <int> needed.
//   In C++17, aggregates like Pair needed a "deduction guide" — a one-line
//   hint that says "Pair(T, T) means Pair<T>". C++20 does it automatically,
//   but you will still see guides in real code, so here's one.
//
// WHY A VPN CARES
//   Less typing, but be careful: Pair p{0, 0} gives Pair<int>, NOT a pair of
//   uint64 counters. When the exact width matters (it always does on the
//   wire), write the type yourself.
//
// RULE
//   CTAD is fine when the type is obvious. For wire-format types, be explicit.
template <typename T>
Pair(T, T) -> Pair<T>;   // deduction guide (needed in C++17, harmless in C++20)

void lesson_13_14()
{
    std::cout << "--- 13.14 CTAD ---\n";
    Pair p{1, 2};                            // deduced Pair<int>
    Pair counters{Counter{5}, Counter{9}};   // deduced Pair<Counter>
    std::pair peerAndCounter{PeerId{7}, Counter{42}};   // std::pair<PeerId, Counter>
    // Pair mixed{1, 2.5};   // ERROR: is T int or double? The compiler won't guess.

    static_assert(std::is_same_v<decltype(p), Pair<int>>);
    static_assert(std::is_same_v<decltype(counters), Pair<Counter>>);
    std::cout << "p is Pair<int>, larger=" << larger(p) << "; counters larger=" << larger(counters)
              << "; peer " << peerAndCounter.first << " ctr " << peerAndCounter.second << "\n\n";
}

// =============================================================================
// 13.15  Alias templates
// =============================================================================
// IN PLAIN WORDS
//   A normal alias (chapter 10.7) names ONE type. An alias template names a
//   whole family of types, with a blank to fill in:
//       template <std::size_t N> using ByteArray = std::array<Byte, N>;
//   Now ByteArray<32> means std::array<Byte, 32>.
//
// WHY A VPN CARES
//   Crypto uses fixed-size byte blobs everywhere. ChaCha20-Poly1305 (the
//   cipher WireGuard uses) takes a 32-byte key and a 12-byte nonce
//   ("number used once" — must NEVER repeat with the same key).
//   Key and Nonce read like the spec. std::array<std::uint8_t, 32> doesn't.
//
// RULE
//   Use alias templates to give meaning to families of types.
//   (Still just nicknames: a Key and a ByteArray<32> are the same type.)
template <std::size_t N>
using ByteArray = std::array<Byte, N>;

using Key = ByteArray<32>;
using Nonce = ByteArray<12>;

template <typename T>
using Range = Pair<T>;   // alias templates can wrap our own templates too

void lesson_13_15()
{
    std::cout << "--- 13.15 alias templates ---\n";
    Key key{};       // all zeros. A real key comes from a secure random source, never zeros!
    Nonce nonce{};
    Range<std::uint16_t> ports{51820, 51830};
    std::cout << "Key is " << key.size() << " bytes, Nonce is " << nonce.size()
              << " bytes, port range " << ports.first << "-" << ports.second << "\n\n";
}

int main()
{
    lesson_13_1();
    lesson_13_2();
    lesson_13_3();
    lesson_13_4();
    lesson_13_5();
    lesson_13_6();
    lesson_13_7();
    lesson_13_8();
    lesson_13_9();
    lesson_13_10();
    lesson_13_11();
    lesson_13_12();
    lesson_13_13();
    lesson_13_14();
    lesson_13_15();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Add 'CookieReply' to MessageType between HandshakeResponse and Data
//      (like real WireGuard). What breaks in messageTypeFromByte()? Fix it so
//      it can't silently go wrong next time someone adds a type.
//   2. Reorder PacketHeader's members (counter first, then length, then type).
//      Print sizeof again. Does it shrink? Why or why not? (Hint: the whole
//      struct is rounded up to a multiple of its biggest member's alignment.)
//   3. Write 'Bytes serializePeer(const PeerEntry&)' and
//      'std::optional<PeerEntry> parsePeer(const Bytes&)'. Feed parsePeer a
//      too-short vector and check it returns nullopt.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why is 'static_cast<MessageType>(byteFromNetwork)' dangerous on its own?
//   - Name two reasons you must not memcpy a struct straight onto the wire.
//   - What's the difference between enum and enum class? Which do you default to?
// =============================================================================
