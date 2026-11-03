// =============================================================================
// CHAPTER 15 — More on Classes
// learncpp.com/cpp-tutorial  (lessons 15.1 – 15.10)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   Classes can clean up after themselves (destructors), share data across
//   all objects (static), let trusted outsiders in (friends), and know
//   whether they're being called on a temporary (ref qualifiers).
//
// HOW THIS CONNECTS TO A VPN
//   This chapter has the single most security-relevant class feature so far:
//   the DESTRUCTOR. When a session ends, its secret key must be wiped from
//   memory, not just "freed". A destructor runs automatically when the
//   object dies, so the wipe can't be forgotten.
//   You'll also build a PacketBuilder with chained calls, a fixed-size
//   packet buffer template, a session-ID allocator, and safe factories.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch15_more_on_classes.cpp -o ch15 && ./ch15
// =============================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>   // std::move
#include <vector>

// Same nicknames as chapter 10.
using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;
using Key = std::array<Byte, 32>;

enum class MessageType : std::uint8_t   // from chapter 13.6
{
    HandshakeInit = 1,
    HandshakeResponse,
    Data,
    Keepalive,
};

constexpr Byte toByte(MessageType t) { return static_cast<Byte>(t); }

void printBytes(const Bytes& bytes)
{
    for (Byte b : bytes) std::cout << ' ' << static_cast<int>(b);
    std::cout << "\n";
}

// =============================================================================
// 15.1  The hidden "this" pointer and member function chaining
// =============================================================================
// IN PLAIN WORDS
//   Inside a member function, 'this' is a pointer to the object the function
//   was called on. When you write m_counter, C++ really means this->m_counter.
//   *this is the object itself.
//
//   "Chaining": if a member function returns *this by reference, you can
//   call the next function on the result:  b.type(...).counter(...).build();
//
// WHY A VPN CARES
//   A packet has many fields. A builder with chained calls reads like a
//   sentence and makes it hard to forget a field.
//
// RULE
//   Return *this by reference (ClassName&) from setters you want to chain.
class PacketBuilder
{
public:
    PacketBuilder& type(MessageType t)
    {
        this->m_type = t;   // 'this->' is optional; written here so you can see it
        return *this;       // return the object itself, so the next call can chain
    }
    PacketBuilder& counter(Counter c)
    {
        m_counter = c;
        return *this;
    }
    PacketBuilder& payload(const Bytes& data)
    {
        m_payload = data;
        return *this;
    }

    // Serialize field by field, big-endian (see chapter 13.11 — never memcpy a struct).
    Bytes build() const
    {
        Bytes out{toByte(m_type)};
        auto length = static_cast<std::uint16_t>(m_payload.size());
        out.push_back(static_cast<Byte>(length >> 8));
        out.push_back(static_cast<Byte>(length & 0xFF));
        for (int shift = 56; shift >= 0; shift -= 8)
            out.push_back(static_cast<Byte>(m_counter >> shift));
        out.insert(out.end(), m_payload.begin(), m_payload.end());
        return out;
    }

private:
    MessageType m_type{MessageType::Data};
    Counter m_counter{0};
    Bytes m_payload{};
};

void lesson_15_1()
{
    std::cout << "--- 15.1 this + chaining ---\n";
    Bytes packet = PacketBuilder{}.type(MessageType::Data).counter(7).payload({0xAA, 0xBB}).build();
    std::cout << "built " << packet.size() << " bytes:";
    printBytes(packet);
    std::cout << '\n';
}

// =============================================================================
// 15.2  Classes and header files
// =============================================================================
// IN PLAIN WORDS
//   In a real project, a class is split into two files:
//     session.h    — the class DEFINITION: members + function declarations.
//                    Anyone who uses Session #includes this.
//     session.cpp  — the member function BODIES, written as Session::name().
//
//   ---- session.h ----
//     #pragma once                       // include this file only once
//     #include <cstdint>
//     class Session {
//     public:
//         explicit Session(std::uint32_t id);
//         std::uint32_t id() const { return m_id; }   // tiny: fine to define here
//         void close();                               // declared only
//     private:
//         std::uint32_t m_id{};
//     };
//
//   ---- session.cpp ----
//     #include "session.h"
//     Session::Session(std::uint32_t id) : m_id{id} {}
//     void Session::close() { /* wipe keys, notify peer... */ }
//
//   Functions defined INSIDE the class body are automatically "inline", so
//   they're allowed in a header. Templates (15.5) usually live fully in headers.
//
// WHY A VPN CARES
//   Your VPN will have tunnel.h, peer.h, crypto.h... Small headers mean
//   faster builds and a clear list of what each part offers.
//
// RULE
//   Definition in .h, non-trivial bodies in .cpp. This file keeps everything
//   in one place only because it's a lesson.
void lesson_15_2()
{
    std::cout << "--- 15.2 classes and headers ---\n";
    std::cout << "(see the comments: session.h holds the class, session.cpp the bodies)\n\n";
}

