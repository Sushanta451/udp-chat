// =============================================================================
// Beej ch 3 practice: build a sockaddr_in yourself, then ASK THE KERNEL
// what it understood.
// =============================================================================
//
// You write one function: make_loopback(port). The checks below then:
//   1. look at the raw bytes of your struct (did you get network order?)
//   2. bind a real UDP socket to it
//   3. ask the kernel with getsockname() "what address + port am I bound to?"
// If the kernel says a different port than you asked for, that's the
// silent wrong-port bug from section 3.2, caught for real.
//
// BUILD + RUN
//   cmake --build learn/build && ./learn/build/ch03_practice
// =============================================================================

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

// Return a sockaddr_in meaning "127.0.0.1, on this port", ready to pass to bind().
sockaddr_in make_loopback(std::uint16_t port)
{
    sockaddr_in addr{};
    // TODO(human): fill in addr so it means 127.0.0.1:port.
    (void)port;  // delete this line once you use `port`
    return addr;
}

// ---- Everything below checks your work. Read it, don't edit it. -------------

void print_bytes(const char* label, const void* p, std::size_t n)
{
    const auto* b = static_cast<const unsigned char*>(p);
    std::cout << "  " << std::left << std::setw(10) << label << std::right;
    for (std::size_t i = 0; i < n; ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << int(b[i]) << ' ';
    std::cout << std::dec << std::setfill(' ') << '\n';
}

int main()
{
    const std::uint16_t wanted = 4950;  // 0x1356
    sockaddr_in a = make_loopback(wanted);

    std::cout << "Your struct's bytes:\n";
    print_bytes("sin_port", &a.sin_port, sizeof a.sin_port);  // want: 13 56
    print_bytes("sin_addr", &a.sin_addr, sizeof a.sin_addr);  // want: 7f 00 00 01

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) { std::perror("socket"); return 1; }

    if (::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) == -1) {
        std::perror("bind");  // EADDRNOTAVAIL here usually means a wrong sin_addr
        ::close(fd);
        return 1;
    }

    sockaddr_in got{};
    socklen_t len = sizeof got;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&got), &len);
    ::close(fd);

    std::array<char, INET_ADDRSTRLEN> ip{};
    ::inet_ntop(AF_INET, &got.sin_addr, ip.data(), ip.size());
    std::uint16_t got_port = ntohs(got.sin_port);

    std::cout << "\nThe kernel says you're bound to " << ip.data() << ':' << got_port << '\n';

    bool ok = (got_port == wanted) && (std::strcmp(ip.data(), "127.0.0.1") == 0);
    std::cout << (ok ? "PASS: the kernel sees exactly what you meant.\n"
                     : "FAIL: compare the bytes above with the 'want' comments.\n");
    return ok ? 0 : 1;
}
