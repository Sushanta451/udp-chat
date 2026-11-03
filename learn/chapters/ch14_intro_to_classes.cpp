// =============================================================================
// CHAPTER 14 — Introduction to Classes
// learncpp.com/cpp-tutorial  (lessons 14.1 – 14.17)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   A class bundles data WITH the functions allowed to touch it, and hides
//   the data so nobody can put it into a broken state.
//
// HOW THIS CONNECTS TO A VPN
//   Security code is full of rules that must ALWAYS be true. Example:
//
//       "The send counter only goes UP. A counter value is never used twice
//        with the same key."
//
//   If two packets are encrypted with the same key and the same counter
//   (the counter is the "nonce" — number used once), an attacker can recover
//   the plaintext. That's a real, catastrophic crypto failure.
//   With a plain struct, any line of code can write 'counter = 0'. With a
//   class, the counter is private, and the only way to touch it is a
//   function that can only go up. The compiler enforces your security rule.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch14_intro_to_classes.cpp -o ch14 && ./ch14
// =============================================================================

#include <array>
#include <cstddef>   // std::size_t
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Same nicknames as chapter 10.
using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;
using Key = std::array<Byte, 32>;   // a 32-byte key (chapter 13.15)

// =============================================================================
// 14.1  Introduction to object-oriented programming
// =============================================================================
// IN PLAIN WORDS
//   "Procedural" style: data in one place, functions somewhere else.
//       sendPacket(tunnel, data);
//   "Object-oriented" (OOP) style: the data and its functions live together
//   in one "object".
//       tunnel.send(data);
//   An object is a thing that HAS data (state) and CAN DO things (behavior).
//
// WHY A VPN CARES
//   A tunnel, a peer, a session — each is naturally a "thing" with state and
//   behavior. OOP lets the code look like the problem.
//
// RULE
//   Think "what things exist, what do they know, what can they do?"
void lesson_14_1()
{
    std::cout << "--- 14.1 OOP ---\n";
    std::cout << "procedural: sendPacket(tunnel, data)   OOP: tunnel.send(data)\n\n";
}

// =============================================================================
// 14.2  Introduction to classes
// =============================================================================
// IN PLAIN WORDS
//   An "invariant" is a rule about an object that must always be true.
//   A struct can't protect its invariants: every member is public, so anyone
//   can break the rule.
//
//   A 'class' is like a struct, but its members are PRIVATE by default
//   (only the class's own functions can touch them). That's the only
//   technical difference between 'class' and 'struct'.
//
// WHY A VPN CARES
//   Below, RawSendState is a struct. One careless line resets the counter
//   to 0 and the next packet reuses a nonce. Nothing stops it.
//
// RULE
//   Plain data with no rules → struct. Data with invariants → class.
struct RawSendState
{
    Counter nextCounter{0};   // invariant (unenforced!): only ever goes up
};

void lesson_14_2()
{
    std::cout << "--- 14.2 classes ---\n";
    RawSendState s{};
    s.nextCounter = 5;   // sent 5 packets
    s.nextCounter = 0;   // a "reset" bug — compiles fine. Packet 0 will be reused!
    std::cout << "struct counter after the bug: " << s.nextCounter
              << " (nonce reuse waiting to happen)\n\n";
}

// =============================================================================
// 14.3  Member functions
// =============================================================================
// IN PLAIN WORDS
//   A "member function" is a function declared inside a class/struct.
//   You call it ON an object:  addr.print();
//   Inside it, members are used directly (just 'port', no 'addr.' needed),
//   because it always works on the object it was called on.
//
// RULE
//   If a function mainly works on one type's data, make it a member.
struct PeerAddress
{
    std::string host{"0.0.0.0"};
    std::uint16_t port{51820};

    void print() { std::cout << host << ":" << port << "\n"; }   // not const yet (see 14.4)
    bool isDefaultPort() { return port == 51820; }
};

void lesson_14_3()
{
    std::cout << "--- 14.3 member functions ---\n";
    PeerAddress addr{"203.0.113.5", 51820};
    addr.print();
    std::cout << "default port? " << std::boolalpha << addr.isDefaultPort() << "\n\n";
}

