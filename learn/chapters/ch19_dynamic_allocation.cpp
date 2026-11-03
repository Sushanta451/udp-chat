// =============================================================================
// CHAPTER 19 — Dynamic Allocation
// learncpp.com/cpp-tutorial  (lessons 19.1 – 19.5)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   'new' gets memory that lives until YOU say 'delete', and every way of
//   getting that wrong (forgetting, doing it twice, using memory after
//   freeing it) is a real security bug.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN server creates a Session object when a peer connects and destroys
//   it when the peer leaves. Sessions come and go while the program runs,
//   so they often live on the "heap" (memory you request at run time).
//   Use-after-free, double free, and leaks are among the most common causes
//   of real CVEs (published security holes) in network software written in
//   C and C++. This chapter shows each one so you can recognize it.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → WHY A VPN CARES → RULE → a function
//   that shows it. main() runs every lesson so you can see the output.
//   ALL the buggy code is commented out, so the program itself is clean.
//
// SEE THE BUGS YOURSELF (AddressSanitizer = "ASan", a bug detector)
//   1. Uncomment ONE buggy block (marked BUG DEMO).
//   2. Build:  clang++ -std=c++20 -g -fsanitize=address ch19_dynamic_allocation.cpp -o ch19
//   3. Run ./ch19 and read the report: it names the bug and the line.
//   (Leak reports: on Linux ASan shows them by default. On macOS ASan's
//    leak checker isn't supported; build WITHOUT -fsanitize and run
//    'leaks --atExit -- ./ch19' instead. Look for "N leaks for N total".)
//
// BUILD + RUN  (macOS/Linux only: uses getaddrinfo)
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch19_dynamic_allocation.cpp -o ch19 && ./ch19
// =============================================================================

#include <netdb.h>        // getaddrinfo, freeaddrinfo, addrinfo
#include <arpa/inet.h>    // inet_ntop
#include <netinet/in.h>   // sockaddr_in, INET_ADDRSTRLEN
#include <sys/socket.h>   // AF_INET, SOCK_DGRAM

#include <cstddef>
#include <cstdint>
#include <cstring>        // std::memcpy
#include <iostream>
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

struct Session
{
    PeerId peer{};
    Counter sendCounter{};
};

// =============================================================================
// 19.1  Dynamic memory allocation with new and delete
// =============================================================================
// IN PLAIN WORDS
//   Normal local variables live on the "stack" and vanish when the function
//   ends. 'new Session{}' puts a Session on the "heap" instead. It stays
//   alive until you 'delete' it, and new gives you a POINTER to it.
//
//   Four classic mistakes:
//     LEAK              never delete → memory is lost until the program exits.
//                       A server that leaks 1 KB per connection dies eventually.
//     DANGLING POINTER  a pointer to memory that was already deleted.
//     USE-AFTER-FREE    reading/writing through a dangling pointer. The memory
//                       may now belong to something else (maybe attacker data!).
//     DOUBLE DELETE     deleting the same memory twice. Corrupts the heap.
//
//   Tip: after delete, set the pointer to nullptr. 'delete nullptr' is safe
//   and does nothing.
//
// WHY A VPN CARES
//   Peer disconnects → session deleted → a timer still holds the old pointer
//   → it fires and writes into freed memory. That's a textbook CVE.
//
// RULE
//   Every new needs exactly ONE delete. Better: don't write new/delete at
//   all — let a class do it (19.3) or use std::unique_ptr (chapter 22).
void lesson_19_1()
{
    std::cout << "--- 19.1 new + delete ---\n";
    Session* s = new Session{7, 0};   // lives on the heap
    s->sendCounter += 1;              // -> means "member of the thing pointed to"
    std::cout << "session for peer " << s->peer << ", counter " << s->sendCounter << "\n";
    delete s;      // give the memory back
    s = nullptr;   // so nobody uses the dangling address

    // ---- BUG DEMO: use-after-free (ASan: "heap-use-after-free") ----
    // Session* a = new Session{1, 0};
    // delete a;
    // a->sendCounter = 99;          // writes into freed memory!

    // ---- BUG DEMO: double delete (ASan: "attempting double-free") ----
    // Session* b = new Session{2, 0};
    // delete b;
    // delete b;

    // ---- BUG DEMO: leak (ASan: "detected memory leaks") ----
    // Session* c = new Session{3, 0};
    // c = nullptr;                  // the only pointer is gone; memory lost
    std::cout << "\n";
}

