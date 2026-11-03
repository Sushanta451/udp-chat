// =============================================================================
// CHAPTER 20 — Functions
// learncpp.com/cpp-tutorial  (lessons 20.1 – 20.7)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   A function is a value too: you can store it, pass it around, and call it
//   later. That power is how network code reacts to packets, and it's also
//   where a lot of crashes and security bugs hide.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN server is a loop that says: "a packet arrived, which function
//   handles it?" That question is answered with function pointers, lambdas
//   and callbacks (all in this chapter). The other half of the chapter is
//   about MEMORY for functions (the stack) and input you can't trust
//   (command-line args, recursion depth, format strings).
//
//       packet arrives ──► read type byte ──► CHECK it ──► call handler[type]
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch20_functions.cpp -o ch20 && ./ch20 --listen 51820
// =============================================================================

#include <algorithm>   // std::sort, std::find_if
#include <arpa/inet.h> // inet_pton (turns "10.0.0.2" into 4 bytes)
#include <array>
#include <charconv>    // std::from_chars (safe string → number)
#include <cstdarg>     // va_list, va_start, va_arg, va_end (ellipsis)
#include <cstdint>
#include <cstdio>      // std::printf
#include <functional>  // std::function
#include <iostream>
#include <string>
#include <string_view>
#include <sys/socket.h> // AF_INET
#include <netinet/in.h> // in_addr
#include <utility>     // std::move
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// =============================================================================
// 20.1  Function Pointers
// =============================================================================
// IN PLAIN WORDS
//   A function lives somewhere in memory, just like a variable. A "function
//   pointer" is a variable that holds the ADDRESS of a function. You can
//   call the function through the pointer.
//
//       void (*handler)(const Bytes&)  =  &handleData;
//       └─┬┘  └──┬───┘ └────┬─────┘
//      returns  name    parameter list
//
//   A "dispatch table" is an array of function pointers. You pick the
//   function by index instead of writing a long if/else chain.
//
// WHY A VPN CARES
//   Every packet starts with a type byte: 0 = handshake, 1 = data,
//   2 = keepalive. The table picks the handler. BUT the type byte comes
//   from the network, so an attacker chooses it. If they send 200 and you
//   do table[200], you read past the end of the array and jump to a random
//   address. That's how "remote code execution" bugs happen.
//
// RULE
//   Bounds-check any index that came from outside BEFORE using it.
//   std::function (below) is a friendlier type that can hold any callable.
void handleHandshake(const Bytes& p) { std::cout << "  handshake handler, " << p.size() << " bytes\n"; }
void handleData(const Bytes& p) { std::cout << "  data handler, " << p.size() << " bytes\n"; }
void handleKeepalive(const Bytes&) { std::cout << "  keepalive handler (peer is alive)\n"; }

using Handler = void (*)(const Bytes&);   // alias: "pointer to a function taking const Bytes&"

// Index = message type. Order matters!
constexpr std::array<Handler, 3> kHandlers{handleHandshake, handleData, handleKeepalive};

bool dispatch(const Bytes& packet)
{
    if (packet.empty()) {
        std::cout << "  rejected: empty packet\n";
        return false;
    }
    Byte type = packet[0];                 // attacker-controlled!
    if (type >= kHandlers.size()) {        // THE IMPORTANT LINE
        std::cout << "  rejected: unknown type " << static_cast<int>(type) << "\n";
        return false;
    }
    kHandlers[type](packet);               // safe: we checked the index
    return true;
}

void lesson_20_1()
{
    std::cout << "--- 20.1 function pointers ---\n";
    Handler h = handleData;                // the & is optional for functions
    h(Bytes{1, 2, 3});                     // call through the pointer

    dispatch(Bytes{0, 0xAA, 0xBB});        // type 0 → handshake
    dispatch(Bytes{2});                    // type 2 → keepalive
    dispatch(Bytes{200, 0x41});            // type 200 → rejected, not a crash

    // std::function: holds a plain function, a lambda (20.6), or a functor.
    // Slightly slower than a raw pointer, much easier to read and use.
    std::function<void(const Bytes&)> nicer{handleHandshake};
    nicer(Bytes(148, 0));                  // 148 = size of a WireGuard handshake initiation
    std::cout << '\n';
}