// =============================================================================
// 14.4  Const class objects and const member functions
// =============================================================================
// IN PLAIN WORDS
//   A const object can't be changed. So C++ only lets you call member
//   functions that PROMISE not to change it. You promise by writing 'const'
//   after the parameter list:  void print() const;
//
// WHY A VPN CARES
//   Config, received packets, peer info passed by const& — all const. If
//   your getters aren't marked const, you can't even read them.
//
// RULE
//   Mark every member function that doesn't modify the object as const.
struct PeerInfo
{
    std::string host{"0.0.0.0"};
    std::uint16_t port{51820};

    void print() const { std::cout << host << ":" << port << "\n"; }
    // void setPort(std::uint16_t p) const { port = p; }   // ERROR: const function can't modify
};

void lesson_14_4()
{
    std::cout << "--- 14.4 const member functions ---\n";
    const PeerAddress fixedAddr{"198.51.100.1", 51820};
    // fixedAddr.print();   // ERROR: PeerAddress::print() isn't const
    std::cout << "(fixedAddr.print() won't compile: print() isn't const; host is "
              << fixedAddr.host << ")\n";

    const PeerInfo info{"198.51.100.1", 51820};
    info.print();   // fine: print() is const
    std::cout << '\n';
}

// =============================================================================
// 14.5  Public and private members and access specifiers
// =============================================================================
// IN PLAIN WORDS
//   "Access specifiers" control who can touch a member:
//     public:   anyone
//     private:  only this class's own member functions
//   class → private by default.  struct → public by default.
//   Private members are usually named with an 'm_' prefix (m = member).
//
// WHY A VPN CARES
//   The secret key, the send counter, the session state: all private. The
//   outside world gets only the operations that keep things safe.
//
// RULE
//   Make data members private. Make the operations public.
class Peer
{
public:
    PeerId id() const { return m_id; }                             // 14.6: a getter
    const std::string& allowedIp() const { return m_allowedIp; }   // 14.7: const ref is OK
    int keepaliveSeconds() const { return m_keepaliveSeconds; }

    bool setKeepaliveSeconds(int seconds)   // 14.6: a setter that CHECKS
    {
        if (seconds < 0 || seconds > 3600) return false;   // reject nonsense
        m_keepaliveSeconds = seconds;
        return true;
    }

private:
    PeerId m_id{1};
    std::string m_allowedIp{"10.0.0.2/32"};   // which inner IPs this peer may use
    int m_keepaliveSeconds{25};
};

void lesson_14_5()
{
    std::cout << "--- 14.5 public/private ---\n";
    Peer peer{};
    // peer.m_allowedIp = "0.0.0.0/0";   // ERROR: m_allowedIp is private
    //   (that line would let this peer send traffic as ANY IP — exactly what we're preventing)
    std::cout << "peer " << peer.id() << " allowed " << peer.allowedIp() << "\n\n";
}

// =============================================================================
// 14.6  Access functions
// =============================================================================
// IN PLAIN WORDS
//   "Access functions" are small public functions to read or change private data:
//     getter  (reads):    keepaliveSeconds()   — should be const
//     setter  (changes):  setKeepaliveSeconds(25)
//   A setter is your chance to CHECK the new value before accepting it.
//
// WHY A VPN CARES
//   A keepalive of -5 or 999999 seconds is nonsense. A checking setter makes
//   that impossible to store.
//
// RULE
//   Don't add a getter/setter for every member by reflex. Add the ones the
//   outside actually needs, and validate in setters.
void lesson_14_6()
{
    std::cout << "--- 14.6 access functions ---\n";
    Peer peer{};
    std::cout << std::boolalpha << "set 30 ok? " << peer.setKeepaliveSeconds(30)
              << "   set -5 ok? " << peer.setKeepaliveSeconds(-5)
              << "   value now: " << peer.keepaliveSeconds() << "\n\n";
}

// =============================================================================
// 14.7  Member functions returning references to data members
// =============================================================================
// IN PLAIN WORDS
//   A getter can return a reference instead of a copy (no copying a long
//   string). That's fine if it's a CONST reference: the caller can only read.
//
//   Returning a NON-const reference to a private member hands the caller a
//   key to your private room. They can change it with no checks.
//
//   Also: a reference into an object dies when the object dies. A reference
//   from a TEMPORARY object dies at the end of that line ("dangling").
//
// WHY A VPN CARES
//   BadKeyHolder::secret() below returns Key&. Now any code can overwrite or
//   copy the secret key. The private keyword is pointless.
//   The fix: no getter for the secret at all. Offer OPERATIONS instead.
//
// RULE
//   Return const& (or a copy) from getters. Never a non-const & to private data.
//   Secrets get no getter.
class BadKeyHolder
{
public:
    Key& secret() { return m_secret; }   // BAD: gives away the private key
private:
    Key m_secret{};
};

