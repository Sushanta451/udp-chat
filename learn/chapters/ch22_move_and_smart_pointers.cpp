// =============================================================================
// CHAPTER 22 — Move Semantics and Smart Pointers
// learncpp.com/cpp-tutorial  (lessons 22.1 – 22.7)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   Every resource (memory, a socket, a key) should have exactly ONE clear
//   owner who cleans it up, and "moving" is how you hand ownership to
//   someone else without copying.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN is full of things that must be cleaned up exactly once:
//     - sockets           (forget to close → you run out of file descriptors)
//     - session objects   (forget to free → memory grows until the server dies)
//     - secret keys       (forget to wipe → keys sit in memory for attackers)
//   And some things must NEVER be copied: a second copy of a key is a second
//   place it can leak from. Move semantics + smart pointers solve both.
//
//       Socket a ──move──► Socket b        a is now empty; only b closes the fd
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   Watch the "[ctor]" / "[dtor]" messages in the output: they show exactly
//   when objects are created and destroyed.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch22_move_and_smart_pointers.cpp -o ch22 && ./ch22
// =============================================================================

#include <arpa/inet.h>    // inet_ntop
#include <array>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>         // std::unique_ptr, std::shared_ptr, std::weak_ptr
#include <netdb.h>        // getaddrinfo, freeaddrinfo
#include <netinet/in.h>   // sockaddr_in
#include <string>
#include <sys/socket.h>   // ::socket
#include <unistd.h>       // ::close
#include <utility>        // std::move, std::exchange
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// =============================================================================
// 22.1  Introduction to smart pointers and move semantics
// =============================================================================
// IN PLAIN WORDS
//   'new' gives you heap memory. YOU must 'delete' it. Every early return,
//   every 'continue', every error path is a chance to forget. Forgetting =
//   a "leak" (memory nobody can free).
//
//   Fix: put the pointer inside an object whose DESTRUCTOR deletes it.
//   Destructors always run when the object goes out of scope, on every path.
//   This trick is called RAII ("Resource Acquisition Is Initialization",
//   a bad name for a great idea: "the object owns it, the object cleans up").
//   A class that does this for a pointer is a "smart pointer".
//
//   Problem: what if you COPY a smart pointer? Two owners → double delete.
//   Answer: don't copy, MOVE. Moving = handing over ownership. The old owner
//   becomes empty. That's "move semantics".
//
// WHY A VPN CARES
//   A packet handler has ~10 ways to bail out (bad length, bad MAC, replayed
//   counter...). With raw new/delete, each exit is a possible leak. A server
//   that leaks 1 KB per bad packet dies after a few million junk packets.
//
// RULE
//   Never write 'delete' in normal code. Let an owning object do it.
struct Session {
    PeerId peer;
    explicit Session(PeerId p) : peer{p} { std::cout << "  [ctor] Session " << peer << '\n'; }
    ~Session() { std::cout << "  [dtor] Session " << peer << '\n'; }
};

bool handleRaw(bool badPacket)
{
    Session* s = new Session{1};
    if (badPacket) return false;   // LEAK: we forgot 'delete s' on this path
    delete s;
    return true;
}

bool handleSmart(bool badPacket)
{
    auto s = std::make_unique<Session>(2);   // owns the Session (22.5)
    if (badPacket) return false;             // no leak: s's destructor deletes it
    return true;
}

void lesson_22_1()
{
    std::cout << "--- 22.1 why smart pointers ---\n";
    std::cout << "raw pointer, bad packet:\n";
    // handleRaw(true);   // would print [ctor] but never [dtor] → leaked (leak tools would flag it)
    handleRaw(false);
    std::cout << "smart pointer, bad packet:\n";
    handleSmart(true);    // [ctor] and [dtor]: cleaned up even on the error path
    std::cout << '\n';
}

// =============================================================================
// 22.2  R-value references
// =============================================================================
// IN PLAIN WORDS
//   l-value: something with a name that lives on ('packet'). You can't just
//            steal its contents; someone will use it later.
//   r-value: a temporary that's about to die ('makePacket()', 'Bytes(100)').
//            Nobody can use it after this line, so it's SAFE TO STEAL FROM.
//
//   'T&'  (l-value reference) binds to named things.
//   'T&&' (r-value reference) binds to temporaries.
//   You can overload a function on both: one version copies, the other steals.
//
// WHY A VPN CARES
//   Stealing a 64 KB buffer = copying 3 pointers. Copying it = 64 KB of
//   memcpy. Per packet. At 100,000 packets per second, that matters.
//
// RULE
//   You'll rarely write '&&' yourself, except in move constructors (22.3).
//   Just learn to recognize "this is a temporary, it can be stolen from".
Bytes makePacket() { return Bytes(1420, 0xAB); }