// =============================================================================
// 20.2  The stack and the heap
// =============================================================================
// IN PLAIN WORDS
//   Your program has two main places to put data:
//
//   STACK  Local variables live here. Super fast. Freed automatically when
//          the function returns. But SMALL: usually 8 MB for the main thread
//          (and often only 512 KB for extra threads on macOS).
//   HEAP   Memory you ask for at runtime (new, std::vector, std::string).
//          Big (gigabytes). A bit slower to get. Must be freed (vector does
//          it for you).
//
//   Each function call pushes a "stack frame" (its local variables). If the
//   frames grow past the limit you get a STACK OVERFLOW: the program crashes.
//
// WHY A VPN CARES
//   A 64 KB receive buffer on the stack is fine: the biggest UDP packet is
//   65,507 bytes. A 10 MB buffer on the stack = crash before you send a byte.
//
// RULE
//   Small, fixed-size buffers → stack (std::array). Big or variable size →
//   heap (std::vector).
void receiveOnePacket()
{
    std::array<Byte, 65536> buffer{};      // 64 KB on the stack: OK
    std::cout << "stack buffer: " << buffer.size() << " bytes (fine)\n";
}

void lesson_20_2()
{
    std::cout << "--- 20.2 stack vs heap ---\n";
    receiveOnePacket();

    // std::array<Byte, 10'000'000> huge{};   // 10 MB on the stack → STACK OVERFLOW crash
    Bytes huge(10'000'000);                   // 10 MB on the heap: fine. vector frees it.
    std::cout << "heap buffer:  " << huge.size() << " bytes (fine)\n\n";
}

// =============================================================================
// 20.3  Recursion
// =============================================================================
// IN PLAIN WORDS
//   A recursive function calls itself. Each call adds a stack frame (20.2).
//   It needs a "base case" (when to stop), or it never stops.
//
//   Example format: TLV options. TLV = Type, Length, Value:
//       [ type: 1 byte ][ length: 2 bytes ][ value: 'length' bytes ]
//   Type 2 means "container": its value is MORE options inside. So options
//   can be nested, and a recursive parser is the natural way to read them.
//
// WHY A VPN CARES
//   The nesting depth comes from the packet. An attacker sends 100,000
//   nested containers → 100,000 stack frames → stack overflow → your VPN
//   crashes. That's a DoS (Denial of Service: knocking the service offline).
//
// RULE
//   Recursion on untrusted input MUST have a depth limit. Or use a loop.
constexpr int kMaxDepth = 8;

// Returns the number of leaf options, or -1 if the input is bad.
int parseOptions(const Bytes& buf, std::size_t begin, std::size_t end, int depth)
{
    if (depth > kMaxDepth) return -1;                // THE IMPORTANT LINE
    int leaves = 0;
    std::size_t i = begin;
    while (i < end) {
        if (end - i < 3) return -1;                  // not enough room for a header
        Byte type = buf[i];
        std::size_t len = static_cast<std::size_t>((buf[i + 1] << 8) | buf[i + 2]);
        std::size_t valueStart = i + 3;
        if (len > end - valueStart) return -1;       // length lies about the size
        if (type == 2) {                             // container → recurse
            int inner = parseOptions(buf, valueStart, valueStart + len, depth + 1);
            if (inner < 0) return -1;
            leaves += inner;
        } else {
            ++leaves;
        }
        i = valueStart + len;
    }
    return leaves;
}

// Wrap 'inner' in a container option n times.
Bytes nest(Bytes inner, int n)
{
    for (int k = 0; k < n; ++k) {   // a LOOP builds it, no recursion needed
        auto len = static_cast<std::uint16_t>(inner.size());
        Bytes wrapped{2, static_cast<Byte>(len >> 8), static_cast<Byte>(len & 0xFF)};
        wrapped.insert(wrapped.end(), inner.begin(), inner.end());
        inner = wrapped;
    }
    return inner;
}

