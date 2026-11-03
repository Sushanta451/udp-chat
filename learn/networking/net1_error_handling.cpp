// =============================================================================
// Networking 1 — Error handling: return codes, errno, exceptions, optional
// =============================================================================
//
// WHY THIS MATTERS FOR THE CHAT
//   Almost every socket call can fail: socket(), bind() (port already in use),
//   getaddrinfo() (bad host name), sendto(), recvfrom(). Users type bad flags
//   ("--port banana"). Packets arrive too short or corrupted. You need a plan
//   for each kind of failure.
//
// TOPICS
//   1. C-style return codes + errno + strerror (how the socket API reports errors)
//   2. Exceptions: throw / try / catch, and why RAII makes them safe
//   3. std::optional<T>: "maybe a value" (great for parsing)
//   4. Returning an error message too: a small Result type (std::expected in C++23)
//   5. assert: for bugs in YOUR code, not for bad input
//   6. noexcept
//   7. Which one do I use?
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra net1_error_handling.cpp -o net1 && ./net1
// =============================================================================

#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>       // errno
#include <charconv>     // std::from_chars: fast, no-exception number parsing
#include <cstdint>
#include <cstring>      // std::strerror
#include <iostream>
#include <optional>
#include <stdexcept>    // std::runtime_error, std::invalid_argument ...
#include <string>
#include <string_view>
#include <system_error> // std::system_error, std::generic_category
#include <variant>

// -----------------------------------------------------------------------------
// 1. Return codes + errno
// -----------------------------------------------------------------------------
// POSIX functions return -1 on failure and set the global 'errno' to a code
// (EADDRINUSE, EBADF, ECONNREFUSED ...). std::strerror(errno) gives text.
// RULE: read errno IMMEDIATELY after the failing call. Any other library call
// (even printing) may overwrite it.
// getaddrinfo is different: it returns its own code; use gai_strerror(code).
void errnoDemo()
{
    std::cout << "--- 1. errno ---\n";
    // Try to close a file descriptor that doesn't exist.
    if (::close(12345) == -1) {
        int err = errno;   // save it right away
        std::cout << "close failed: errno=" << err << " (" << std::strerror(err) << ")\n";
    }
    // Ask for an impossible socket.
    if (::socket(-1, SOCK_DGRAM, 0) == -1) {
        int err = errno;
        std::cout << "socket failed: " << std::strerror(err) << "\n\n";
    }
}

// -----------------------------------------------------------------------------
// 2. Exceptions
// -----------------------------------------------------------------------------
// throw stops the function and unwinds up the call stack until a matching
// catch. On the way, EVERY local object's destructor runs. That's why RAII
// matters: the unique_ptr / FileDescriptor cleanup still happens.
//
// Good fit: setup failures you can't continue from ("could not bind port").
// Turn the errno into an exception right at the C boundary, then the rest of
// your code doesn't need to check -1 everywhere.
//
// Catch by const reference: catch (const std::exception& e).
int openUdpSocketOrThrow(int family)
{
    int fd = ::socket(family, SOCK_DGRAM, 0);
    if (fd == -1) {
        // system_error carries errno and builds a readable message.
        throw std::system_error(errno, std::generic_category(), "socket()");
    }
    return fd;
}

void exceptionDemo()
{
    std::cout << "--- 2. exceptions ---\n";
    try {
        int fd = openUdpSocketOrThrow(AF_INET);
        std::cout << "got fd " << fd << '\n';
        ::close(fd);
        openUdpSocketOrThrow(-1);   // this one throws
        std::cout << "never printed\n";
    } catch (const std::system_error& e) {   // most specific first
        std::cout << "system_error: " << e.what() << " (code " << e.code().value() << ")\n";
    } catch (const std::exception& e) {      // the base of all standard exceptions
        std::cout << "other error: " << e.what() << '\n';
    }
    std::cout << '\n';
}

// -----------------------------------------------------------------------------
// 3. std::optional<T>
// -----------------------------------------------------------------------------
// optional<T> holds either a T or nothing (std::nullopt). Use it when "no
// answer" is a normal, expected outcome, not an emergency.
// Parsing user input is the classic case: "banana" isn't a port, and that's
// not exceptional, it's just bad input.
//
// std::from_chars: parses numbers without exceptions and without locale.
// Better than std::stoi (which throws) or atoi (which returns 0 for "banana",
// which is also a "valid" number. Nice).
std::optional<std::uint16_t> parsePort(std::string_view text)
{
    unsigned value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;   // not a number, or junk after the number ("90x")
    }
    if (value == 0 || value > 65535) {
        return std::nullopt;   // out of range for a port
    }
    return static_cast<std::uint16_t>(value);
}