void describe(const Bytes& b) { std::cout << "  l-value version (must copy), " << b.size() << " bytes\n"; }
void describe(Bytes&& b) { std::cout << "  r-value version (may steal), " << b.size() << " bytes\n"; }

void lesson_22_2()
{
    std::cout << "--- 22.2 r-value references ---\n";
    Bytes packet = makePacket();
    describe(packet);          // has a name → l-value
    describe(makePacket());    // temporary → r-value
    describe(Bytes(64));       // temporary → r-value

    int&& temp = 51820;        // an r-value reference keeps the temporary alive
    std::cout << "  int&& bound to a temporary: " << temp << "\n\n";
}

// =============================================================================
// 22.3  Move constructors and move assignment
// =============================================================================
// IN PLAIN WORDS
//   Copy constructor:  T(const T& other)  → duplicate other's stuff.
//   Move constructor:  T(T&& other)       → take other's stuff, leave other
//                                           EMPTY but still safe to destroy.
//   Same for assignment: operator=(const T&) vs operator=(T&&).
//
//   For some resources, a copy makes no sense at all. What would copying a
//   socket mean? Two objects both calling close() on fd 3 → the second close
//   might close a DIFFERENT socket that reused number 3. So we "= delete"
//   the copy operations. The class is "move-only".
//
// WHY A VPN CARES
//   The VPN's UDP socket is created once and handed to the event loop.
//   Move-only makes "two owners of one socket" a compile error.
//
// RULE
//   Resource-owning classes: delete copy, write move, and set the
//   moved-from object to "empty" (here: fd = -1). Mark moves noexcept
//   (std::vector only uses your move when it's noexcept).
class Socket {
public:
    Socket() : m_fd{::socket(AF_INET, SOCK_DGRAM, 0)}
    {
        std::cout << "  [ctor] Socket fd=" << m_fd << '\n';
    }
    ~Socket()
    {
        if (m_fd >= 0) {
            std::cout << "  [dtor] Socket closes fd=" << m_fd << '\n';
            ::close(m_fd);
        } else {
            std::cout << "  [dtor] Socket (empty, nothing to close)\n";
        }
    }

    Socket(const Socket&) = delete;              // no copies
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : m_fd{std::exchange(other.m_fd, -1)} {}   // steal
    Socket& operator=(Socket&& other) noexcept
    {
        if (this != &other) {
            if (m_fd >= 0) ::close(m_fd);        // drop what we had
            m_fd = std::exchange(other.m_fd, -1);
        }
        return *this;
    }

    int fd() const { return m_fd; }
    bool valid() const { return m_fd >= 0; }

private:
    int m_fd{-1};
};

void lesson_22_3()
{
    std::cout << "--- 22.3 move constructor / move assignment ---\n";
    Socket a{};
    // Socket copy = a;          // ERROR: copy constructor is deleted. Good!
    Socket b = std::move(a);     // move constructor: b takes the fd
    std::cout << "  after move: a.valid()=" << std::boolalpha << a.valid()
              << ", b.valid()=" << b.valid() << '\n';   // false, true
    std::cout << "  (end of scope: b closes the fd, a has nothing to close)\n";
}

// =============================================================================
// 22.4  std::move
// =============================================================================
// IN PLAIN WORDS
//   std::move DOES NOT MOVE ANYTHING. It's just a cast that says "treat
//   this named thing like a temporary; I give permission to steal from it".
//   The actual stealing happens in the move constructor/assignment it picks.
//
//   After a move, the old object is "valid but unspecified": you may
//   destroy it or assign a new value, but don't read it and expect anything.
//   (Your OWN classes can promise more, like SecretKey below.)
//
// WHY A VPN CARES
//   1. Pushing a packet buffer into a send queue: move it, don't copy it.
//   2. Secret keys: a copy of a key is a second place it can leak from.
//      SecretKey is move-only, and it WIPES its bytes when destroyed and
//      when moved-from. So exactly one live copy exists at any time.
//
// RULE
//   Use std::move when you're DONE with a named object and want to hand it
//   off. Never use the object afterwards (except to assign or destroy it).

