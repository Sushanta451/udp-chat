// =============================================================================
// Networking 4 — Putting it together: UDP sockets + poll(), in one process
// =============================================================================
//
// WHY THIS MATTERS FOR THE CHAT
//   This IS the chat's engine, minus the keyboard. Two UDP sockets on
//   127.0.0.1 ("alice" and "bob") send numbered packets to each other.
//   Alice "drops" some on purpose, and bob's poll() loop receives the rest.
//   Most chapters show up here:
//     ch10 fixed-width ints    ch22 RAII socket class   ch11 big-endian
//     ch12 sockaddr pointers   ch14 classes             net1 errno + exceptions
//     ch16/17 vector / array   net2 random + chrono
//
// BEEJ SECTIONS (read these first)
//   7.1 Blocking   7.2 poll()   7.6 Son of Data Encapsulation (packet format)
//   Assumes you've done net3 (Beej ch 3 + 5): sockaddr, bind, sendto, recvfrom.
//
// TOPICS
//   1. socket() → bind() → sendto() / recvfrom()  (UDP has no connect/accept)
//   2. Port 0 + getsockname(): let the OS pick a free port
//   3. Packet format: [4-byte big-endian seq][text bytes]
//   4. poll(): wait on several file descriptors with a timeout
//   5. The real chat's loop: poll(stdin + socket)
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra net4_beej_ch7_udp_and_poll.cpp -o net4 && ./net4
// =============================================================================

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

// ---- RAII socket (ch22) ---------------------------------------------------
class Socket {
public:
    Socket() : fd_(::socket(AF_INET, SOCK_DGRAM, 0))
    {
        if (fd_ == -1) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }
    }
    ~Socket()
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    Socket& operator=(Socket&& o) noexcept
    {
        if (this != &o) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = std::exchange(o.fd_, -1);
        }
        return *this;
    }
    [[nodiscard]] int fd() const { return fd_; }

private:
    int fd_ = -1;
};

// ---- Addresses (ch12) -------------------------------------------------------
// Bind to 127.0.0.1 on port 0. Port 0 = "OS, pick any free port for me".
// Then ask the OS which port it picked with getsockname().
sockaddr_in bindLoopback(const Socket& s)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // 127.0.0.1

    // bind wants the GENERIC sockaddr*, so we cast (the sockaddr trick).
    if (::bind(s.fd(), reinterpret_cast<const sockaddr*>(&addr), sizeof addr) == -1) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }
    socklen_t len = sizeof addr;
    if (::getsockname(s.fd(), reinterpret_cast<sockaddr*>(&addr), &len) == -1) {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }
    return addr;   // now holds the real port
}

// ---- Packet format (ch16 + ch11) --------------------------------------------
// [ seq: 4 bytes, big-endian ][ text: the rest ]
constexpr std::size_t kHeaderSize = 4;

std::vector<std::uint8_t> encode(std::uint32_t seq, std::string_view text)
{
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + text.size());
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(seq >> shift));
    }
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

struct Packet {
    std::uint32_t seq;
    std::string text;
};

// Network data is UNTRUSTED. A short packet must not crash us: return nullopt.
std::optional<Packet> decode(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < kHeaderSize) {
        return std::nullopt;
    }
    std::uint32_t seq = 0;
    for (std::size_t i = 0; i < kHeaderSize; ++i) {
        seq = (seq << 8) | bytes[i];
    }
    auto body = bytes.subspan(kHeaderSize);
    return Packet{seq, std::string(body.begin(), body.end())};
}

// ---- Send / receive ------------------------------------------------------------
void sendPacket(const Socket& s, const sockaddr_in& to, std::span<const std::uint8_t> data)
{
    auto sent = ::sendto(s.fd(), data.data(), data.size(), 0,
                         reinterpret_cast<const sockaddr*>(&to), sizeof to);
    if (sent == -1) {
        throw std::system_error(errno, std::generic_category(), "sendto");
    }
}

std::optional<Packet> receivePacket(const Socket& s)
{
    std::array<std::uint8_t, 1500> buf{};   // one Ethernet frame's worth
    sockaddr_storage from{};                 // big enough for IPv4 or IPv6
    socklen_t fromLen = sizeof from;
    auto n = ::recvfrom(s.fd(), buf.data(), buf.size(), 0,
                        reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n < 0) {
        return std::nullopt;
    }
    // Only the first n bytes are real. span.first(n) = exactly those.
    return decode(std::span{buf}.first(static_cast<std::size_t>(n)));
}