class KeyHolder
{
public:
    // An operation, not a getter. TOY ONLY: XOR of all bytes is NOT a real
    // fingerprint and NOT secure. Real code uses a vetted library (libsodium).
    Byte toyFingerprint() const
    {
        Byte x{0};
        for (Byte b : m_secret) x ^= b;
        return x;
    }

private:
    Key m_secret{};   // no getter. Nobody outside can read or change it.
};

void lesson_14_7()
{
    std::cout << "--- 14.7 returning references ---\n";
    BadKeyHolder bad{};
    bad.secret()[0] = 0xFF;   // outsider rewrote "private" key material!
    std::cout << "outsider changed bad key byte 0 to " << static_cast<int>(bad.secret()[0]) << "\n";

    KeyHolder good{};
    std::cout << "good holder exposes only a toy fingerprint: "
              << static_cast<int>(good.toyFingerprint()) << "\n";

    // const std::string& ip = Peer{}.allowedIp();   // compiles, but Peer{} dies at the ';'
    // std::cout << ip;                               // → dangling reference. Undefined behavior.
    std::string ip = Peer{}.allowedIp();               // safe: copy it out instead
    std::cout << "copied from a temporary: " << ip << "\n\n";
}

// =============================================================================
// 14.8  The benefits of data hiding (encapsulation)
// =============================================================================
// IN PLAIN WORDS
//   "Encapsulation" = hide the data, expose safe operations.
//   Benefits:
//     1. Invariants can't be broken from outside.
//     2. You can change the inside later without breaking callers.
//     3. Bugs are easier to find: only a few functions touch the data.
//
// WHY A VPN CARES
//   NonceCounter below is the fixed version of 14.2. The ONLY way to get a
//   counter is next(), and it only goes up. When it runs out, it refuses
//   (returns nothing) instead of wrapping to 0. Running out means "rekey".
//   Real protocols rekey long before that (WireGuard rekeys after 2^60
//   messages). Our limit is tiny so you can see it happen.
//
// RULE
//   Put the security rule INSIDE the class, so it can't be forgotten outside.
class NonceCounter
{
public:
    std::optional<Counter> next()
    {
        if (m_next >= m_limit) return std::nullopt;   // exhausted: must rekey, never wrap
        return m_next++;
    }

private:
    Counter m_next{0};
    Counter m_limit{3};   // tiny for the demo
};

void lesson_14_8()
{
    std::cout << "--- 14.8 encapsulation ---\n";
    NonceCounter nonces{};
    for (int i = 0; i < 4; ++i)
    {
        if (auto n = nonces.next())
            std::cout << "send with counter " << *n << "\n";
        else
            std::cout << "counter exhausted → rekey before sending\n";
    }
    // nonces.m_next = 0;   // ERROR: private. The 14.2 bug is now impossible.
    std::cout << '\n';
}

// =============================================================================
// 14.9  Introduction to constructors
// =============================================================================
// IN PLAIN WORDS
//   A "constructor" is a special member function that runs AUTOMATICALLY
//   when an object is created. Same name as the class, no return type.
//   It sets the object up so it starts valid.
//
//   Once a class has private members, you can't use {a, b} aggregate init
//   any more (13.8) — you need a constructor.
//
// WHY A VPN CARES
//   A Tunnel with no peer and MTU 0 should never exist. A constructor forces
//   the caller to provide them at creation time.
//
// RULE
//   Use constructors so every object is valid from its first moment.
class Tunnel
{
public:
    Tunnel(PeerId peer, std::uint16_t mtu)
        : m_peer{peer}, m_mtu{mtu}   // this ": ..." list is explained in 14.10
    {
        std::cout << "Tunnel to peer " << m_peer << " created (mtu " << m_mtu << ")\n";
    }

private:
    PeerId m_peer{};
    std::uint16_t m_mtu{};
};

void lesson_14_9()
{
    std::cout << "--- 14.9 constructors ---\n";
    Tunnel t{7, 1420};   // the constructor runs right here
    // Tunnel bad{};      // ERROR: no constructor takes zero arguments
    std::cout << '\n';
}