void optionalDemo()
{
    std::cout << "--- 3. optional ---\n";
    for (std::string_view s : {"9000", "banana", "70000", "90x", "0"}) {
        if (auto port = parsePort(s)) {   // optional converts to bool: has a value?
            std::cout << '"' << s << "\" → port " << *port << '\n';   // * to get it
        } else {
            std::cout << '"' << s << "\" → invalid\n";
        }
    }
    // value_or gives a fallback:
    std::cout << "with fallback: " << parsePort("nope").value_or(9000) << "\n\n";
}

// -----------------------------------------------------------------------------
// 4. A value OR an error message
// -----------------------------------------------------------------------------
// optional can't say WHY it failed. C++23 adds std::expected<T, E> for that.
// We're on C++20, so here's the same idea with std::variant: it holds exactly
// one of the listed types.
struct ParseError {
    std::string message;
};
template <typename T>
using Result = std::variant<T, ParseError>;

struct PeerAddr {
    std::string host;
    std::uint16_t port;
};

// "127.0.0.1:9001" → PeerAddr, for the --peer flag.
Result<PeerAddr> parsePeer(std::string_view text)
{
    auto colon = text.rfind(':');
    if (colon == std::string_view::npos) {
        return ParseError{"missing ':' in '" + std::string(text) + "'"};
    }
    auto port = parsePort(text.substr(colon + 1));
    if (!port) {
        return ParseError{"bad port in '" + std::string(text) + "'"};
    }
    return PeerAddr{std::string(text.substr(0, colon)), *port};
}

void resultDemo()
{
    std::cout << "--- 4. value or error ---\n";
    for (std::string_view s : {"127.0.0.1:9001", "localhost", "10.0.0.2:http"}) {
        auto r = parsePeer(s);
        if (const auto* peer = std::get_if<PeerAddr>(&r)) {
            std::cout << "ok: host=" << peer->host << " port=" << peer->port << '\n';
        } else {
            std::cout << "error: " << std::get<ParseError>(r).message << '\n';
        }
    }
    std::cout << '\n';
}

// -----------------------------------------------------------------------------
// 5. assert
// -----------------------------------------------------------------------------
// assert(cond) crashes with a message if cond is false, but ONLY in debug
// builds (it vanishes when NDEBUG is defined, as in Release builds).
// Use it for "this can't happen unless MY code has a bug". Never for checking
// user input or network data: those CAN happen, so handle them properly.
std::uint32_t nextSeq(std::uint32_t current)
{
    assert(current != 0 && "sequence numbers start at 1");
    return current + 1;
}

// -----------------------------------------------------------------------------
// 6. noexcept
// -----------------------------------------------------------------------------
// noexcept promises the function never throws. If it does anyway, the
// program terminates. It matters most for MOVE constructors: std::vector only
// moves your objects during reallocation if the move is noexcept (otherwise
// it copies, to stay safe). That's why FileDescriptor's moves are noexcept.
// Destructors are noexcept automatically. Never throw from a destructor.

// -----------------------------------------------------------------------------
// 7. Which one?
// -----------------------------------------------------------------------------
//   Situation                                         Tool
//   ------------------------------------------------  ----------------------------
//   Calling a C/POSIX function                        check its return, read errno
//   Setup failed, program can't continue              throw (catch in main, print, exit 1)
//   Parsing input, "no result" is normal              std::optional
//   Parsing input, and the caller needs the reason    variant / expected / error struct
//   A packet off the network is malformed             return optional/error, DROP the
//                                                     packet, keep running. Never crash
//                                                     because of what a peer sent.
//   "This should be impossible"                       assert
//   In the hot loop (every packet)                    no exceptions; return values

int main()
{
    errnoDemo();
    exceptionDemo();
    optionalDemo();
    resultDemo();
    std::cout << "--- 5. assert ---\nnextSeq(1) = " << nextSeq(1) << '\n';
    // nextSeq(0);   // uncomment: aborts with "Assertion failed" in a debug build
    return 0;
}

// =============================================================================
// TRY IT
//   1. Uncomment nextSeq(0). Run. Then compile with -DNDEBUG and run again.
//   2. Add an IPv6 case to parsePeer: "[::1]:9001". (Hint: if it starts with
//      '[', find the ']'.)
//   3. In main, wrap everything in try/catch(const std::exception&) and return
//      1 on error. That's the standard shape of a real main().
// =============================================================================