// =============================================================================
// 19.2  Dynamically allocating arrays
// =============================================================================
// IN PLAIN WORDS
//   new Byte[n] makes an array of n bytes on the heap. n can be decided
//   while the program runs. Free it with delete[] (with the brackets!).
//     new    ↔ delete
//     new[]  ↔ delete[]
//   Mixing them up is undefined behavior (UB = anything can happen).
//   'new Byte[n]{}' zeroes the bytes. Without {} they hold leftover garbage.
//
// WHY A VPN CARES
//   Uninitialized buffers can leak old data (maybe old keys!) into a packet
//   you send. Always zero buffers you'll send.
//
// RULE
//   Match new[] with delete[]. Honestly: just use std::vector, which does
//   all of this for you (ch 16).
void lesson_19_2()
{
    std::cout << "--- 19.2 new[] + delete[] ---\n";
    std::size_t len = 1500;   // imagine this came from config at run time
    Byte* buf = new Byte[len]{};   // 1500 zero bytes
    buf[0] = 0x04;
    std::cout << "heap buffer of " << len << " bytes, buf[0]=" << static_cast<int>(buf[0])
              << ", buf[1]=" << static_cast<int>(buf[1]) << "\n";
    delete[] buf;   // brackets because we used new[]

    // ---- BUG DEMO: mismatch (ASan: "alloc-dealloc-mismatch", Linux) ----
    // Byte* oops = new Byte[16];
    // delete oops;                  // should be delete[]

    // ---- BUG DEMO: overflow (ASan: "heap-buffer-overflow") ----
    // Byte* small = new Byte[8]{};
    // small[8] = 1;                 // index 8 of an 8-byte array!
    // delete[] small;
    std::cout << "\n";
}

// =============================================================================
// 19.3  Destructors
// =============================================================================
// IN PLAIN WORDS
//   A "destructor" is a special member function, written ~ClassName(),
//   that runs AUTOMATICALLY when an object dies (goes out of scope or is
//   deleted). Put cleanup there: free memory, close sockets, wipe keys.
//
//   This idea has a name: RAII ("Resource Acquisition Is Initialization").
//   Plain meaning: the constructor GETS the resource, the destructor GIVES
//   IT BACK. You can't forget, even if the function returns early.
//
//   Catch: if you copy such an object, both copies would delete the same
//   memory (double delete!). So we forbid copying with '= delete'.
//   (Chapter 22's std::unique_ptr does all of this for you.)
//
// WHY A VPN CARES
//   Sessions hold keys and buffers. The destructor guarantees they're
//   cleaned up on every path out, including errors.
//
// RULE
//   Owning raw memory? Wrap it in a class with a destructor, and block
//   copying. Next step: std::unique_ptr (ch 22).
class PacketBuffer
{
public:
    explicit PacketBuffer(std::size_t size)
        : m_data{new Byte[size]{}}, m_size{size}
    {
        std::cout << "  PacketBuffer: got " << m_size << " bytes\n";
    }

    ~PacketBuffer()
    {
        // Real code wiping secrets would use sodium_memzero here first.
        delete[] m_data;
        std::cout << "  ~PacketBuffer: freed " << m_size << " bytes\n";
    }

    PacketBuffer(const PacketBuffer&) = delete;              // no copies
    PacketBuffer& operator=(const PacketBuffer&) = delete;   // no copy-assign

    std::size_t size() const { return m_size; }

private:
    Byte* m_data{};
    std::size_t m_size{};
};

void lesson_19_3()
{
    std::cout << "--- 19.3 destructors (RAII) ---\n";
    {
        PacketBuffer pb{1420};
        std::cout << "  using buffer of size " << pb.size() << "\n";
        // PacketBuffer copy = pb;   // COMPILE ERROR: copying is deleted
    }   // ← pb dies here, destructor runs automatically
    std::cout << "(left the block, memory already freed)\n\n";
}