// =============================================================================
// 14.10  Constructor member initializer lists
// =============================================================================
// IN PLAIN WORDS
//   The part after the colon,  : m_linkMtu{linkMtu}, m_tunnelMtu{...}
//   is the "member initializer list". It INITIALIZES members (gives them
//   their first value) instead of assigning them later in the body.
//   - const members and references MUST be set here.
//   - Members are initialized in the order they're DECLARED in the class,
//     not the order you write them in the list. -Wall warns if they differ.
//
// WHY A VPN CARES
//   WireGuard adds up to 80 bytes of overhead (outer IPv6 + UDP + its own
//   header + auth tag), so tunnel MTU = link MTU − 80. 1500 − 80 = 1420.
//   m_tunnelMtu uses m_linkMtu, so m_linkMtu must be declared first.
//
// RULE
//   Initialize members in the list, in declaration order.
class MtuInfo
{
public:
    explicit MtuInfo(std::uint16_t linkMtu)
        : m_linkMtu{linkMtu},
          m_tunnelMtu{static_cast<std::uint16_t>(m_linkMtu - kOverhead)}   // safe: m_linkMtu is ready
    {
    }

    void print() const { std::cout << "link " << m_linkMtu << " → tunnel " << m_tunnelMtu << "\n"; }

private:
    static constexpr int kOverhead{80};   // (static is chapter 15.6: one shared constant)
    const std::uint16_t m_linkMtu;        // const: must be set in the init list
    const std::uint16_t m_tunnelMtu;      // declared AFTER m_linkMtu, so initialized after it
};

void lesson_14_10()
{
    std::cout << "--- 14.10 member initializer lists ---\n";
    MtuInfo{1500}.print();
    std::cout << '\n';
}

// =============================================================================
// 14.11  Default constructors and default arguments
// =============================================================================
// IN PLAIN WORDS
//   A "default constructor" takes no arguments:  ListenConfig cfg{};
//   '= default' says "compiler, write the obvious one for me" (it uses the
//   default member initializers).
//   Default ARGUMENTS let one constructor serve several calls:
//       ListenConfig(std::uint16_t port, int keepalive = 25)
//
// RULE
//   Give members defaults, and add 'ClassName() = default;' when "all
//   defaults" is a valid object.
class ListenConfig
{
public:
    ListenConfig() = default;
    explicit ListenConfig(std::uint16_t port, int keepaliveSeconds = 25)   // explicit: see 14.16
        : m_port{port}, m_keepaliveSeconds{keepaliveSeconds}
    {
    }

    void print() const { std::cout << "port " << m_port << ", keepalive " << m_keepaliveSeconds << "s\n"; }

private:
    std::uint16_t m_port{51820};
    int m_keepaliveSeconds{25};
};

void lesson_14_11()
{
    std::cout << "--- 14.11 default ctors + default args ---\n";
    ListenConfig a{};           // default constructor
    ListenConfig b{4500};       // keepalive uses its default (25)
    ListenConfig c{4500, 10};   // both given
    a.print();
    b.print();
    c.print();
    std::cout << '\n';
}

// =============================================================================
// 14.12  Delegating constructors
// =============================================================================
// IN PLAIN WORDS
//   One constructor can hand the work to another ("delegate"):
//       Endpoint(std::string_view host) : Endpoint{host, kDefaultPort} {}
//   So the real setup (and any checking) is written ONCE.
//
// WHY A VPN CARES
//   Most peers use port 51820. A one-argument constructor fills that in,
//   without copy-pasting the setup logic.
//
// RULE
//   Don't duplicate constructor code. Delegate to the most complete one.
class Endpoint
{
public:
    static constexpr std::uint16_t kDefaultPort{51820};

    Endpoint(std::string_view host, std::uint16_t port) : m_host{host}, m_port{port} {}
    explicit Endpoint(std::string_view host) : Endpoint{host, kDefaultPort} {}   // delegates

    void print() const { std::cout << m_host << ":" << m_port << "\n"; }

private:
    std::string m_host{};
    std::uint16_t m_port{};
};

void lesson_14_12()
{
    std::cout << "--- 14.12 delegating constructors ---\n";
    Endpoint a{"10.0.0.1", 4500};
    Endpoint b{"10.0.0.2"};   // gets 51820
    a.print();
    b.print();
    std::cout << '\n';
}

// =============================================================================
// 14.13  Temporary class objects
// =============================================================================
// IN PLAIN WORDS
//   You can create an object with no name, right where you need it:
//       printEndpoint(Endpoint{"10.0.0.1", 51820});
//   It's a "temporary". It lives until the end of that full line (the ';').
//
// RULE
//   Great for one-off arguments and return values. Never keep a reference
//   or pointer to a temporary (see 14.7).
void printEndpoint(const Endpoint& e) { e.print(); }

