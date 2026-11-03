// =============================================================================
// Networking 3 — Beej's Guide chapters 3 and 5, in C++
// =============================================================================
//
// WHAT THIS FILE IS
//   Beej teaches sockets in C. The OS API (socket, bind, sendto...) IS a C
//   API, so you still call the same functions from C++. What C++ changes is
//   how you MANAGE them: RAII classes instead of "remember to close()",
//   std::string instead of char buffers, exceptions instead of perror+exit.
//
// SECTIONS
//   Ch 3.1  IP addresses + ports            (what they are)
//   Ch 3.2  Byte order                       (htons / ntohs, why)
//   Ch 3.3  The sockaddr structs             (the "address envelope" types)
//   Ch 3.4  inet_pton / inet_ntop            (string <-> binary IP)
//   Ch 5.1  getaddrinfo                      (the modern way to fill structs)
//   Ch 5.2  socket()                         (get a file descriptor)
//   Ch 5.3  bind()                           (claim a port)
//   Ch 5.8  sendto() / recvfrom()            (THE UDP calls)
//   Ch 5.4  connect() on UDP                 (optional shortcut)
//   Ch 5.9  close()                          (done by RAII here)
//   Ch 5.10 getpeername / getsockname        (who am I / who are you)
//   Ch 5.11 gethostname
//   (Skipped on purpose: listen/accept/send/recv for TCP. UDP chat doesn't
//    use them. They're in Beej 5.5–5.7 if you're curious.)
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra net3_beej_ch3_ch5_sockets.cpp -o net3
//   ./net3                 # looks up "localhost"
//   ./net3 example.com     # looks up any host name
// =============================================================================

#include <arpa/inet.h>   // inet_pton, inet_ntop, htons...
#include <netdb.h>       // getaddrinfo, freeaddrinfo, gai_strerror
#include <netinet/in.h>  // sockaddr_in, sockaddr_in6
#include <sys/socket.h>  // socket, bind, sendto, recvfrom, sockaddr
#include <unistd.h>      // close, gethostname

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

// -----------------------------------------------------------------------------
// Helper: turn "errno + function name" into a C++ exception.
// Beej does perror("bind"); exit(1);  — we throw instead so the caller decides.
// -----------------------------------------------------------------------------
[[noreturn]] void throw_errno(const char* what)
{
    throw std::system_error(errno, std::generic_category(), what);
}

// =============================================================================
// CH 3.1 — IP ADDRESSES AND PORTS
// =============================================================================
//   IPv4 address = 32 bits  (4 bytes)  written "192.0.2.111"
//   IPv6 address = 128 bits (16 bytes) written "2001:db8::51"
//   Port         = 16 bits  (2 bytes)  0..65535. Ports < 1024 need root.
//
//   Analogy: IP = street address of a hotel, port = room number.
//   Loopback ("this machine"): 127.0.0.1 (v4) or ::1 (v6).
//   Private networks (behind a NAT router): 10.x.x.x, 192.168.x.x, 172.16-31.x.x
//
//   NOTE: these sizes matter. A port is uint16_t. An IPv4 address is
//   uint32_t (NOT uint16_t!). The address family is a small int (sa_family_t).

// =============================================================================
// CH 3.2 — BYTE ORDER
// =============================================================================
//   The number 0x1234 can be stored in memory two ways:
//     Big-endian    (network order): 12 34   <- the internet uses this
//     Little-endian (Intel, Apple M-series): 34 12
//
//   RULE: every multi-byte integer you put on the wire goes through hton*.
//         Every one you take off the wire goes through ntoh*.
//     htons = host-to-network short (16 bit)  -> ports
//     htonl = host-to-network long  (32 bit)  -> IPv4 addrs, your seq numbers
//   On a big-endian machine they do nothing, so it's always safe to call them.

void demo_byte_order()
{
    std::cout << "\n== Ch 3.2 byte order ==\n";
    std::uint16_t port = 0x1234;
    std::uint16_t net_port = htons(port);

    // Look at the raw bytes in memory.
    auto dump = [](const char* label, std::uint16_t v) {
        unsigned char bytes[2];
        std::memcpy(bytes, &v, 2);
        std::cout << "  " << label << ": " << std::hex << std::setfill('0')
                  << std::setw(2) << int(bytes[0]) << ' ' << std::setw(2)
                  << int(bytes[1]) << std::dec << '\n';
    };
    dump("host order    0x1234", port);      // your Mac: 34 12
    dump("network order 0x1234", net_port);  // always:   12 34
    std::cout << "  round trip ntohs(htons(x)) == x ? "
              << (ntohs(net_port) == port ? "yes" : "no") << '\n';
}