void lesson_20_3()
{
    std::cout << "--- 20.3 recursion ---\n";
    Bytes leaf{1, 0, 1, 0xAA};       // one leaf option: type 1, length 1, value 0xAA

    Bytes friendly = nest(leaf, 3);
    std::cout << "3 levels deep:   leaves = "
              << parseOptions(friendly, 0, friendly.size(), 0) << "\n";   // 1

    Bytes attack = nest(leaf, 100);
    std::cout << "100 levels deep: result = "
              << parseOptions(attack, 0, attack.size(), 0) << " (rejected)\n\n";   // -1
}

// =============================================================================
// 20.4  Command line arguments
// =============================================================================
// IN PLAIN WORDS
//   main can take two parameters:
//       int main(int argc, char* argv[])
//   argc = "argument count". argv = "argument vector", an array of C strings.
//   argv[0] is the program's own name. For:
//       ./vpn --listen 51820 --peer 10.0.0.2:51820
//   argc is 5, argv[1] is "--listen", argv[2] is "51820", and so on.
//   Every argument is TEXT. "51820" is a string until you convert it.
//
// WHY A VPN CARES
//   Typos and bad values must fail LOUDLY at startup: port 99999, a missing
//   value after --peer, "10.0.0.999". A VPN listening on the wrong port, or
//   on port 0, is worse than one that refuses to start.
//
// RULE
//   Copy argv into std::string / std::string_view, check every value, and
//   use std::from_chars (not atoi, which returns 0 for garbage).
struct Config {
    int listenPort{0};
    std::string peerIp{};
    int peerPort{0};
};

bool parsePort(std::string_view text, int& out)
{
    int value{};
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    bool usedAllText = (ptr == text.data() + text.size());
    if (ec != std::errc{} || !usedAllText || value < 1 || value > 65535) return false;
    out = value;
    return true;
}

bool parseArgs(const std::vector<std::string>& args, Config& cfg)
{
    for (std::size_t i = 1; i < args.size(); ++i) {   // skip args[0] (program name)
        const std::string& flag = args[i];
        if (i + 1 >= args.size()) {
            std::cout << "  error: " << flag << " needs a value\n";
            return false;
        }
        const std::string& value = args[++i];
        if (flag == "--listen") {
            if (!parsePort(value, cfg.listenPort)) {
                std::cout << "  error: bad port '" << value << "'\n";
                return false;
            }
        } else if (flag == "--peer") {
            auto colon = value.rfind(':');
            if (colon == std::string::npos) {
                std::cout << "  error: --peer needs ip:port\n";
                return false;
            }
            std::string ip = value.substr(0, colon);
            in_addr parsed{};
            if (inet_pton(AF_INET, ip.c_str(), &parsed) != 1) {   // 1 = valid IPv4
                std::cout << "  error: bad IP '" << ip << "'\n";
                return false;
            }
            if (!parsePort(std::string_view{value}.substr(colon + 1), cfg.peerPort)) {
                std::cout << "  error: bad peer port\n";
                return false;
            }
            cfg.peerIp = ip;
        } else {
            std::cout << "  error: unknown flag " << flag << "\n";
            return false;
        }
    }
    return true;
}

void tryArgs(const std::vector<std::string>& args)
{
    Config cfg{};
    std::cout << "args:";
    for (const auto& a : args) std::cout << ' ' << a;
    std::cout << '\n';
    if (parseArgs(args, cfg)) {
        std::cout << "  OK: listen=" << cfg.listenPort;
        if (!cfg.peerIp.empty()) std::cout << " peer=" << cfg.peerIp << ':' << cfg.peerPort;
        std::cout << '\n';
    }
}

void lesson_20_4(int argc, char* argv[])
{
    std::cout << "--- 20.4 command line arguments ---\n";
    std::vector<std::string> real(argv, argv + argc);   // copy into safe strings
    std::cout << "this run got argc=" << argc << '\n';
    tryArgs(real);

    // Fake command lines, so you see both good and bad cases:
    tryArgs({"vpn", "--listen", "51820", "--peer", "10.0.0.2:51820"});
    tryArgs({"vpn", "--listen", "99999"});
    tryArgs({"vpn", "--listen", "51820abc"});
    tryArgs({"vpn", "--peer", "10.0.0.999:51820"});
    tryArgs({"vpn", "--peer"});
    std::cout << '\n';
}

