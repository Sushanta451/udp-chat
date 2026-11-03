// =============================================================================
// sequence_tracker.h — classify arriving sequence numbers  (ASSIGNMENT PART B)
// =============================================================================
//
// This is the brain of the "lossy" chat. Every time a packet arrives, the
// receiver calls tracker.onReceive(seq) and prints a warning based on the
// answer. The rules (sequence numbers start at 1):
//
//   Invalid    seq == 0. Senders never use 0.
//   Duplicate  this seq was already received before.
//   InOrder    seq == highest() + 1. The one we expected next.
//   Gap        seq  > highest() + 1. Jumped ahead. Every number in between
//              that has NOT been received is now "missing" and is listed in
//              Event::missing (sorted, smallest first).
//   Late       seq  < highest(), and it's one of the missing numbers. It
//              wasn't lost after all; it's no longer missing.
//
// Worked example, receiving 1, 2, 5, 4, 4, 3:
//   onReceive(1) → InOrder                     highest 1   missing {}
//   onReceive(2) → InOrder                     highest 2   missing {}
//   onReceive(5) → Gap, missing = {3, 4}       highest 5   missing {3, 4}
//   onReceive(4) → Late                        highest 5   missing {3}
//   onReceive(4) → Duplicate                   highest 5   missing {3}
//   onReceive(3) → Late                        highest 5   missing {}
//
// You may add PRIVATE members and private helper functions. Don't change the
// public interface: the tests depend on it.
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace chat {

enum class Arrival {
    Invalid,
    Duplicate,
    InOrder,
    Gap,
    Late,
};

const char* toString(Arrival a);   // "invalid", "duplicate", "in-order", "gap", "late"

struct Event {
    Arrival kind = Arrival::Invalid;
    std::vector<std::uint32_t> missing;   // only filled for Gap: the newly-missing numbers
};

class SequenceTracker {
public:
    Event onReceive(std::uint32_t seq);

    // Highest seq received so far (0 if nothing yet).
    [[nodiscard]] std::uint32_t highest() const;

    // How many DISTINCT valid packets have arrived (duplicates and 0 don't count).
    [[nodiscard]] std::size_t receivedCount() const;

    // Every number below highest() that hasn't arrived yet, sorted.
    [[nodiscard]] std::vector<std::uint32_t> missing() const;

private:
    // TODO(B0): decide what you need to remember. Think about it before
    // reaching for "a set of everything I've seen":
    //   - What's the smallest amount of state that can answer
    //     "have I seen seq X?" given highest() and the missing numbers?
    //   - Which container keeps the missing numbers sorted for you? (std::set: a sorted container, see cppreference)
    // Remember: default member initializers (ch13.9).
};

} // namespace chat
