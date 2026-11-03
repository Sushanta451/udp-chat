// =============================================================================
// tests.cpp — tests for the assignment. You should NOT need to edit this file.
// =============================================================================
//
// A tiny homemade test framework, so there's nothing to install. Real projects
// use GoogleTest or Catch2, which work the same way with nicer output.
//
//   ./build/assignment_tests          run everything
//   ./build/assignment_tests packet   run only tests whose name contains "packet"
// =============================================================================
#include "packet.h"
#include "sequence_tracker.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ---- the "framework" ---------------------------------------------------------
namespace {

int gFailures = 0;

// A macro (not a function) so it can print the EXPRESSION text and line number.
// #expr turns the code into a string. __LINE__ is the current line. That's
// about the only good reason to write a macro in modern C++.
#define CHECK(expr)                                                                            \
    do {                                                                                       \
        if (!(expr)) {                                                                         \
            std::cout << "    FAILED line " << __LINE__ << ": " << #expr << '\n';              \
            ++gFailures;                                                                       \
        }                                                                                      \
    } while (false)

struct TestCase {
    std::string name;
    std::function<void()> body;
};

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;   // created on first use
    return tests;
}

void test(std::string name, std::function<void()> body)
{
    registry().push_back({std::move(name), std::move(body)});
}

using Bytes = std::vector<std::uint8_t>;

Bytes bytesOf(std::string_view s) { return {s.begin(), s.end()}; }

} // namespace

// ---- Part A: packet ------------------------------------------------------------
void registerPacketTests()
{
    using chat::decode;
    using chat::encode;
    using chat::Packet;

    test("packet: encode puts seq first, big-endian", [] {
        CHECK(encode(Packet{300, "hi"}) == (Bytes{0x00, 0x00, 0x01, 0x2c, 'h', 'i'}));
        CHECK(encode(Packet{1, ""}) == (Bytes{0, 0, 0, 1}));
        CHECK(encode(Packet{0x01020304, "x"}) == (Bytes{1, 2, 3, 4, 'x'}));
    });

    test("packet: encode handles the largest seq", [] {
        CHECK(encode(Packet{0xFFFFFFFF, ""}) == (Bytes{0xff, 0xff, 0xff, 0xff}));
    });

    test("packet: encode rejects text that is too long", [] {
        bool threw = false;
        try {
            encode(Packet{1, std::string(chat::kMaxText + 1, 'a')});
        } catch (const std::length_error&) {
            threw = true;
        }
        CHECK(threw);
        // Exactly kMaxText is allowed.
        CHECK(encode(Packet{1, std::string(chat::kMaxText, 'a')}).size() == chat::kMaxPacket);
    });

    test("packet: decode reads what encode wrote", [] {
        for (const Packet& p : {Packet{1, "hello"}, Packet{300, "did #4 make it?"},
                                Packet{0xFFFFFFFF, ""}, Packet{42, "caf\xc3\xa9 \xf0\x9f\x91\x8b"}}) {
            auto back = decode(encode(p));
            CHECK(back.has_value());
            CHECK(back && *back == p);
        }
    });

    test("packet: decode handles header bytes >= 0x80", [] {
        // Catches the classic bug of doing the shifts with (signed) char.
        auto p = decode(Bytes{0x80, 0x90, 0xa0, 0xb0});
        CHECK(p && p->seq == 0x8090a0b0u);
    });

    test("packet: decode rejects short packets", [] {
        CHECK(!decode(Bytes{}).has_value());
        CHECK(!decode(Bytes{0, 0, 1}).has_value());
        auto headerOnly = decode(Bytes{0, 0, 0, 9});
        CHECK(headerOnly && headerOnly->seq == 9 && headerOnly->text.empty());
    });

    test("packet: decode rejects oversized packets", [] {
        Bytes huge(chat::kMaxPacket + 1, 'a');
        CHECK(!decode(huge).has_value());
    });

    test("packet: decode works on a slice of a bigger buffer", [] {
        // Like recvfrom: a 1500-byte buffer where only the first n bytes are real.
        Bytes buf(1500, 0xEE);
        Bytes real = bytesOf(std::string("\0\0\0\x07ok", 6));
        std::copy(real.begin(), real.end(), buf.begin());
        auto p = decode(std::span{buf}.first(real.size()));
        CHECK(p && p->seq == 7 && p->text == "ok");
    });
}