// Wipe memory in a way the optimizer won't delete. Plain memset before
// freeing can be optimized away. Real code: sodium_memzero / explicit_bzero.
// This volatile loop is fine for learning.
void secureWipe(Byte* p, std::size_t n)
{
    volatile Byte* v = p;
    for (std::size_t i = 0; i < n; ++i) v[i] = 0;
}

class SecretKey {
public:
    static constexpr std::size_t kSize = 32;   // 32 bytes = 256-bit key (ChaCha20 key size)

    explicit SecretKey(const std::array<Byte, kSize>& bytes) : m_bytes{bytes} {}
    ~SecretKey() { secureWipe(m_bytes.data(), kSize); }

    SecretKey(const SecretKey&) = delete;              // NO copies of secrets
    SecretKey& operator=(const SecretKey&) = delete;

    SecretKey(SecretKey&& other) noexcept : m_bytes{other.m_bytes}
    {
        secureWipe(other.m_bytes.data(), kSize);       // moved-from = all zeros
    }
    SecretKey& operator=(SecretKey&& other) noexcept
    {
        if (this != &other) {
            m_bytes = other.m_bytes;
            secureWipe(other.m_bytes.data(), kSize);
        }
        return *this;
    }

    Byte first() const { return m_bytes[0]; }          // for the demo only. Never log keys!

private:
    std::array<Byte, kSize> m_bytes{};
};

void lesson_22_4()
{
    std::cout << "--- 22.4 std::move ---\n";
    std::deque<Bytes> sendQueue{};
    Bytes packet(65'000, 0x11);
    const Byte* before = packet.data();
    sendQueue.push_back(std::move(packet));   // steals the buffer: no 65 KB copy
    std::cout << "  queued buffer is the SAME memory? " << std::boolalpha
              << (sendQueue.back().data() == before) << '\n';   // true
    // 'packet' is now valid-but-unspecified. Give it a new value before reuse:
    packet.assign(10, 0);
    std::cout << "  packet reused, size=" << packet.size() << '\n';

    std::array<Byte, SecretKey::kSize> raw{};
    raw.fill(0x7F);                 // TOY key bytes. Real keys come from a CSPRNG
    SecretKey k1{raw};              // (randombytes_buf in libsodium).
    secureWipe(raw.data(), raw.size());   // don't leave the source bytes lying around
    // SecretKey k2 = k1;           // ERROR: copy is deleted. Good!
    SecretKey k2 = std::move(k1);
    std::cout << "  k2 first byte = 0x" << std::hex << int{k2.first()}
              << ", moved-from k1 first byte = 0x" << int{k1.first()} << std::dec
              << "  (wiped)\n\n";   // 0x7f, 0x0
}

// =============================================================================
// 22.5  std::unique_ptr
// =============================================================================
// IN PLAIN WORDS
//   std::unique_ptr<T> = a smart pointer with exactly ONE owner.
//   Create with std::make_unique<T>(args). It deletes T in its destructor.
//   It's move-only (like Socket). It costs nothing extra vs a raw pointer.
//
//   Need to let a function USE the object without owning it? Pass a plain
//   reference (T&) or raw pointer (T*). Raw pointers are fine for
//   "looking at"; they're only bad for "owning".
//
//   Custom deleter: C libraries give you things that must be freed with a
//   SPECIFIC function (getaddrinfo → freeaddrinfo, fopen → fclose).
//   unique_ptr can call that function for you.
//
// WHY A VPN CARES
//   The session table owns one Session per peer. Removing a peer from the
//   map destroys its Session automatically. No manual cleanup, no leaks.
//
// RULE
//   Default choice for heap objects: std::unique_ptr via std::make_unique.
void printSession(const Session& s)   // non-owning: just looks
{
    std::cout << "  looking at session for peer " << s.peer << '\n';
}

struct AddrInfoDeleter {
    void operator()(addrinfo* p) const
    {
        std::cout << "  [deleter] freeaddrinfo\n";
        freeaddrinfo(p);
    }
};
using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

AddrInfoPtr resolve(const char* host, const char* port)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICHOST;   // numbers only, no DNS lookup
    addrinfo* result = nullptr;
    if (getaddrinfo(host, port, &hints, &result) != 0) return AddrInfoPtr{};
    return AddrInfoPtr{result};        // from here on, cleanup is automatic
}