// =============================================================================
// 15.3  Nested types (member types)
// =============================================================================
// IN PLAIN WORDS
//   A class can define types inside itself: an enum, an alias, even another
//   class. Outside, you name them with the class:  Session::State::Up.
//
// WHY A VPN CARES
//   "State" means nothing on its own. Session::State says exactly whose
//   state it is, and doesn't clash with Tunnel::State or Peer::State.
//
// RULE
//   If a type only makes sense as part of a class, nest it. Put nested types
//   at the top of the class (they must be declared before they're used).
class Session
{
public:
    enum class State
    {
        Down,
        Handshaking,
        Up,
    };
    using Id = std::uint32_t;   // nested alias

    explicit Session(Id id) : m_id{id} {}

    void startHandshake()
    {
        if (m_state == State::Down) m_state = State::Handshaking;
    }
    void handshakeDone()
    {
        if (m_state == State::Handshaking) m_state = State::Up;   // can't jump from Down to Up
    }
    State state() const { return m_state; }
    Id id() const { return m_id; }

private:
    Id m_id{};
    State m_state{State::Down};
};

std::string_view getName(Session::State s)
{
    switch (s)
    {
    case Session::State::Down:        return "Down";
    case Session::State::Handshaking: return "Handshaking";
    case Session::State::Up:          return "Up";
    default:                          return "???";
    }
}

void lesson_15_3()
{
    std::cout << "--- 15.3 nested types ---\n";
    Session s{Session::Id{1}};
    s.handshakeDone();   // ignored: no handshake started yet
    std::cout << "after early handshakeDone: " << getName(s.state()) << "\n";
    s.startHandshake();
    s.handshakeDone();
    std::cout << "after real handshake:      " << getName(s.state()) << "\n\n";
}

// =============================================================================
// 15.4  Introduction to destructors
// =============================================================================
// IN PLAIN WORDS
//   A "destructor" runs AUTOMATICALLY when an object dies (goes out of
//   scope, or is deleted). Name: ~ClassName(). No arguments, no return.
//   It's where the object cleans up after itself.
//
// WHY A VPN CARES  (important!)
//   When an object dies, its memory is just marked "free". The key bytes
//   are STILL THERE until something overwrites them. They can leak via:
//     - a core dump (the OS saves memory to a file when a program crashes)
//     - swap (the OS writes memory to disk when RAM is full)
//     - a later bug that reads freed memory
//   So the destructor must WIPE the key (overwrite it with zeros).
//
//   Gotcha: a plain loop or memset that zeroes memory nobody reads again
//   can be REMOVED by the optimizer ("dead store elimination"). Writing
//   through a 'volatile' pointer tells the compiler "every write here
//   matters, don't skip it". It's a common trick, not a perfect guarantee.
//   Real code uses sodium_memzero() (libsodium) or explicit_bzero().
//
// RULE
//   Any class that holds a secret wipes it in its destructor. And it should
//   not be copyable (14.14), so there's exactly one copy to wipe.
void secureWipe(Byte* data, std::size_t len)
{
    volatile Byte* p = data;   // volatile: the compiler must perform every write
    for (std::size_t i = 0; i < len; ++i) p[i] = 0;
}

class SecretKey
{
public:
    explicit SecretKey(const Key& bytes) : m_bytes{bytes} {}
    ~SecretKey()
    {
        secureWipe(m_bytes.data(), m_bytes.size());
        std::cout << "  ~SecretKey: key wiped\n";
    }
    SecretKey(const SecretKey&) = delete;              // one copy only (14.14)
    SecretKey& operator=(const SecretKey&) = delete;