// ---- Part B: tracker -----------------------------------------------------------
void registerTrackerTests()
{
    using chat::Arrival;
    using chat::SequenceTracker;
    using Seqs = std::vector<std::uint32_t>;

    test("tracker: toString names every kind", [] {
        CHECK(std::string(chat::toString(Arrival::Invalid)) == "invalid");
        CHECK(std::string(chat::toString(Arrival::Duplicate)) == "duplicate");
        CHECK(std::string(chat::toString(Arrival::InOrder)) == "in-order");
        CHECK(std::string(chat::toString(Arrival::Gap)) == "gap");
        CHECK(std::string(chat::toString(Arrival::Late)) == "late");
    });

    test("tracker: starts empty", [] {
        const SequenceTracker t;
        CHECK(t.highest() == 0);
        CHECK(t.receivedCount() == 0);
        CHECK(t.missing().empty());
    });

    test("tracker: 1, 2, 3 are in order", [] {
        SequenceTracker t;
        CHECK(t.onReceive(1).kind == Arrival::InOrder);
        CHECK(t.onReceive(2).kind == Arrival::InOrder);
        CHECK(t.onReceive(3).kind == Arrival::InOrder);
        CHECK(t.highest() == 3);
        CHECK(t.receivedCount() == 3);
        CHECK(t.missing().empty());
    });

    test("tracker: seq 0 is invalid and changes nothing", [] {
        SequenceTracker t;
        CHECK(t.onReceive(0).kind == Arrival::Invalid);
        CHECK(t.receivedCount() == 0);
        CHECK(t.onReceive(1).kind == Arrival::InOrder);
    });

    test("tracker: first packet lost", [] {
        SequenceTracker t;
        auto e = t.onReceive(3);
        CHECK(e.kind == Arrival::Gap);
        CHECK(e.missing == (Seqs{1, 2}));
        CHECK(t.missing() == (Seqs{1, 2}));
        CHECK(t.highest() == 3);
    });

    test("tracker: the worked example from the header", [] {
        SequenceTracker t;
        CHECK(t.onReceive(1).kind == Arrival::InOrder);
        CHECK(t.onReceive(2).kind == Arrival::InOrder);

        auto gap = t.onReceive(5);
        CHECK(gap.kind == Arrival::Gap);
        CHECK(gap.missing == (Seqs{3, 4}));
        CHECK(t.missing() == (Seqs{3, 4}));

        CHECK(t.onReceive(4).kind == Arrival::Late);
        CHECK(t.missing() == (Seqs{3}));

        CHECK(t.onReceive(4).kind == Arrival::Duplicate);
        CHECK(t.onReceive(3).kind == Arrival::Late);
        CHECK(t.missing().empty());
        CHECK(t.highest() == 5);
        CHECK(t.receivedCount() == 5);
    });

    test("tracker: duplicates of in-order packets", [] {
        SequenceTracker t;
        t.onReceive(1);
        t.onReceive(2);
        CHECK(t.onReceive(2).kind == Arrival::Duplicate);
        CHECK(t.onReceive(1).kind == Arrival::Duplicate);
        CHECK(t.receivedCount() == 2);
    });

    test("tracker: only NEWLY missing numbers are reported", [] {
        SequenceTracker t;
        t.onReceive(1);
        CHECK(t.onReceive(4).missing == (Seqs{2, 3}));
        auto second = t.onReceive(7);
        CHECK(second.kind == Arrival::Gap);
        CHECK(second.missing == (Seqs{5, 6}));          // not 2 and 3 again
        CHECK(t.missing() == (Seqs{2, 3, 5, 6}));       // but those are still missing
    });

    test("tracker: missing() stays sorted when holes fill in any order", [] {
        SequenceTracker t;
        t.onReceive(10);   // 1..9 missing
        t.onReceive(5);
        t.onReceive(2);
        t.onReceive(9);
        CHECK(t.missing() == (Seqs{1, 3, 4, 6, 7, 8}));
        CHECK(t.receivedCount() == 4);
    });

    test("tracker: in-order after a gap is in order", [] {
        SequenceTracker t;
        t.onReceive(1);
        t.onReceive(3);
        CHECK(t.onReceive(4).kind == Arrival::InOrder);
        CHECK(t.missing() == (Seqs{2}));
    });

    test("tracker: works through a const reference", [] {
        SequenceTracker t;
        t.onReceive(2);
        const SequenceTracker& view = t;   // only const methods are callable here
        CHECK(view.highest() == 2);
        CHECK(view.missing() == (Seqs{1}));
    });
}

int main(int argc, char* argv[])
{
    registerPacketTests();
    registerTrackerTests();

    std::string_view filter = argc > 1 ? argv[1] : "";
    int ran = 0;
    int failedTests = 0;
    for (const auto& t : registry()) {
        if (t.name.find(filter) == std::string::npos) {
            continue;
        }
        ++ran;
        int before = gFailures;
        try {
            t.body();
        } catch (const std::exception& e) {
            std::cout << "    THREW: " << e.what() << '\n';
            ++gFailures;
        }
        bool ok = gFailures == before;
        failedTests += ok ? 0 : 1;
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << t.name << '\n';
    }
    std::cout << '\n' << (ran - failedTests) << '/' << ran << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
}
