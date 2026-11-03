// =============================================================================
// Networking 2 — Program structure: headers, multiple files, argv, random, time
// =============================================================================
//
// WHY THIS MATTERS FOR THE CHAT
//   main.cpp is one big file. The real chat should be split: packet code,
//   tracker code, socket code, and a small main. Then the tests can include
//   the packet/tracker code without dragging in sockets. The chat also needs
//   to parse --port/--peer/--drop, roll dice for --drop, and maybe timestamp.
//
// WHAT THIS FILE IS (AND ISN'T)
//   Also NOT networking. It's the GLUE every real program needs that school
//   exercises usually skip: splitting code into several files, how CMake
//   builds them, reading flags from the command line, random numbers, and
//   time. A one-file practice program doesn't need it. Your chat does.
//
// WHEN TO OPEN THIS FILE (the exact moments in YOUR chat)
//   You're...                                           Look at
//   ---------------------------------------------------  -------------------------
//   moving packet/tracker code out of main.cpp           2 headers vs .cpp
//   seeing "undefined symbol" or "duplicate symbol"      1 build pipeline, 3 ODR
//   adding a chatcore library to CMakeLists.txt          4 CMake
//     (assignment Part C, step 1)
//   reading --port 9000 --peer ... --drop 10             5 parseArgs + Options
//   making --drop "lose" a random 10% of messages        6 DropSimulator
//   giving poll() a timeout, timestamping messages       7 chronoDemo
//
// THE WHOLE FILE IN ONE SENTENCE
//   It's how a single-file experiment becomes a real program with flags,
//   several files, and tests.
//
// TOPICS
//   1. How a C++ program is built: preprocess → compile → link
//   2. Headers (.h) vs source files (.cpp). What goes where. #pragma once
//   3. The One Definition Rule (ODR), and 'inline'
//   4. How CMake fits in (this repo's CMakeLists.txt, explained)
//   5. Parsing command-line arguments (argc / argv)
//   6. <random>: simulating packet loss for --drop
//   7. <chrono>: time, durations, timeouts
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra net2_program_structure.cpp -o net2
//   ./net2 --port 9000 --peer 127.0.0.1:9001 --drop 25
// =============================================================================

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// -----------------------------------------------------------------------------
// 1. The build pipeline
// -----------------------------------------------------------------------------
//   PREPROCESS  #include literally pastes the header's text into your file.
//               #define / #if are handled here too.
//   COMPILE     each .cpp file (plus everything it included) becomes ONE
//               "translation unit" → an object file (.o). Compiled separately,
//               knowing nothing about the other .cpp files.
//   LINK        the linker glues all .o files + libraries together, matching
//               each function CALL to that function's ONE definition.
//
// Two classic error kinds:
//   compile error  "use of undeclared identifier"  → missing #include / typo
//   link error     "undefined symbol: foo()"       → declared but the .cpp that
//                                                    defines it isn't built/linked
//                  "duplicate symbol: foo()"       → defined in 2 places (ODR)

// -----------------------------------------------------------------------------
// 2. Headers vs source files
// -----------------------------------------------------------------------------
// HEADER (packet.h): DECLARATIONS. "These things exist, here is their shape."
//   - function declarations:    std::vector<std::uint8_t> encode(const Packet& p);
//   - class definitions (with member declarations)
//   - templates and constexpr/inline functions (bodies MUST be visible)
//   - constants: inline constexpr std::size_t kHeaderSize = 4;
//
// SOURCE (packet.cpp): DEFINITIONS. The function bodies. #includes its own
//   header first (catches mistakes where the header and the .cpp disagree).
//
// INCLUDE GUARD: #pragma once at the top of every header stops the same
//   header being pasted twice into one translation unit.
//
// RULES:
//   - Never #include a .cpp file.
//   - Never put 'using namespace std;' in a header (it leaks into every file
//     that includes it).
//   - Include what you use; don't rely on another header including it for you.
//
// See ../assignment/ for a real header/source/test split.

// -----------------------------------------------------------------------------
// 3. ODR and inline
// -----------------------------------------------------------------------------
// One Definition Rule: every non-inline function/variable must be DEFINED
// exactly once in the whole program.
// If you write a normal function BODY in a header and two .cpp files include
// it → two definitions → "duplicate symbol" link error.
// Fix: move the body to a .cpp, OR mark it 'inline' (which says "identical
// copies are allowed, pick one"). Templates, constexpr functions, and functions
// defined inside a class body are implicitly inline.