void lesson_22_5()
{
    std::cout << "--- 22.5 std::unique_ptr ---\n";
    std::map<PeerId, std::unique_ptr<Session>> sessions{};
    sessions[7] = std::make_unique<Session>(7);
    sessions[9] = std::make_unique<Session>(9);
    printSession(*sessions[7]);   // lend it out by reference

    // auto copy = sessions[7];   // ERROR: unique_ptr can't be copied
    std::cout << "  peer 9 disconnects → erase:\n";
    sessions.erase(9);            // Session 9 destroyed right here

    if (AddrInfoPtr info = resolve("10.0.0.2", "51820")) {
        auto* sin = reinterpret_cast<sockaddr_in*>(info->ai_addr);
        std::array<char, INET_ADDRSTRLEN> text{};
        inet_ntop(AF_INET, &sin->sin_addr, text.data(), static_cast<socklen_t>(text.size()));
        std::cout << "  resolved " << text.data() << ':' << ntohs(sin->sin_port) << '\n';
    }   // ← freeaddrinfo runs here
    std::cout << "  end of lesson: map destroys Session 7:\n";
}

// =============================================================================
// 22.6  std::shared_ptr
// =============================================================================
// IN PLAIN WORDS
//   std::shared_ptr<T> = MANY owners. It keeps a count ("use_count") of how
//   many shared_ptrs point at the object. When the last one goes away, the
//   object is deleted. Create with std::make_shared<T>(args).
//
//   It's not free: there's an extra "control block" (where the count lives),
//   and every copy changes the count with an atomic (thread-safe, slower)
//   operation. It also makes it unclear who's in charge.
//
// WHY A VPN CARES
//   A peer's config (public key, endpoint, allowed IPs) may be read by
//   several handshake jobs in flight at once. If the admin removes the
//   peer mid-handshake, the jobs still hold it safely until they finish.
//
// RULE
//   Use shared_ptr only when ownership REALLY is shared and you can't say
//   who will finish last. Otherwise unique_ptr.
struct PeerConfig {
    std::string endpoint;
    explicit PeerConfig(std::string ep) : endpoint{std::move(ep)}
    {
        std::cout << "  [ctor] PeerConfig " << endpoint << '\n';
    }
    ~PeerConfig() { std::cout << "  [dtor] PeerConfig " << endpoint << '\n'; }
};

struct HandshakeJob {
    std::shared_ptr<const PeerConfig> config;   // shared owner
};

void lesson_22_6()
{
    std::cout << "--- 22.6 std::shared_ptr ---\n";
    auto config = std::make_shared<const PeerConfig>("10.0.0.2:51820");
    std::cout << "  use_count = " << config.use_count() << '\n';   // 1

    std::vector<HandshakeJob> jobs{};
    jobs.push_back({config});
    jobs.push_back({config});
    std::cout << "  two jobs started, use_count = " << config.use_count() << '\n';   // 3

    config.reset();   // admin removes the peer from the main table
    std::cout << "  admin removed peer, jobs still see " << jobs[0].config->endpoint
              << ", use_count = " << jobs[0].config.use_count() << '\n';   // 2
    std::cout << "  jobs finish:\n";
    jobs.clear();     // last owners gone → [dtor] PeerConfig
    std::cout << '\n';
}

// =============================================================================
// 22.7  Circular dependency issues with std::shared_ptr, and std::weak_ptr
// =============================================================================
// IN PLAIN WORDS
//   If A holds a shared_ptr to B and B holds a shared_ptr to A, each keeps
//   the other's count above 0. When your last outside pointer goes away,
//   neither count reaches 0. Neither is deleted. LEAK, silently.
//
//       Peer ──shared──► Session
//        ▲                  │
//        └─────shared───────┘      counts never hit 0
//
//   std::weak_ptr<T> points at a shared object WITHOUT owning it (it doesn't
//   add to use_count). To use it, call .lock(): you get a shared_ptr if the
//   object is still alive, or an empty one if it's gone.
//
// WHY A VPN CARES
//   A Peer owns its current Session; the Session wants to know its Peer.
//   Make the back-pointer weak. Otherwise every peer that ever connected
//   stays in memory forever.
//
// RULE
//   Owner → owned: shared_ptr (or unique_ptr). Back-pointer: weak_ptr.
struct BadSession;
struct BadPeer {
    std::shared_ptr<BadSession> session;
    ~BadPeer() { std::cout << "  [dtor] BadPeer\n"; }
};
struct BadSession {
    std::shared_ptr<BadPeer> peer;          // strong back-pointer: the bug
    ~BadSession() { std::cout << "  [dtor] BadSession\n"; }
};