// =============================================================================
// 19.4  Pointers to pointers and dynamic multidimensional arrays
// =============================================================================
// IN PLAIN WORDS
//   A pointer to a pointer (int**) holds the address of a pointer.
//
//   Where you'll meet it: C functions that need to GIVE YOU a pointer.
//   A function can't change your pointer unless you pass its address:
//
//     addrinfo* result = nullptr;
//     getaddrinfo("host", "port", &hints, &result);   // &result is addrinfo**
//     ... use result ...
//     freeaddrinfo(result);                           // the C way of "delete"
//
//   getaddrinfo turns a host name/IP + port into socket addresses. It
//   allocates the answer for you, so YOU must call freeaddrinfo, or leak.
//
//   It's also how a "2-D array" of rows of different size can be built:
//   Byte** rows = new Byte*[n]; then each rows[i] = new Byte[...].
//   That's a lot of new/delete. A flat std::vector is usually better.
//
// WHY A VPN CARES
//   Every VPN client resolves the server's address with getaddrinfo.
//
// RULE
//   If a C API allocates for you, find its matching free function and call
//   it exactly once.
void lesson_19_4()
{
    std::cout << "--- 19.4 pointer to pointer ---\n";
    addrinfo hints{};
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_DGRAM;     // UDP (WireGuard runs over UDP)
    hints.ai_flags = AI_NUMERICHOST;    // "it's already an IP, no DNS lookup"

    addrinfo* result = nullptr;
    int rc = getaddrinfo("127.0.0.1", "51820", &hints, &result);   // &result: addrinfo**
    if (rc != 0)
    {
        std::cout << "getaddrinfo failed: " << gai_strerror(rc) << "\n\n";
        return;
    }
    const auto* addr = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    std::cout << "resolved " << ip << " port " << ntohs(addr->sin_port) << "\n";
    freeaddrinfo(result);   // matching free; skipping this = leak
    result = nullptr;

    // Hand-made 2-D array with pointer-to-pointer: 3 rows of 4 bytes.
    constexpr std::size_t kRows = 3;
    constexpr std::size_t kCols = 4;
    Byte** grid = new Byte*[kRows];
    for (std::size_t r = 0; r < kRows; ++r)
        grid[r] = new Byte[kCols]{};
    grid[2][3] = 9;
    std::cout << "grid[2][3]=" << static_cast<int>(grid[2][3]) << "\n";
    for (std::size_t r = 0; r < kRows; ++r)
        delete[] grid[r];   // free each row first...
    delete[] grid;          // ...then the array of row pointers

    // The easy way: one flat vector, index with row * kCols + col.
    Bytes flat(kRows * kCols, 0);
    flat[2 * kCols + 3] = 9;
    std::cout << "flat[2*4+3]=" << static_cast<int>(flat[2 * kCols + 3]) << " (no delete needed)\n\n";
}

// =============================================================================
// 19.5  Void pointers
// =============================================================================
// IN PLAIN WORDS
//   void* is a pointer to "some bytes, type unknown". Any object pointer
//   converts to void* automatically. C APIs use it to accept ANY data:
//
//     void*   memcpy(void* dst, const void* src, size_t n);
//     ssize_t sendto(int sock, const void* buf, size_t len, ...);
//     ssize_t recvfrom(int sock, void* buf, size_t len, ...);
//
//   Because the type is unknown, the compiler doesn't know how big one
//   element is. So you CAN'T index a void* (p[1]) or do p + 1, or *p.
//   You must cast it back to a real type first (static_cast<Byte*>).
//   Notice the APIs always take a LENGTH too: void* has no size info at all.
//
// WHY A VPN CARES
//   Every send and receive goes through void*. Pass the wrong length and
//   nothing stops you from reading or writing past the buffer.
//
// RULE
//   Use void* only at the C API boundary. Always pass the true buffer size
//   (buf.size(), sizeof(arr)). Cast back to Byte* with static_cast.
void lesson_19_5()
{
    std::cout << "--- 19.5 void pointers ---\n";
    Counter counter = 0x0102030405060708;
    Byte wire[8]{};
    std::memcpy(wire, &counter, sizeof(counter));   // both convert to void* automatically

    void* raw = wire;
    // raw[0];                           // COMPILE ERROR: can't index void*
    // raw + 1;                          // ERROR (standard C++): unknown element size
    Byte* bytes = static_cast<Byte*>(raw);   // cast back to a real type
    std::cout << "first byte in memory: 0x" << std::hex << static_cast<int>(bytes[0]) << std::dec
              << (bytes[0] == 0x08 ? " (little-endian machine)" : " (big-endian machine)")
              << "\n\n";
}

int main()
{
    lesson_19_1();
    lesson_19_2();
    lesson_19_3();
    lesson_19_4();
    lesson_19_5();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Uncomment the use-after-free BUG DEMO in 19.1. Build with
//      -g -fsanitize=address and run. Find the three stack traces in the
//      report: where it was allocated, freed, and used.
//   2. Give PacketBuffer a 'Byte& at(std::size_t i)' that checks i < m_size
//      and returns m_data[i]. What should it do when i is too big?
//   3. In 19.4, comment out freeaddrinfo(result). Run a leak checker
//      (see the top of the file). What does it report?
//
// CHECK YOURSELF (answer out loud before moving on)
//   - What's the difference between a dangling pointer and use-after-free?
//   - Why does new[] need delete[] and not delete?
//   - Why does getaddrinfo take addrinfo** instead of addrinfo*?
// =============================================================================