    void wipeNow() { secureWipe(m_bytes.data(), m_bytes.size()); }   // e.g. on rekey
    bool isWiped() const
    {
        for (Byte b : m_bytes)
            if (b != 0) return false;
        return true;
    }

private:
    Key m_bytes{};
};

void lesson_15_4()
{
    std::cout << "--- 15.4 destructors ---\n";
    Key fake{};
    fake.fill(0x42);   // pretend key. Real keys come from a secure random source.
    {
        SecretKey key{fake};
        std::cout << "  inside scope, wiped? " << std::boolalpha << key.isWiped() << "\n";
        key.wipeNow();
        std::cout << "  after wipeNow, wiped? " << key.isWiped() << "\n";
    }   // ← destructor runs here, automatically
    secureWipe(fake.data(), fake.size());   // our local copy of the bytes needs wiping too!
    std::cout << "  scope ended\n\n";
}

// =============================================================================
// 15.5  Class templates with member functions
// =============================================================================
// IN PLAIN WORDS
//   A class template (13.13) can have member functions. If you define one
//   OUTSIDE the class, you must repeat the template line:
//       template <std::size_t N>
//       bool FixedBuffer<N>::append(const Bytes& bytes) { ... }
//   N here is a value, not a type ("non-type template parameter").
//
// WHY A VPN CARES
//   A packet buffer with a fixed max size (like MTU) never reallocates and
//   can't grow without limit. append() refuses data that would overflow it,
//   instead of writing past the end (a buffer overflow).
//
// RULE
//   Keep template member definitions in the header (the compiler needs the
//   body to stamp out each FixedBuffer<N>).
template <std::size_t N>
class FixedBuffer
{
public:
    bool append(const Bytes& bytes);   // defined below the class
    std::size_t size() const { return m_size; }
    std::size_t capacity() const { return N; }

private:
    std::array<Byte, N> m_data{};
    std::size_t m_size{0};
};

template <std::size_t N>
bool FixedBuffer<N>::append(const Bytes& bytes)
{
    // Written as 'bytes.size() > N - m_size' (not 'm_size + bytes.size() > N')
    // so the math can never overflow. m_size <= N is always true (invariant).
    if (bytes.size() > N - m_size) return false;
    for (Byte b : bytes) m_data[m_size++] = b;
    return true;
}

void lesson_15_5()
{
    std::cout << "--- 15.5 template member functions ---\n";
    FixedBuffer<16> buf{};
    bool first = buf.append(Bytes(11, 0));    // an 11-byte header fits
    bool second = buf.append(Bytes(10, 0));   // 11 + 10 > 16 → refused
    std::cout << std::boolalpha << "append 11: " << first << ", append 10 more: " << second
              << ", size " << buf.size() << "/" << buf.capacity() << "\n\n";
}

// =============================================================================
// 15.6  Static member variables
// =============================================================================
// IN PLAIN WORDS
//   A normal member belongs to ONE object. A 'static' member belongs to the
//   CLASS: there's one copy, shared by all objects (it exists even with zero
//   objects). 'inline static' lets you initialize it right in the class.
//
// WHY A VPN CARES
//   "How many sessions are alive right now?" and "what ID do I give the
//   next session?" are questions about ALL sessions, not one.
//   (Note: real WireGuard picks RANDOM 32-bit session indexes, because
//   sequential IDs are easy for an attacker to guess. Sequential is fine
//   for learning.)
//
// RULE
//   Use static members for data shared by the whole class. Keep them private
//   (s_liveCount is public here only so you can see it; 15.7 fixes that).
class Connection
{
public:
    Connection() : m_id{s_nextId++} { ++s_liveCount; }
    ~Connection() { --s_liveCount; }
    Connection(const Connection&) = delete;   // a copy would break the count
    Connection& operator=(const Connection&) = delete;

    std::uint32_t id() const { return m_id; }

    inline static int s_liveCount{0};   // ONE counter, shared by every Connection

private:
    inline static std::uint32_t s_nextId{1};
    std::uint32_t m_id{};
};

void lesson_15_6()
{
    std::cout << "--- 15.6 static member variables ---\n";
    std::cout << "live before: " << Connection::s_liveCount << "\n";
    {
        Connection a{};
        Connection b{};
        std::cout << "ids " << a.id() << ", " << b.id() << "; live: " << Connection::s_liveCount
                  << "\n";
    }   // both destructors run
    std::cout << "live after scope: " << Connection::s_liveCount << "\n\n";
}