// =============================================================================
// CH 3.3 — THE SOCKADDR STRUCTS (the confusing part)
// =============================================================================
//   The OS functions (bind, sendto...) take a `sockaddr*`. That's a GENERIC
//   pointer type — like a base class in C. The real data lives in a more
//   specific struct, and you cast its address.
//
//   struct sockaddr          generic "header". 16 bytes. Too small for IPv6.
//   struct sockaddr_in       IPv4: sin_family, sin_port, sin_addr (in_addr)
//   struct sockaddr_in6      IPv6: sin6_family, sin6_port, sin6_addr (in6_addr), ...
//   struct sockaddr_storage  big enough for ANY of them. Use it when RECEIVING
//                            (recvfrom) since you don't know v4 or v6 yet.
//
//   All of them START with the family field, so the OS reads that first,
//   then knows which shape the rest is.
//
//   DON'T define your own version of these. Use the system headers' ones;
//   the layout must match exactly what the kernel expects (and it differs
//   per OS — macOS has an extra sin_len byte at the front that Linux doesn't).
//
//   C++ tip: the cast is reinterpret_cast<sockaddr*>(&addr). That's the one
//   place in networking code where reinterpret_cast is normal and expected.

// =============================================================================
// CH 3.4 — inet_pton / inet_ntop
// =============================================================================
//   pton = "presentation to network": "10.0.0.1" -> 4 binary bytes
//   ntop = "network to presentation": 4 binary bytes -> "10.0.0.1"
//   pton returns 1 = ok, 0 = not a valid address, -1 = error. CHECK IT.
//   (inet_addr / inet_aton / inet_ntoa are old IPv4-only versions. Skip them.)

// Build an IPv4 address struct by hand. This is the "old way" (Beej 5.3).
sockaddr_in make_ipv4(const std::string& ip, std::uint16_t port)
{
    sockaddr_in addr{};                 // {} zero-fills everything (= memset 0)
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);        // <- byte order!
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        throw std::invalid_argument("not an IPv4 address: " + ip);
    }
    return addr;
}

// Turn ANY sockaddr (v4 or v6) into "ip:port". Works on sockaddr_storage.
std::string to_string(const sockaddr* sa)
{
    std::array<char, INET6_ADDRSTRLEN> buf{};  // big enough for v6, so v4 too
    std::uint16_t port = 0;

    if (sa->sa_family == AF_INET) {
        const auto* v4 = reinterpret_cast<const sockaddr_in*>(sa);
        inet_ntop(AF_INET, &v4->sin_addr, buf.data(), buf.size());
        port = ntohs(v4->sin_port);
        return std::string(buf.data()) + ":" + std::to_string(port);
    }
    if (sa->sa_family == AF_INET6) {
        const auto* v6 = reinterpret_cast<const sockaddr_in6*>(sa);
        inet_ntop(AF_INET6, &v6->sin6_addr, buf.data(), buf.size());
        port = ntohs(v6->sin6_port);
        return "[" + std::string(buf.data()) + "]:" + std::to_string(port);
    }
    return "<unknown family>";
}

void demo_structs_and_pton()
{
    std::cout << "\n== Ch 3.3/3.4 structs + inet_pton/ntop ==\n";
    sockaddr_in a = make_ipv4("192.0.2.33", 4950);
    std::cout << "  built by hand: " << to_string(reinterpret_cast<sockaddr*>(&a))
              << '\n';
    std::cout << "  sizeof sockaddr_in      = " << sizeof(sockaddr_in) << '\n'
              << "  sizeof sockaddr_in6     = " << sizeof(sockaddr_in6) << '\n'
              << "  sizeof sockaddr_storage = " << sizeof(sockaddr_storage) << '\n';
    try {
        make_ipv4("999.1.1.1", 1);
    } catch (const std::exception& e) {
        std::cout << "  bad input caught: " << e.what() << '\n';
    }
}