// =============================================================================
// 20.5  Ellipsis (and why to avoid them)
// =============================================================================
// IN PLAIN WORDS
//   "..." in a parameter list (an "ellipsis") means "any number of
//   arguments of ANY type". printf works this way. The catch: the function
//   has NO idea what types or how many arguments you passed. You must tell
//   it some other way (a count, or a format string like "%d %s").
//   If you lie, it reads garbage memory. The compiler can't check it.
//
// WHY A VPN CARES (the format string bug)
//   printf reads its FORMAT string to decide how many arguments to grab.
//       printf(userInput);          // BUG
//   If userInput is "%s%s%s%s", printf goes looking for 4 string arguments
//   that don't exist and reads random stack memory (crash, or leaked
//   secrets like keys). "%n" even WRITES to memory. If a peer's name or a
//   log message from the network ends up as the format → you are owned.
//       printf("%s", userInput);    // SAFE: userInput is just data
//
// RULE
//   Don't write ellipsis functions. Use std::vector, std::initializer_list,
//   or variadic templates (later chapter). Never let outside text be a
//   format string. Prefer std::cout.
int sumBytes(int count, ...)   // old C style. Shown so you recognize it.
{
    va_list args;
    va_start(args, count);
    int total = 0;
    for (int i = 0; i < count; ++i)
        total += va_arg(args, int);   // we just HOPE each argument is an int
    va_end(args);
    return total;
}

void lesson_20_5()
{
    std::cout << "--- 20.5 ellipsis ---\n";
    std::cout << "sumBytes(3, 20, 8, 1400) = " << sumBytes(3, 20, 8, 1400) << '\n';
    // sumBytes(3, 20, 8);      // says 3, passes 2 → reads garbage. Compiles fine!
    // sumBytes(1, 2.5);        // passes a double, read as int → garbage. Compiles fine!

    std::string peerName = "bob%s%s%s%n";   // pretend this came from the network
    // std::printf(peerName.c_str());       // FORMAT STRING BUG. clang warns
    //                                      // (-Wformat-security), but only sometimes.
    std::printf("peer name (safe printf): %s\n", peerName.c_str());
    std::cout << "peer name (cout, also safe): " << peerName << "\n\n";
}

// =============================================================================
// 20.6  Introduction to lambdas (anonymous functions)
// =============================================================================
// IN PLAIN WORDS
//   A lambda is a function with no name that you write right where you use
//   it:
//       [](int a, int b) { return a < b; }
//       └┘ └──────────┘ └───────────────┘
//    capture  params        body
//   (Captures are lesson 20.7.) Store one in 'auto' or std::function.
//
// WHY A VPN CARES
//   Callbacks are everywhere: "when a packet arrives, call THIS", "sort
//   peers by THIS rule", "find the peer where THIS is true". A lambda puts
//   the rule right next to the code that uses it.
//
// RULE
//   Short, one-off logic → lambda. Reused or long logic → a named function.
struct PeerInfo {
    PeerId id{};
    std::string endpoint{};
    Counter lastSeen{};   // seconds since start
};

void lesson_20_6()
{
    std::cout << "--- 20.6 lambdas ---\n";
    std::vector<PeerInfo> peers{{7, "1.2.3.4:51820", 50}, {3, "5.6.7.8:51820", 90},
                                {9, "9.9.9.9:51820", 10}};

    // Comparator: most recently seen first.
    std::sort(peers.begin(), peers.end(),
              [](const PeerInfo& a, const PeerInfo& b) { return a.lastSeen > b.lastSeen; });
    std::cout << "sorted by lastSeen:";
    for (const auto& p : peers) std::cout << " peer" << p.id << "(" << p.lastSeen << ")";
    std::cout << '\n';   // peer3(90) peer7(50) peer9(10)

    auto it = std::find_if(peers.begin(), peers.end(),
                           [](const PeerInfo& p) { return p.id == 9; });
    if (it != peers.end()) std::cout << "found peer9 at " << it->endpoint << '\n';

    // A callback stored for later: "what to do when a packet arrives".
    std::function<void(PeerId, const Bytes&)> onPacket =
        [](PeerId from, const Bytes& data) {
            std::cout << "onPacket: " << data.size() << " bytes from peer" << from << '\n';
        };
    onPacket(7, Bytes(1420));   // the network loop would call this
    std::cout << '\n';
}