struct GoodSession;
struct GoodPeer {
    PeerId id{};
    std::shared_ptr<GoodSession> session;
    ~GoodPeer() { std::cout << "  [dtor] GoodPeer\n"; }
};
struct GoodSession {
    std::weak_ptr<GoodPeer> peer;           // weak back-pointer: the fix
    ~GoodSession() { std::cout << "  [dtor] GoodSession\n"; }
};

void lesson_22_7()
{
    std::cout << "--- 22.7 cycles and std::weak_ptr ---\n";
    std::weak_ptr<BadPeer> watcher;         // lets us peek without owning
    {
        auto peer = std::make_shared<BadPeer>();
        peer->session = std::make_shared<BadSession>();
        peer->session->peer = peer;         // cycle!
        watcher = peer;
        std::cout << "  bad: leaving scope...\n";
    }
    std::cout << "  bad: no [dtor] printed. Peer still alive? " << std::boolalpha
              << !watcher.expired() << ", use_count = " << watcher.use_count() << '\n';

    // Clean up by hand so this program doesn't actually leak (keeps leak checkers quiet).
    // In real code you wouldn't have a 'watcher' — the memory would be lost.
    if (auto stuck = watcher.lock()) {
        std::cout << "  bad: breaking the cycle by hand:\n";
        stuck->session.reset();             // BadSession dies, drops its hold on BadPeer
    }                                       // 'stuck' gone → BadPeer dies

    {
        auto peer = std::make_shared<GoodPeer>();
        peer->id = 7;
        peer->session = std::make_shared<GoodSession>();
        peer->session->peer = peer;         // weak: does NOT raise use_count
        std::cout << "  good: peer use_count = " << peer.use_count() << '\n';   // 1
        if (auto p = peer->session->peer.lock())   // session asks "is my peer alive?"
            std::cout << "  good: session's peer is " << p->id << '\n';
        std::cout << "  good: leaving scope...\n";
    }   // GoodPeer and GoodSession both destroyed here
    std::cout << '\n';
}

// =============================================================================
// WHICH ONE DO I USE?  (the decision table)
// =============================================================================
//   Situation                                      Use
//   ---------------------------------------------  ------------------------------
//   Small/normal object, one obvious owner         plain object (no pointer!)
//                                                  e.g. Socket sock; Bytes buf;
//   Must live on the heap, ONE owner               std::unique_ptr (make_unique)
//     (big, polymorphic, or stored in a map)       e.g. map<PeerId, unique_ptr<Session>>
//   Truly SHARED, can't say who finishes last      std::shared_ptr (make_shared)
//                                                  e.g. PeerConfig read by many jobs
//   Need to point back at a shared owner           std::weak_ptr
//                                                  e.g. Session → its Peer
//   Just USE something someone else owns           T& (or const T&); T* if it can be null
//                                                  e.g. printSession(const Session&)
//   C resource (fd, FILE*, addrinfo*)              RAII class or unique_ptr + custom deleter
//   Secret material                                move-only class that wipes itself
//
//   Never: 'new'/'delete' by hand. Never: a raw pointer that OWNS something.

int main()
{
    lesson_22_1();
    lesson_22_2();
    lesson_22_3();
    std::cout << '\n';
    lesson_22_4();
    lesson_22_5();
    std::cout << '\n';
    lesson_22_6();
    lesson_22_7();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment 'handleRaw(true);' in 22.1 and find the leak with a tool.
//      Linux: build with -fsanitize=address, run, read the "LeakSanitizer"
//      report. macOS (Apple's ASan has no leak checker): run
//      'leaks --atExit -- ./ch22'. Then fix handleRaw with std::unique_ptr.
//   2. Add 'bool sendTo(const Bytes& packet, ...)' to Socket using ::sendto,
//      and a 'static Socket bound(std::uint16_t port)' that also calls ::bind.
//      Return an empty Socket (fd -1) on failure. Can you return a Socket by
//      value even though it can't be copied? Why?
//   3. Write a unique_ptr with a custom deleter for FILE* (fopen/fclose).
//      Use it to write "peer 7 connected" to a log file.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - std::move doesn't move anything. So what does it do?
//   - Why should a SecretKey be move-only, and what must its move
//     constructor do to the moved-from object?
//   - Peer holds shared_ptr<Session>, Session holds shared_ptr<Peer>. What
//     happens, and which pointer do you change to weak_ptr?
// =============================================================================