// =============================================================================
// CH 5.1 — getaddrinfo (the modern way)
// =============================================================================
//   Instead of filling sockaddr_in by hand, ask the OS:
//     "give me addresses for host X, port Y, UDP please"
//   It does DNS, handles v4 AND v6, and gives you a LINKED LIST of results.
//   You MUST free that list with freeaddrinfo(). In C you just remember to.
//   In C++ we use unique_ptr with a custom deleter, so it's freed
//   automatically — even if an exception is thrown. That's RAII (ch22).
//
//   hints fields you care about:
//     ai_family   = AF_UNSPEC (either), AF_INET (v4 only), AF_INET6
//     ai_socktype = SOCK_DGRAM for UDP
//     ai_flags    = AI_PASSIVE when you're going to bind() (server side):
//                   pass nullptr as host and it fills in "any of my addresses"

struct AddrInfoDeleter {
    void operator()(addrinfo* p) const noexcept { freeaddrinfo(p); }
};
using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

AddrInfoPtr resolve(const char* host, const char* port, int family, bool passive)
{
    addrinfo hints{};
    hints.ai_family = family;
    hints.ai_socktype = SOCK_DGRAM;
    if (passive) hints.ai_flags = AI_PASSIVE;

    addrinfo* raw = nullptr;
    int rv = getaddrinfo(host, port, &hints, &raw);
    if (rv != 0) {
        // getaddrinfo does NOT use errno. It returns its own code.
        throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rv));
    }
    return AddrInfoPtr(raw);  // now owned; freed when it goes out of scope
}

// Beej's showip.c, in C++.
void demo_showip(const char* host)
{
    std::cout << "\n== Ch 5.1 getaddrinfo (showip " << host << ") ==\n";
    AddrInfoPtr list = resolve(host, "4950", AF_UNSPEC, false);
    for (addrinfo* p = list.get(); p != nullptr; p = p->ai_next) {
        std::cout << "  " << (p->ai_family == AF_INET ? "IPv4" : "IPv6") << ": "
                  << to_string(p->ai_addr) << '\n';
    }
}   // <- freeaddrinfo runs here automatically

// =============================================================================
// CH 5.2 / 5.9 — socket() and close(), wrapped in RAII
// =============================================================================
//   socket() gives you an int "file descriptor" (fd). It's a handle, like a
//   ticket number. -1 means failure. When done, close(fd) — or you leak it.
//   A class that owns the fd and closes it in its destructor means you can
//   never forget. Copying is deleted (two owners would double-close);
//   moving is allowed (ownership transfers).

class UdpSocket {
public:
    UdpSocket(int family, int socktype, int protocol)
        : fd_(::socket(family, socktype, protocol))
    {
        if (fd_ == -1) throw_errno("socket");
    }
    ~UdpSocket() { if (fd_ != -1) ::close(fd_); }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    UdpSocket& operator=(UdpSocket&& o) noexcept
    {
        if (this != &o) {
            if (fd_ != -1) ::close(fd_);
            fd_ = std::exchange(o.fd_, -1);
        }
        return *this;
    }

    int fd() const { return fd_; }

    // ---- CH 5.3 bind(): "I want to receive on THIS address/port" ----------
    // Servers (and both chat peers) bind so others know where to send.
    // Port 0 = "OS, pick any free port for me".
    void bind_to(const sockaddr* addr, socklen_t len)
    {
        if (::bind(fd_, addr, len) == -1) throw_errno("bind");
    }

    // ---- CH 5.8 sendto(): send one datagram to an address ------------------
    // UDP is message-based: one sendto() = one packet = one recvfrom().
    // Returns bytes sent. For UDP it's all-or-nothing (no partial sends
    // like TCP), but still check for -1.
    std::size_t send_to(const std::string& msg, const sockaddr* to, socklen_t len)
    {
        ssize_t n = ::sendto(fd_, msg.data(), msg.size(), 0, to, len);
        if (n == -1) throw_errno("sendto");
        return static_cast<std::size_t>(n);
    }

    // ---- CH 5.8 recvfrom(): wait for one datagram, learn who sent it -------
    // BLOCKS until a packet arrives (net4 shows poll() to avoid that).
    // `from` is sockaddr_storage because it might be v4 or v6.
    // fromlen is IN/OUT: you tell it the buffer size, it tells you the real size.
    // If a packet is bigger than your buffer, the rest is silently DROPPED.
    std::string recv_from(sockaddr_storage& from)
    {
        std::array<char, 1500> buf{};          // ~1 Ethernet MTU
        socklen_t fromlen = sizeof from;
        ssize_t n = ::recvfrom(fd_, buf.data(), buf.size(), 0,
                               reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (n == -1) throw_errno("recvfrom");
        return std::string(buf.data(), static_cast<std::size_t>(n));
    }

    // ---- CH 5.10 getsockname(): "what address/port did I end up on?" -------
    // (getpeername is the same idea for the OTHER side of a connected socket.)
    sockaddr_storage local_address() const
    {
        sockaddr_storage ss{};
        socklen_t len = sizeof ss;
        if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&ss), &len) == -1)
            throw_errno("getsockname");
        return ss;
    }