Endpoint makeServerEndpoint() { return Endpoint{"vpn.example.com"}; }   // returns a temporary

void lesson_14_13()
{
    std::cout << "--- 14.13 temporaries ---\n";
    printEndpoint(Endpoint{"10.0.0.9", 51820});   // the temporary dies after this line
    printEndpoint(makeServerEndpoint());
    std::cout << '\n';
}

// =============================================================================
// 14.14  Introduction to the copy constructor
// =============================================================================
// IN PLAIN WORDS
//   The "copy constructor" builds a new object as a copy of an existing one:
//       ClassName(const ClassName& other)
//   The compiler writes one for you (copy every member). It runs when you
//   pass by value, or write  B b{a};
//   '= delete' forbids copying entirely.
//
// WHY A VPN CARES  — "should a session key be copyable?"
//   No! Think about a Session holding {key, sendCounter = 5}. Copy it, and
//   now TWO objects both believe the next counter is 5. Both send packet 5
//   with the same key → nonce reuse → broken encryption. Copies also spread
//   secret bytes across memory that someone has to wipe (chapter 15.4).
//   So we delete the copy constructor and let the compiler stop the mistake.
//
// RULE
//   Ask "does a copy of this make sense?" for every class. For secrets and
//   unique resources (keys, sockets, sessions), the answer is no: = delete.
class PeerConfig
{
public:
    explicit PeerConfig(std::string_view name) : m_name{name} {}
    PeerConfig(const PeerConfig& other) : m_name{other.m_name}   // our own copy constructor
    {
        std::cout << "  (copy constructor ran for " << m_name << ")\n";
    }
    const std::string& name() const { return m_name; }

private:
    std::string m_name{};
};

class SessionKeys
{
public:
    SessionKeys() = default;
    SessionKeys(const SessionKeys&) = delete;              // no copies, ever
    SessionKeys& operator=(const SessionKeys&) = delete;   // no copy-assignment either

    std::optional<Counter> nextCounter() { return m_counter.next(); }
    std::size_t keySize() const { return m_sendKey.size(); }

private:
    Key m_sendKey{};
    NonceCounter m_counter{};
};

void showConfigByValue(PeerConfig cfg) { std::cout << "  got " << cfg.name() << "\n"; }

void lesson_14_14()
{
    std::cout << "--- 14.14 copy constructor ---\n";
    PeerConfig alice{"alice"};
    showConfigByValue(alice);   // pass by value → copy constructor runs

    SessionKeys keys{};
    // SessionKeys clone{keys};   // ERROR: copy constructor is deleted. Nonce reuse prevented.
    std::cout << "key size " << keys.keySize() << ", first counter " << *keys.nextCounter() << "\n\n";
}

// =============================================================================
// 14.15  Class initialization and copy elision
// =============================================================================
// IN PLAIN WORDS
//   "Copy elision" = the compiler skips a copy it doesn't need, and builds
//   the object directly in its final place.
//   Since C++17 this is GUARANTEED when you return or initialize from a
//   temporary (a "prvalue"), e.g.  return PeerConfig{"bob"};
//   That's why returning objects by value is cheap.
//
// RULE
//   Return by value. Don't put side effects (like logging) in copy
//   constructors and rely on them running — they may be skipped.
PeerConfig makeConfig(std::string_view name) { return PeerConfig{name}; }

void lesson_14_15()
{
    std::cout << "--- 14.15 copy elision ---\n";
    PeerConfig bob = makeConfig("bob");        // no "(copy constructor ran)" line!
    PeerConfig carol = PeerConfig{"carol"};    // no copy here either
    std::cout << "made " << bob.name() << " and " << carol.name() << " with zero copies\n\n";
}

// =============================================================================
// 14.16  Converting constructors and the explicit keyword
// =============================================================================
// IN PLAIN WORDS
//   A constructor with one argument is also a "converting constructor":
//   C++ will use it BY ITSELF to turn that argument type into your class.
//   So if LoosePort(std::uint16_t) exists, any number silently becomes a port.
//   Writing 'explicit' turns that off: you must say Port{51820} on purpose.
//
// WHY A VPN CARES
//   sendKeepalive(LoosePort port, PeerId peer) called as
//   sendKeepalive(7, 51820) — arguments swapped — compiles fine and sends
//   to port 7. With an explicit Port, that call is a compile error.
//
// RULE
//   Make single-argument constructors explicit, unless conversion is truly
//   what you want.
class LoosePort
{
public:
    LoosePort(std::uint16_t value) : m_value{value} {}   // NOT explicit: converts silently
    std::uint16_t value() const { return m_value; }

private:
    std::uint16_t m_value{};
};