// =============================================================================
// 15.7  Static member functions
// =============================================================================
// IN PLAIN WORDS
//   A static member function belongs to the class, not an object. Call it
//   as  Handshake::create(...)  — no object needed. It has no 'this'.
//
//   Classic use: a "factory". Make the constructor private, and offer a
//   static create() that CHECKS the input first. If the input is bad, it
//   returns std::nullopt, so an invalid object can never exist.
//
// WHY A VPN CARES
//   Handshake messages come from the network (untrusted). Only a message
//   that passes validation should become a Handshake object.
//
// RULE
//   Use a static factory when creating an object can fail. Use static
//   getters to expose private static data read-only.
class Handshake
{
public:
    static std::optional<Handshake> create(PeerId peer, const Bytes& message)
    {
        if (message.size() < 4) return std::nullopt;                                // too short
        if (message[0] != toByte(MessageType::HandshakeInit)) return std::nullopt;  // wrong type
        ++s_accepted;
        return Handshake{peer};   // create() is a member, so it may use the private constructor
    }
    static int acceptedCount() { return s_accepted; }   // read-only view of static data

    PeerId peer() const { return m_peer; }

private:
    explicit Handshake(PeerId peer) : m_peer{peer} {}   // private: only create() can call it

    PeerId m_peer{};
    inline static int s_accepted{0};
};

void lesson_15_7()
{
    std::cout << "--- 15.7 static member functions ---\n";
    // Handshake h{7};   // ERROR: constructor is private. Must go through create().
    auto good = Handshake::create(7, {1, 0, 0, 0});
    auto bad = Handshake::create(8, {200, 0, 0, 0});   // attacker's garbage type byte
    std::cout << std::boolalpha << "good accepted? " << good.has_value()
              << " (peer " << (good ? good->peer() : 0) << "), bad accepted? " << bad.has_value()
              << ", total accepted: " << Handshake::acceptedCount() << "\n\n";
}

// =============================================================================
// 15.8  Friend non-member functions
// =============================================================================
// IN PLAIN WORDS
//   A "friend" is a function that is NOT a member, but is allowed to see
//   the class's private members. The class itself grants it:
//       friend std::ostream& operator<<(std::ostream&, const PeerStats&);
//   operator<< can't be a member (the stream is on the left), so friend is
//   the usual way to let it print private data.
//
// WHY A VPN CARES
//   You want readable logs of traffic counters without adding a public
//   getter for every counter.
//
// RULE
//   A friend is part of the class's interface. Grant it sparingly, and never
//   to something that prints secrets.
class PeerStats
{
public:
    explicit PeerStats(PeerId peer) : m_peer{peer} {}
    void recordSent(std::size_t bytes) { m_bytesSent += bytes; }
    void recordReceived(std::size_t bytes) { m_bytesReceived += bytes; }

    friend std::ostream& operator<<(std::ostream& out, const PeerStats& s)
    {
        return out << "peer " << s.m_peer << " sent=" << s.m_bytesSent
                   << " recv=" << s.m_bytesReceived;   // can read private members
    }

private:
    PeerId m_peer{};
    std::size_t m_bytesSent{0};
    std::size_t m_bytesReceived{0};
};

void lesson_15_8()
{
    std::cout << "--- 15.8 friend functions ---\n";
    PeerStats stats{7};
    stats.recordSent(1420);
    stats.recordReceived(96);
    std::cout << stats << "\n\n";
}

// =============================================================================
// 15.9  Friend classes and friend member functions
// =============================================================================
// IN PLAIN WORDS
//   'friend class Tunnel;' inside CipherState lets EVERY member function of
//   Tunnel touch CipherState's private members.
//   You can be narrower and befriend just one function of another class:
//       friend void Tunnel::rekey(CipherState&, const Key&);
//   (that needs Tunnel's full definition to appear first, which is fiddly).
//
// WHY A VPN CARES
//   Resetting a send counter to 0 is normally FORBIDDEN (14.8). It's only
//   safe together with a brand-new key. CipherState has no public way to do
//   that; only its trusted friend Tunnel can, in rekey(), which always does
//   both at once.
//
//   TRADE-OFF: a friend can break your invariants too. Every friend is one
//   more place to review. If a public member function can do the job
//   safely (e.g. CipherState::rekey(newKey)), prefer that.
//
// RULE
//   Use friend classes rarely, for tightly coupled pairs. Prefer a public,
//   invariant-keeping member function when you can.
class Tunnel;   // forward declaration: "a class called Tunnel exists"