private:
    int fd_ = -1;
};

// =============================================================================
// THE UDP PATTERN — this is your chat in miniature
// =============================================================================
//   receiver: getaddrinfo(passive) -> socket -> bind -> recvfrom (loop)
//   sender:   getaddrinfo(host)    -> socket          -> sendto
//   A chat peer is BOTH: bind to its own port, sendto the other's port,
//   recvfrom on its own. No listen(), no accept() — that's TCP.

void demo_udp_loopback()
{
    std::cout << "\n== Ch 5.2/5.3/5.8 UDP on loopback ==\n";

    // "listener": bind 127.0.0.1, port 0 (OS picks).
    AddrInfoPtr laddr = resolve("127.0.0.1", "0", AF_INET, true);
    UdpSocket listener(laddr->ai_family, laddr->ai_socktype, laddr->ai_protocol);
    listener.bind_to(laddr->ai_addr, laddr->ai_addrlen);

    sockaddr_storage where = listener.local_address();
    std::cout << "  listener bound to "
              << to_string(reinterpret_cast<sockaddr*>(&where)) << '\n';

    // "talker": no bind needed. sendto() auto-binds a random local port.
    UdpSocket talker(AF_INET, SOCK_DGRAM, 0);
    std::size_t sent = talker.send_to("hello from talker",
                                      reinterpret_cast<sockaddr*>(&where),
                                      sizeof(sockaddr_in));
    std::cout << "  talker sent " << sent << " bytes\n";

    sockaddr_storage from{};
    std::string got = listener.recv_from(from);
    std::cout << "  listener got \"" << got << "\" from "
              << to_string(reinterpret_cast<sockaddr*>(&from)) << '\n';

    // Reply to whoever sent it: `from` is exactly the address to sendto().
    listener.send_to("hi back", reinterpret_cast<sockaddr*>(&from), sizeof(sockaddr_in));
    sockaddr_storage from2{};
    std::cout << "  talker got reply \"" << talker.recv_from(from2) << "\"\n";

    // ---- CH 5.4 connect() on a UDP socket ---------------------------------
    // No handshake happens! It just tells the kernel "default destination is X,
    // and drop packets from anyone else". Then send()/recv() work.
    // Handy for a 2-person chat: connect to your peer once.
    if (::connect(talker.fd(), reinterpret_cast<sockaddr*>(&where),
                  sizeof(sockaddr_in)) == -1)
        throw_errno("connect");
    const std::string m = "via connect+send";
    if (::send(talker.fd(), m.data(), m.size(), 0) == -1) throw_errno("send");
    sockaddr_storage from3{};
    std::cout << "  listener got \"" << listener.recv_from(from3) << "\"\n";
}   // both sockets close() here, automatically

// =============================================================================
// CH 5.11 — gethostname
// =============================================================================
void demo_hostname()
{
    std::cout << "\n== Ch 5.11 gethostname ==\n";
    std::array<char, 256> name{};
    if (::gethostname(name.data(), name.size()) == -1) throw_errno("gethostname");
    std::cout << "  this machine is \"" << name.data() << "\"\n";
}

int main(int argc, char* argv[])
{
    const char* host = (argc > 1) ? argv[1] : "localhost";
    try {
        demo_byte_order();
        demo_structs_and_pton();
        demo_showip(host);
        demo_udp_loopback();
        demo_hostname();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}

// =============================================================================
// TRY IT YOURSELF (do these before looking at net4)
// =============================================================================
//  1. Change demo_udp_loopback to use AF_INET6 and "::1". What else must change?
//     (Hint: the sizeof(sockaddr_in) calls. Better: carry the length around.)
//  2. Send a 3000-byte string. What does recv_from print? Why? (Ch 5.8 note.)
//  3. Run two copies of a tiny program: one binds port 4950 and loops on
//     recv_from; the other reads std::getline and sends each line. That's
//     a one-way chat. Then make both sides do both (you'll need poll — net4).
//  4. Put a uint32_t sequence number in front of each message, using htonl
//     on send and ntohl on receive. Print it on the other side.
// =============================================================================