// =============================================================================
// 20.7  Lambda captures
// =============================================================================
// IN PLAIN WORDS
//   A lambda can't see local variables unless it CAPTURES them:
//     [x]   copy x into the lambda (by value). Later changes to x: not seen.
//     [&x]  refer to the real x (by reference). Sees changes, no copy.
//     [=]   copy everything used.   [&]  reference everything used.
//   By-value captures are read-only unless you add 'mutable'.
//
// WHY A VPN CARES (dangling references)
//   VPNs use TIMERS: "if no reply in 5 seconds, resend the handshake". The
//   callback runs LATER, after the function that created it has returned.
//   If it captured a local by reference, that local is gone. The callback
//   reads dead stack memory = undefined behavior (UB: anything can happen).
//
// RULE
//   If the lambda is called right now → [&] is fine.
//   If the lambda is STORED and called later → capture by value.
struct TimerQueue {
    std::vector<std::function<void()>> pending{};
    void after(std::function<void()> fn) { pending.push_back(std::move(fn)); }
    void fireAll()
    {
        for (auto& fn : pending) fn();
        pending.clear();
    }
};

void scheduleRetry(TimerQueue& timers, PeerId peer)
{
    int attempt = 1;   // a local: dies when this function returns

    // BAD (commented out): captures 'attempt' by reference. When the timer
    // fires, scheduleRetry is long gone → dangling reference → UB.
    // timers.after([&attempt, peer] { std::cout << "retry #" << attempt << '\n'; });

    // GOOD: copy the values into the lambda. The copy lives as long as the lambda.
    timers.after([attempt, peer] {
        std::cout << "timer: retry handshake #" << attempt << " to peer" << peer << '\n';
    });
}

void lesson_20_7()
{
    std::cout << "--- 20.7 lambda captures ---\n";
    Counter sent = 0;
    auto byValue = [sent] { return sent; };        // copied NOW (0)
    auto byRef = [&sent] { return sent; };         // looks at the real 'sent'
    sent = 5;
    std::cout << "byValue sees " << byValue() << ", byRef sees " << byRef() << '\n';   // 0, 5

    // mutable: the lambda's OWN copy can change. The outside 'sent' does not.
    auto nextCounter = [sent]() mutable { return ++sent; };
    nextCounter();
    std::cout << "nextCounter() twice → " << nextCounter() << ", outside sent still " << sent
              << '\n';   // 7, 5

    TimerQueue timers{};
    scheduleRetry(timers, 7);   // returns; its local 'attempt' is gone
    timers.fireAll();           // safe, because we captured by value
    std::cout << '\n';
}

int main(int argc, char* argv[])
{
    lesson_20_1();
    lesson_20_2();
    lesson_20_3();
    lesson_20_4(argc, argv);
    lesson_20_5();
    lesson_20_6();
    lesson_20_7();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. In 20.1, delete the 'type >= kHandlers.size()' check and build with
//      -fsanitize=address. Run it. Read what AddressSanitizer says about
//      dispatch(Bytes{200, 0x41}). Put the check back.
//   2. Rewrite parseOptions (20.3) WITHOUT recursion: use a loop and a
//      std::vector<std::size_t> as your own stack of "end" positions. Keep
//      the depth limit.
//   3. Add a '--keepalive SECONDS' flag to parseArgs (20.4) that accepts
//      1..3600 and rejects everything else. Test it with tryArgs.
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why must you check the type byte BEFORE indexing the dispatch table?
//   - Why is printf(userInput) dangerous but printf("%s", userInput) safe?
//   - A timer callback runs 5 seconds later. Why must it capture by value?
// =============================================================================