class CipherState
{
public:
    std::optional<Counter> nextCounter()
    {
        if (m_sendCounter == kRekeyAfter) return std::nullopt;   // must rekey
        return m_sendCounter++;
    }
    std::uint32_t generation() const { return m_generation; }

    friend class Tunnel;   // Tunnel may touch the private parts below

private:
    static constexpr Counter kRekeyAfter{2};   // tiny, for the demo
    Key m_key{};
    Counter m_sendCounter{0};
    std::uint32_t m_generation{0};   // which key this is (1st, 2nd, ...)
};

class Tunnel
{
public:
    void rekey(CipherState& cs, const Key& newKey)
    {
        cs.m_key = newKey;      // private access, allowed because we're a friend
        cs.m_sendCounter = 0;   // safe ONLY because the key changed on the line above
        ++cs.m_generation;
    }
};

void lesson_15_9()
{
    std::cout << "--- 15.9 friend classes ---\n";
    CipherState cs{};
    Tunnel tunnel{};
    Key newKey{};
    newKey.fill(0x11);   // pretend key; real ones come from the handshake
    for (int i = 0; i < 4; ++i)
    {
        if (auto c = cs.nextCounter())
        {
            std::cout << "key gen " << cs.generation() << ", counter " << *c << "\n";
        }
        else
        {
            std::cout << "exhausted → tunnel rekeys\n";
            tunnel.rekey(cs, newKey);
        }
    }
    secureWipe(newKey.data(), newKey.size());
    std::cout << '\n';
}

// =============================================================================
// 15.10  Ref qualifiers
// =============================================================================
// IN PLAIN WORDS
//   An "lvalue" is an object with a name that sticks around (peer).
//   An "rvalue" is a temporary that dies at the end of the line (Peer{...}).
//   Ref qualifiers let a member function behave differently for each:
//       const std::string& name() const &;   // called on a named object
//       std::string        name() &&;        // called on a temporary
//   On a named object: return a cheap const reference (the object lives on).
//   On a temporary: return BY VALUE (move the string out), because a
//   reference into a dying temporary would dangle (14.7).
//
// WHY A VPN CARES
//   for (char c : makePeer().name())  — with a plain const& getter, that
//   reference points into a temporary that's already gone. Undefined
//   behavior. The && overload makes it safe.
//
// RULE
//   Most code doesn't need ref qualifiers. Use them for getters that return
//   references, when callers might call them on temporaries.
class NamedPeer
{
public:
    explicit NamedPeer(std::string_view name) : m_name{name} {}

    const std::string& name() const & { return m_name; }   // lvalue: no copy
    std::string name() && { return std::move(m_name); }    // rvalue: take it, safely

private:
    std::string m_name{};
};

NamedPeer makePeer() { return NamedPeer{"laptop-of-alice"}; }

void lesson_15_10()
{
    std::cout << "--- 15.10 ref qualifiers ---\n";
    NamedPeer peer{"phone-of-bob"};
    const std::string& ref = peer.name();   // & version: reference, peer still alive → fine
    std::string owned = makePeer().name();  // && version: returns a real string, no dangling
    std::cout << "lvalue → " << ref << ", temporary → " << owned << "\n\n";
}

int main()
{
    lesson_15_1();
    lesson_15_2();
    lesson_15_3();
    lesson_15_4();
    lesson_15_5();
    lesson_15_6();
    lesson_15_7();
    lesson_15_8();
    lesson_15_9();
    lesson_15_10();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Give PacketBuilder::build() a check: if the payload is bigger than
//      65535 bytes, return std::nullopt (change the return type to
//      std::optional<Bytes>). Why is the static_cast in build() dangerous
//      without that check? (Chapter 10.3.)
//   2. Add a destructor to CipherState that wipes m_key with secureWipe().
//      Add a print to it and watch it run at the end of lesson_15_9().
//   3. Replace 'friend class Tunnel' with a public member function
//      'void CipherState::rekey(const Key& newKey)'. Is the friend still
//      needed? Which design is easier to review?
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why isn't "free the memory" enough when a key object dies?
//   - Why might a plain zeroing loop be removed by the compiler?
//   - What problem does the && overload of name() solve?
// =============================================================================