class Port
{
public:
    explicit Port(std::uint16_t value) : m_value{value} {}
    std::uint16_t value() const { return m_value; }

private:
    std::uint16_t m_value{};
};

void sendKeepaliveLoose(LoosePort port, PeerId peer)
{
    std::cout << "keepalive → peer " << peer << " on port " << port.value() << "\n";
}

void sendKeepalive(Port port, PeerId peer)
{
    std::cout << "keepalive → peer " << peer << " on port " << port.value() << "\n";
}

void lesson_14_16()
{
    std::cout << "--- 14.16 explicit ---\n";
    sendKeepaliveLoose(7, 51820);         // swapped by mistake — compiles! Wrong port.
    sendKeepalive(Port{51820}, 7);        // explicit: you can SEE it's a port
    // sendKeepalive(7, 51820);           // ERROR: 7 won't silently become a Port
    // Port p = 51820;                    // ERROR: copy-init needs a non-explicit ctor
    std::cout << '\n';
}

// =============================================================================
// 14.17  Constexpr aggregates and classes
// =============================================================================
// IN PLAIN WORDS
//   'constexpr' objects are built at COMPILE time. Works for:
//     - aggregates (simple structs) — just write constexpr
//     - classes — if the constructor and the functions you call are constexpr
//   Then static_assert can check them before the program ever runs.
//   (Note: constexpr member functions are NOT automatically const. Add const.)
//
// WHY A VPN CARES
//   Protocol constants (port, MTU, overhead) are known in advance. Checking
//   "payload room is positive" at compile time means a bad edit fails the
//   build instead of breaking in production.
//
// RULE
//   Make protocol configuration constexpr and static_assert its rules.
struct ProtocolConfig   // aggregate
{
    std::uint16_t defaultPort;
    std::uint16_t linkMtu;
    std::uint16_t overhead;
};

class MtuBudget   // class with constexpr constructor + member function
{
public:
    constexpr explicit MtuBudget(ProtocolConfig cfg) : m_cfg{cfg} {}
    constexpr int payloadRoom() const { return m_cfg.linkMtu - m_cfg.overhead; }

private:
    ProtocolConfig m_cfg;
};

constexpr ProtocolConfig kProtocol{51820, 1500, 80};
constexpr MtuBudget kBudget{kProtocol};
static_assert(kBudget.payloadRoom() > 0, "overhead can't eat the whole MTU");
static_assert(kBudget.payloadRoom() == 1420);

void lesson_14_17()
{
    std::cout << "--- 14.17 constexpr classes ---\n";
    std::cout << "port " << kProtocol.defaultPort << ", payload room " << kBudget.payloadRoom()
              << " (checked at compile time)\n\n";
}

int main()
{
    lesson_14_1();
    lesson_14_2();
    lesson_14_3();
    lesson_14_4();
    lesson_14_5();
    lesson_14_6();
    lesson_14_7();
    lesson_14_8();
    lesson_14_9();
    lesson_14_10();
    lesson_14_11();
    lesson_14_12();
    lesson_14_13();
    lesson_14_14();
    lesson_14_15();
    lesson_14_16();
    lesson_14_17();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Write a class 'AllowedIp' that stores an IPv4 address + prefix length
//      (like 10.0.0.2/32). Its constructor must reject prefix > 32. Since a
//      constructor can't return false, use a static-style helper for now:
//      'std::optional<AllowedIp> makeAllowedIp(...)' that checks first.
//   2. Uncomment 'SessionKeys clone{keys};' in 14.14. Read the error. Then
//      explain in one sentence, in your own words, why that error is a
//      security feature.
//   3. Remove 'explicit' from Port. Uncomment 'sendKeepalive(7, 51820);'.
//      Does it compile now? Run it — what port is used? Put explicit back.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - What's an invariant? Give one from a VPN.
//   - Why is returning a non-const reference to a private member a problem?
//   - Why should copying a session that holds a key + counter be forbidden?
// =============================================================================