// -----------------------------------------------------------------------------
// 4. CMake, in one paragraph
// -----------------------------------------------------------------------------
// CMake doesn't compile anything. It GENERATES build files (Makefiles/Ninja)
// for your platform. In this repo:
//   add_executable(udpchat src/main.cpp)     "build a program from these files"
//   set(CMAKE_CXX_STANDARD 20)               "use C++20"
//   set_project_warnings(udpchat)            "-Wall -Wextra -Wconversion ..."
//   add_subdirectory(tests) + CTest          "ctest runs the tests"
// When you split code into files, the usual pattern is a LIBRARY for the
// logic, linked by both the app and the tests:
//   add_library(chatcore src/packet.cpp src/tracker.cpp)
//   add_executable(udpchat src/main.cpp)
//   target_link_libraries(udpchat PRIVATE chatcore)
//   add_executable(tests tests/tracker_test.cpp)
//   target_link_libraries(tests PRIVATE chatcore)

// -----------------------------------------------------------------------------
// 5. argc / argv
// -----------------------------------------------------------------------------
// argv is an array of C strings: argv[0] is the program name, argv[1..argc-1]
// are the words the user typed. Wrap them in string_views once, up front, and
// work with those.
struct Options {
    std::uint16_t port = 9000;
    std::string peer = "127.0.0.1:9001";
    int dropPercent = 0;
};

// Returns false (and prints why) on bad input. Real code would reuse
// parsePort from net1_error_handling.cpp instead of std::stoi.
bool parseArgs(int argc, char* argv[], Options& out)
{
    std::vector<std::string_view> args(argv + 1, argv + argc);   // skip argv[0]
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view flag = args[i];
        if (i + 1 >= args.size()) {
            std::cerr << "missing value after " << flag << '\n';
            return false;
        }
        std::string value{args[++i]};   // consume the next word as the value
        try {
            if (flag == "--port") {
                out.port = static_cast<std::uint16_t>(std::stoi(value));
            } else if (flag == "--peer") {
                out.peer = value;
            } else if (flag == "--drop") {
                out.dropPercent = std::stoi(value);
            } else {
                std::cerr << "unknown flag " << flag << '\n';
                return false;
            }
        } catch (const std::exception&) {   // stoi throws on "banana"
            std::cerr << "bad number for " << flag << ": " << value << '\n';
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// 6. <random>
// -----------------------------------------------------------------------------
// Don't use rand(): poor quality and global state. The modern recipe:
//   ENGINE        makes raw random bits      std::mt19937 (seed it once)
//   DISTRIBUTION  shapes them into a range   std::uniform_int_distribution
// Make the engine ONCE and reuse it. Seeding it on every call gives bad
// (often identical) numbers.
// For tests, seed with a FIXED number so the "random" drops are repeatable.
class DropSimulator {
public:
    DropSimulator(int percent, std::uint32_t seed) : percent_(percent), engine_(seed) {}

    bool shouldDrop()
    {
        std::uniform_int_distribution<int> roll(0, 99);   // 0..99 inclusive
        return roll(engine_) < percent_;
    }

private:
    int percent_;
    std::mt19937 engine_;
};

// -----------------------------------------------------------------------------
// 7. <chrono>
// -----------------------------------------------------------------------------
// Types with units built in, so you can't mix up seconds and milliseconds.
//   steady_clock   for measuring elapsed time / timeouts (never jumps back)
//   system_clock   for wall-clock "what time is it" (can jump: NTP, DST)
// using namespace std::chrono_literals lets you write 250ms, 2s.
// poll() wants a timeout in milliseconds as an int: duration.count().
void chronoDemo()
{
    using namespace std::chrono_literals;   // OK inside a function: small scope
    std::cout << "--- 7. chrono ---\n";
    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(30ms);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    std::cout << "slept about " << ms.count() << " ms\n";

    std::chrono::milliseconds pollTimeout = 2s;   // 2s converts to 2000ms automatically
    std::cout << "poll timeout as int: " << pollTimeout.count() << '\n';
}

int main(int argc, char* argv[])
{
    std::cout << "--- 5. argv ---\n";
    Options opts;
    if (!parseArgs(argc, argv, opts)) {
        std::cerr << "usage: " << argv[0] << " --port N --peer HOST:PORT --drop PERCENT\n";
        return 1;   // non-zero exit = failure (shells and CI check this)
    }
    std::cout << "port=" << opts.port << " peer=" << opts.peer << " drop=" << opts.dropPercent
              << "%\n\n";

    std::cout << "--- 6. random drop simulation (seed 42) ---\n";
    DropSimulator sim(opts.dropPercent, 42);
    int dropped = 0;
    for (std::uint32_t seq = 1; seq <= 20; ++seq) {
        if (sim.shouldDrop()) {
            std::cout << "#" << seq << " dropped\n";
            ++dropped;
        }
    }
    std::cout << dropped << " of 20 dropped\n\n";

    chronoDemo();
    return 0;
}

// =============================================================================
// TRY IT
//   1. Run with no flags, with --drop 50, and with --port banana.
//   2. Seed DropSimulator with std::random_device{}() instead of 42 and run it
//      several times. Then put 42 back and run several times. Why do tests
//      want the fixed seed?
//   3. Replace std::stoi in parseArgs with parsePort from net1_error_handling.cpp. What does
//      "--port 70000" do before and after? (Hint: static_cast wraps it.)
// =============================================================================