// -----------------------------------------------------------------------------
// poll(), explained
// -----------------------------------------------------------------------------
// recvfrom() BLOCKS: it waits forever until a packet arrives. The chat must
// ALSO watch the keyboard. Can't block on both at once... unless you ask the
// OS to watch several fds for you. That's poll():
//
//   pollfd fds[2] = {{STDIN_FILENO, POLLIN, 0},   // keyboard
//                    {sock.fd(),    POLLIN, 0}};  // network
//   int ready = poll(fds, 2, timeoutMs);
//     ready == 0   → timeout, nothing happened
//     ready  > 0   → check each fds[i].revents & POLLIN, and read THAT one
//     ready == -1  → error (EINTR = interrupted by a signal: just retry)
//
// THE REAL CHAT LOOP, in pseudocode:
//   while (running) {
//       poll(stdin + socket, 1000ms)
//       if stdin ready:  read a line; "/quit" → stop; else encode(seq++, line),
//                        maybe drop it (--drop), sendto(peer)
//       if socket ready: recvfrom → decode → tracker.onReceive(seq) → print
//                        the message and any [warn] gap / late / duplicate
//   }
// This file runs the same loop with only the socket, so it doesn't wait for
// you to type.

int main()
{
    try {
        Socket alice;
        Socket bob;
        sockaddr_in aliceAddr = bindLoopback(alice);
        sockaddr_in bobAddr = bindLoopback(bob);
        std::cout << "alice on port " << ntohs(aliceAddr.sin_port) << ", bob on port "
                  << ntohs(bobAddr.sin_port) << "\n\n";

        // Alice sends 8 messages, dropping ~25% (fixed seed = same result every run).
        std::mt19937 rng(7);
        std::uniform_int_distribution<int> roll(0, 99);
        for (std::uint32_t seq = 1; seq <= 8; ++seq) {
            std::string text = "message " + std::to_string(seq);
            if (roll(rng) < 25) {
                std::cout << "[alice] #" << seq << " (dropped on purpose)\n";
                continue;
            }
            sendPacket(alice, bobAddr, encode(seq, text));
            std::cout << "[alice] #" << seq << " sent\n";
        }
        // A duplicate, and a garbage 2-byte packet, to test bob's defenses.
        sendPacket(alice, bobAddr, encode(2, "message 2"));
        std::array<std::uint8_t, 2> junk{0xde, 0xad};
        sendPacket(alice, bobAddr, junk);
        std::cout << '\n';

        // Bob's receive loop.
        std::unordered_set<std::uint32_t> seen;
        std::uint32_t highest = 0;
        while (true) {
            pollfd pfd{bob.fd(), POLLIN, 0};
            int ready = ::poll(&pfd, 1, 200);   // wait up to 200 ms
            if (ready == -1 && errno == EINTR) {
                continue;
            }
            if (ready <= 0) {
                std::cout << "[bob] quiet for 200ms, done\n";
                break;
            }
            auto pkt = receivePacket(bob);
            if (!pkt) {
                std::cout << "[warn] malformed packet ignored\n";
                continue;
            }
            // A tiny version of the tracker you'll build in the assignment.
            if (!seen.insert(pkt->seq).second) {
                std::cout << "[warn] duplicate: #" << pkt->seq << '\n';
                continue;
            }
            for (std::uint32_t missing = highest + 1; missing < pkt->seq; ++missing) {
                std::cout << "[warn] gap: #" << missing << " missing\n";
            }
            highest = std::max(highest, pkt->seq);
            std::cout << "[bob] #" << pkt->seq << ' ' << pkt->text << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}

// =============================================================================
// TRY IT
//   1. Change the seed 7 to other numbers and watch the gaps move.
//   2. Add STDIN_FILENO to the poll set, and when it's ready, read a line with
//      std::getline and send it from alice. Now you have half a chat.
//   3. This version prints a gap as soon as a later number arrives. What if
//      the "missing" one shows up after that (it was LATE, not lost)? The
//      assignment handles that case.
// =============================================================================
