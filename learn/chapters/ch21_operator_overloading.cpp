// =============================================================================
// CHAPTER 21 — Operator Overloading
// learncpp.com/cpp-tutorial  (lessons 21.1 – 21.14)
// =============================================================================
//
// THE BIG IDEA (in one sentence)
//   You can teach C++ what +, ==, <<, [] and friends mean for YOUR types,
//   so your types read like built-in ones — but every operator you add is
//   a promise, so only add the ones whose meaning is obvious.
//
// HOW THIS CONNECTS TO A VPN
//   A VPN hands each client a tunnel IP: 10.8.0.2, 10.8.0.3, 10.8.0.4 ...
//   Under the hood an IPv4 address is just one 32-bit number:
//
//       10 . 8 . 0 . 2   =   0x0A 08 00 02   =   168296450
//
//   This whole chapter builds ONE class, IPv4Address, and gives it the
//   operators a VPN's "IP pool" needs: print it, parse it, compare it,
//   get the next one, check if it's in our subnet.
//   Lessons 21.12–21.13 switch to a small KeyBuffer class to show the most
//   dangerous part of the chapter: copying objects that own memory.
//
// HOW TO READ THIS FILE
//   Each lesson has: IN PLAIN WORDS → EXAMPLE → WHY A VPN CARES → RULE.
//   The IPv4Address class is written in pieces, lesson by lesson, in the
//   class body below. Then main() runs every example so you can see the output.
//
// BUILD + RUN
//   clang++ -std=c++20 -Wall -Wextra -Wconversion ch21_operator_overloading.cpp -o ch21 && ./ch21
// =============================================================================

#include <algorithm>   // std::max, std::count_if
#include <cassert>
#include <charconv>    // std::from_chars
#include <compare>     // operator<=>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>     // std::istringstream (pretend input)
#include <string>
#include <string_view>
#include <vector>

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using Counter = std::uint64_t;
using PeerId = std::uint32_t;

// =============================================================================
// 21.1  Introduction to operator overloading
// =============================================================================
// IN PLAIN WORDS
//   An operator is just a function with a funny name. 'a + b' is really a
//   call to a function named 'operator+' with a and b as arguments.
//   "Overloading" = writing your own operator+ for your own type.
//
//   Limits: you can't invent new operators (no '**'), can't change how
//   many operands one takes, and at least one operand must be YOUR type
//   (you can't change what int + int means).
//
// WHY A VPN CARES
//   'ip + 1' reads better than 'IPv4Address{ip.value() + 1}'. Readable code
//   gets reviewed; reviewed code has fewer bugs.
//
// RULE
//   Overload an operator only if its meaning is OBVIOUS. 'ip + 1' = next
//   address: obvious. 'ip * key': nobody knows what that means. Don't.
//
// ----- The main example. Read the comments; each part belongs to a lesson. ---
class IPv4Address {
public:
    constexpr IPv4Address() = default;                         // 0.0.0.0
    constexpr explicit IPv4Address(std::uint32_t value) : m_value{value} {}
    constexpr IPv4Address(Byte a, Byte b, Byte c, Byte d)
        : m_value{(std::uint32_t{a} << 24) | (std::uint32_t{b} << 16) |
                  (std::uint32_t{c} << 8) | std::uint32_t{d}}
    {
    }

    constexpr std::uint32_t value() const { return m_value; }  // a normal getter

    // 21.2: friend function. It can read m_value directly.
    friend IPv4Address operator+(IPv4Address ip, std::uint32_t offset);

    // 21.4: I/O operators are always NON-members (left side is a stream).
    friend std::ostream& operator<<(std::ostream& out, IPv4Address ip);
    friend std::istream& operator>>(std::istream& in, IPv4Address& ip);

    // 21.5: member function. Changes *this, so it lives inside the class.
    IPv4Address& operator+=(std::uint32_t offset)
    {
        m_value += offset;
        return *this;   // return *this so 'a += 1' can be used in a bigger expression
    }

    // 21.6: unary !  → "is this the empty address 0.0.0.0?"
    constexpr bool operator!() const { return m_value == 0; }

    // 21.7: C++20 does all six comparisons (== != < <= > >=) from these.
    // '= default' compares the members in order. We only have one: m_value.
    constexpr auto operator<=>(const IPv4Address&) const = default;

    // 21.8: increment. Prefix (++ip) and postfix (ip++).
    IPv4Address& operator++()        // prefix: add 1, return the NEW value
    {
        ++m_value;
        return *this;
    }
    IPv4Address operator++(int)      // postfix: the dummy 'int' just marks it as postfix
    {
        IPv4Address old{*this};
        ++(*this);                   // reuse prefix
        return old;                  // return the OLD value
    }

    // 21.9: subscript. ip[0] is the first octet (the 10 in 10.8.0.2).
    Byte operator[](std::size_t i) const
    {
        assert(i < 4 && "octet index must be 0..3");
        return static_cast<Byte>(m_value >> (24 - 8 * i));
    }

    // 21.11: typecasts. 'explicit' = only when you ASK for it.
    constexpr explicit operator std::uint32_t() const { return m_value; }
    constexpr explicit operator bool() const { return m_value != 0; }

private:
    std::uint32_t m_value{};
};

void lesson_21_1()
{
    std::cout << "--- 21.1 intro ---\n";
    IPv4Address ip{10, 8, 0, 2};
    std::cout << "ip + 1 = " << ip + 1 << '\n';                         // reads nicely
    std::cout << "same call, spelled out: ";
    operator<<(std::cout, operator+(ip, 1)) << "\n\n";                 // what C++ really does
}

// =============================================================================
// 21.2  Overloading the arithmetic operators using friend functions
// =============================================================================
// IN PLAIN WORDS
//   A "friend" function is declared inside the class with the word 'friend'.
//   It is NOT a member, but it's allowed to touch private members.
//   Arithmetic operators like + usually return a NEW object and leave the
//   inputs alone, so they fit well as friends (or normal functions, 21.3).
//
// WHY A VPN CARES
//   pool start + 5 = the 5th client's address.
//
// RULE
//   operator+ returns a new value by value. It must not change its inputs.
//   Note: 255.255.255.255 + 1 wraps to 0.0.0.0 (unsigned math wraps).
//   A real pool must check it doesn't run past the end of its subnet.
IPv4Address operator+(IPv4Address ip, std::uint32_t offset)
{
    return IPv4Address{ip.m_value + offset};   // friend: may use m_value
}

void lesson_21_2()
{
    std::cout << "--- 21.2 friend operator+ ---\n";
    IPv4Address poolStart{10, 8, 0, 2};
    std::cout << "poolStart + 5 = " << poolStart + 5 << '\n';             // 10.8.0.7
    std::cout << "10.8.0.255 + 1 = " << IPv4Address{10, 8, 0, 255} + 1   // carries over
              << '\n';                                                     // 10.8.1.0
    std::cout << "255.255.255.255 + 1 = " << IPv4Address{0xFFFFFFFF} + 1 << " (wrapped!)\n\n";
}

// =============================================================================
// 21.3  Overloading operators using normal functions
// =============================================================================
// IN PLAIN WORDS
//   If the class already has public getters, the operator doesn't need to
//   be a friend. A normal function works and touches less private stuff.
//
// WHY A VPN CARES
//   'last - first' = how many addresses are between them (pool size).
//
// RULE
//   Prefer a normal (non-friend) function when public functions are enough.
std::uint32_t operator-(IPv4Address a, IPv4Address b)
{
    return a.value() - b.value();   // only public API used: no friend needed
}

void lesson_21_3()
{
    std::cout << "--- 21.3 normal-function operator- ---\n";
    IPv4Address first{10, 8, 0, 2};
    IPv4Address last{10, 8, 0, 254};
    std::cout << "addresses in pool = " << (last - first) + 1 << "\n\n";   // 253
}

// =============================================================================
// 21.4  Overloading the I/O operators
// =============================================================================
// IN PLAIN WORDS
//   operator<< prints your type to any stream (cout, a file, a log).
//   operator>> reads your type from any stream.
//   Both take the stream by reference and RETURN it, so you can chain:
//   'std::cout << a << b'.
//
// WHY A VPN CARES
//   Addresses come from config files and from users. Parsing is the front
//   door for bad input. "10.8.0.256", "1.2.3", "10.8.0.2junk" must all be
//   rejected, not quietly turned into some other address.
//
// RULE
//   On bad input, set the stream's failbit and leave the object unchanged.
std::ostream& operator<<(std::ostream& out, IPv4Address ip)
{
    // Byte is a char type, so cast to int or it prints as a character.
    return out << int{ip[0]} << '.' << int{ip[1]} << '.' << int{ip[2]} << '.' << int{ip[3]};
}

std::istream& operator>>(std::istream& in, IPv4Address& ip)
{
    std::string text;
    if (!(in >> text)) return in;   // nothing to read

    std::string_view rest{text};
    std::uint32_t result = 0;
    for (int part = 0; part < 4; ++part) {
        unsigned octet{};
        auto [ptr, ec] = std::from_chars(rest.data(), rest.data() + rest.size(), octet);
        auto used = static_cast<std::size_t>(ptr - rest.data());
        if (ec != std::errc{} || used == 0 || used > 3 || octet > 255) {
            in.setstate(std::ios::failbit);
            return in;
        }
        result = (result << 8) | octet;
        rest.remove_prefix(used);
        if (part < 3) {                                  // expect a dot between octets
            if (rest.empty() || rest.front() != '.') {
                in.setstate(std::ios::failbit);
                return in;
            }
            rest.remove_prefix(1);
        }
    }
    if (!rest.empty()) {                                 // leftover junk
        in.setstate(std::ios::failbit);
        return in;
    }
    ip.m_value = result;                                 // only change on success
    return in;
}

void lesson_21_4()
{
    std::cout << "--- 21.4 I/O operators ---\n";
    for (std::string input : {"10.8.0.2", "10.8.0.256", "1.2.3", "10.8.0.2junk", "abc"}) {
        std::istringstream in{input};   // a pretend config file
        IPv4Address ip{};
        if (in >> ip)
            std::cout << "parsed  \"" << input << "\" → " << ip << '\n';
        else
            std::cout << "REJECT  \"" << input << "\"\n";
    }
    std::cout << '\n';
}

// =============================================================================
// 21.5  Overloading operators using member functions
// =============================================================================
// IN PLAIN WORDS
//   A member operator lives inside the class. The LEFT operand is *this,
//   so it takes one fewer parameter: 'a += 5' calls a.operator+=(5).
//
//   Which to pick?
//     - =, [], (), -> MUST be members (the language says so).
//     - <<, >> with a stream on the left MUST be non-members.
//     - Operators that change the left side (+=, ++) → member.
//     - Operators that make a new value (+, -) → non-member.
//
// RULE
//   Follow the list above. When unsure: modifies *this → member.
void lesson_21_5()
{
    std::cout << "--- 21.5 member operator+= ---\n";
    IPv4Address next{10, 8, 0, 2};
    next += 10;
    std::cout << "10.8.0.2 += 10 → " << next << "\n\n";   // 10.8.0.12
}

// =============================================================================
// 21.6  Overloading unary operators +, -, and !
// =============================================================================
// IN PLAIN WORDS
//   "Unary" = takes ONE operand: -x, +x, !x. As a member, it takes no
//   parameters at all (the operand is *this).
//
// WHY A VPN CARES
//   0.0.0.0 means "no address assigned yet". '!ip' reads as "ip is empty".
//   What about -ip? A "negative IP address" means nothing, so we DON'T
//   write operator-() for it. (A class like a time offset would want one.)
//
// RULE
//   Overload only the unary operators that make sense for your type.
void lesson_21_6()
{
    std::cout << "--- 21.6 unary ! ---\n";
    IPv4Address unassigned{};
    IPv4Address assigned{10, 8, 0, 2};
    std::cout << std::boolalpha << "!0.0.0.0 = " << !unassigned << ", !10.8.0.2 = " << !assigned
              << "\n\n";   // true, false
}

// =============================================================================
// 21.7  Overloading the comparison operators
// =============================================================================
// IN PLAIN WORDS
//   Before C++20 you wrote 6 operators by hand: == != < <= > >=.
//   C++20 has the "spaceship" operator <=>. It answers "less, equal, or
//   greater?" in one go. With '= default', the compiler writes it for you
//   AND gives you == too. Then all six comparisons just work.
//
// WHY A VPN CARES
//   Sorting addresses, storing them in std::set/std::map (which need <),
//   and checking "is this address already handed out?" (which needs ==).
//
// RULE
//   In C++20: 'auto operator<=>(const T&) const = default;' and you're done,
//   as long as comparing member-by-member is what you want.
void lesson_21_7()
{
    std::cout << "--- 21.7 comparisons (<=>) ---\n";
    IPv4Address a{10, 8, 0, 2};
    IPv4Address b{10, 8, 0, 3};
    std::cout << std::boolalpha << "a == b " << (a == b) << ", a != b " << (a != b)
              << ", a < b " << (a < b) << ", a >= b " << (a >= b) << '\n';

    std::vector<IPv4Address> leased{{10, 8, 0, 9}, {10, 8, 0, 2}, {10, 8, 0, 5}};
    std::sort(leased.begin(), leased.end());   // uses <
    std::cout << "sorted leases:";
    for (auto ip : leased) std::cout << ' ' << ip;
    std::cout << "\n\n";
}

// =============================================================================
// 21.8  Overloading the increment and decrement operators
// =============================================================================
// IN PLAIN WORDS
//   ++ip (prefix): add one, give back the NEW value.
//   ip++ (postfix): add one, give back the OLD value (so it makes a copy).
//   C++ tells them apart by a fake 'int' parameter on the postfix one.
//
// WHY A VPN CARES
//   An IP pool hands out addresses one after another: lease = next++.
//
// RULE
//   Write prefix first, then build postfix from it. Prefer ++x in your own
//   code: it doesn't need the copy.
class IpPool {
public:
    IpPool(IPv4Address first, IPv4Address last) : m_next{first}, m_last{last} {}
    bool exhausted() const { return m_next > m_last; }
    IPv4Address lease() { return m_next++; }   // postfix: hand out current, move on
private:
    IPv4Address m_next;
    IPv4Address m_last;
};

void lesson_21_8()
{
    std::cout << "--- 21.8 ++ / IP pool ---\n";
    IPv4Address ip{10, 8, 0, 2};
    std::cout << "ip++ gives " << ip++ << ", now ip = " << ip << '\n';   // .2 then .3
    std::cout << "++ip gives " << ++ip << '\n';                           // .4

    IpPool pool{IPv4Address{10, 8, 0, 2}, IPv4Address{10, 8, 0, 4}};   // tiny pool: 3 addrs
    for (PeerId peer = 1; peer <= 4; ++peer) {
        if (pool.exhausted())
            std::cout << "peer" << peer << ": pool exhausted, rejected\n";
        else
            std::cout << "peer" << peer << " gets " << pool.lease() << '\n';
    }
    std::cout << '\n';
}

// =============================================================================
// 21.9  Overloading the subscript operator
// =============================================================================
// IN PLAIN WORDS
//   operator[] lets you write obj[i]. Must be a member. Usually you also
//   want a const version (ours is const-only, since octets are read-only).
//
// WHY A VPN CARES
//   Reading octet 0 tells you if an address is private (10.x.x.x).
//   An out-of-range index reads memory you don't own.
//
// RULE
//   Check the index. assert() catches YOUR bugs while testing, but asserts
//   are removed in release builds (-DNDEBUG). If the index comes from the
//   network, check it with a real 'if' and reject.
void lesson_21_9()
{
    std::cout << "--- 21.9 subscript [] ---\n";
    IPv4Address ip{10, 8, 0, 2};
    std::cout << "octets: " << int{ip[0]} << ' ' << int{ip[1]} << ' ' << int{ip[2]} << ' '
              << int{ip[3]} << '\n';
    std::cout << std::boolalpha << "private 10/8 address? " << (ip[0] == 10) << '\n';
    // ip[4];   // assert fires: "octet index must be 0..3" → program stops
    std::cout << '\n';
}

// =============================================================================
// 21.10  Overloading the parenthesis operator
// =============================================================================
// IN PLAIN WORDS
//   operator() makes an object callable like a function: subnet(ip).
//   Such an object is called a "functor". Unlike a plain function, it can
//   carry data (here: which subnet).
//
//   A subnet like 10.8.0.0/24 means "the first 24 bits must match".
//   /24 → mask 255.255.255.0 → 10.8.0.0 through 10.8.0.255.
//
// WHY A VPN CARES
//   Before forwarding a packet into the tunnel, check: is its source
//   address inside the subnet this peer is allowed to use? If not, drop
//   it. (WireGuard calls this "AllowedIPs".) Otherwise a client can spoof
//   other clients' addresses.
//
// RULE
//   Use a functor when a "function" needs to remember settings. It plugs
//   right into std:: algorithms like std::count_if.
class Subnet {
public:
    Subnet(IPv4Address network, int prefixLength)
        : m_mask{prefixLength <= 0 ? 0u : 0xFFFFFFFFu << (32 - std::min(prefixLength, 32))},
          m_network{network.value() & m_mask}
    {
    }
    bool operator()(IPv4Address ip) const { return (ip.value() & m_mask) == m_network; }

private:
    std::uint32_t m_mask;      // declared first: m_network's initializer uses it
    std::uint32_t m_network;
};

void lesson_21_10()
{
    std::cout << "--- 21.10 functor () ---\n";
    Subnet tunnel{IPv4Address{10, 8, 0, 0}, 24};
    std::cout << std::boolalpha << "10.8.0.77 in 10.8.0.0/24? " << tunnel(IPv4Address{10, 8, 0, 77})
              << "\n10.8.1.5  in 10.8.0.0/24? " << tunnel(IPv4Address{10, 8, 1, 5}) << '\n';

    std::vector<IPv4Address> sources{{10, 8, 0, 2}, {192, 168, 1, 1}, {10, 8, 0, 9}};
    auto allowed = std::count_if(sources.begin(), sources.end(), tunnel);   // functor as callback
    std::cout << "allowed packet sources: " << allowed << " of " << sources.size() << "\n\n";
}

// =============================================================================
// 21.11  Overloading typecasts
// =============================================================================
// IN PLAIN WORDS
//   'operator std::uint32_t()' lets you convert your object to a uint32_t.
//   Mark it 'explicit' so it only happens when you write static_cast.
//   Without explicit, C++ converts silently (chapter 10 all over again).
//
//   'explicit operator bool' is special: it still works inside if(), while,
//   !, &&, || (C++ calls this "contextual conversion") but NOT in 'int x = ip'.
//
// WHY A VPN CARES
//   You need the raw number to write the address into a packet header.
//   You do NOT want 'ip + 5' to quietly become 'uint32_t + 5'.
//
// RULE
//   Conversion operators: always explicit.
void lesson_21_11()
{
    std::cout << "--- 21.11 typecasts ---\n";
    IPv4Address ip{10, 8, 0, 2};
    auto raw = static_cast<std::uint32_t>(ip);   // asked for it: OK
    std::cout << "10.8.0.2 as uint32 = " << raw << " (0x" << std::hex << raw << std::dec << ")\n";
    // std::uint32_t oops = ip;                  // ERROR: explicit blocks silent conversion

    if (ip) std::cout << "if (ip): address is assigned\n";   // explicit bool works in if
    // int n = ip;                                         // ERROR: not in arithmetic
    std::cout << '\n';
}

// =============================================================================
// 21.12  Overloading the assignment operator
// 21.13  Shallow vs. deep copying
// =============================================================================
// IN PLAIN WORDS
//   If you don't write them, C++ writes a copy constructor and copy
//   assignment (operator=) for you. They copy each member one by one.
//   For an int: perfect. For a POINTER: it copies the ADDRESS, not the data.
//
//   SHALLOW copy: both objects now point at the SAME memory.
//   DEEP copy:    allocate new memory, copy the bytes into it.
//
//       shallow:  a.m_data ──┐            deep:  a.m_data ──► [k e y]
//                 b.m_data ──┴─► [k e y]         b.m_data ──► [k e y]
//
//   With shallow copy, when a dies it delete[]s the memory, then b dies and
//   delete[]s the SAME memory again: a "double free". That's UB (undefined
//   behavior): a crash if you're lucky, a security hole if you're not.
//
//   operator= must also handle "self-assignment" (k = k). If you delete
//   your own buffer first, you then copy from freed memory.
//
// WHY A VPN CARES
//   Key material lives in buffers. A double free in key-handling code is
//   the kind of bug attackers love.
//
// RULE
//   If a class has a destructor that frees memory, it needs a copy
//   constructor and copy assignment too ("rule of three").
//   BETTER: don't hold a raw pointer at all. Use std::vector or std::array;
//   they copy correctly by themselves ("rule of zero").
//   For SECRETS, even better: forbid copies completely (move-only, ch22).

// Wipe memory in a way the optimizer won't delete. A plain memset right
// before delete[] can be optimized away ("nobody reads it after"). Real
// code uses sodium_memzero() or explicit_bzero(). This volatile loop is
// fine for learning.
void secureWipe(Byte* p, std::size_t n)
{
    volatile Byte* v = p;
    for (std::size_t i = 0; i < n; ++i) v[i] = 0;
}

// ---- The WRONG way (shallow). Uses the compiler's member-by-member copy. ----
class ShallowKeyBuffer {
public:
    explicit ShallowKeyBuffer(std::size_t size) : m_data{new Byte[size]{}} {}
    ~ShallowKeyBuffer() { delete[] m_data; }
    // No copy constructor written → compiler copies the POINTER.
    Byte* data() { return m_data; }
    // Only for this demo: forget the pointer without freeing it.
    void forgetPointer() { m_data = nullptr; }

private:
    Byte* m_data;   // (a real class would also store the size)
};

// ---- The RIGHT way (deep copy + self-assignment check). ----
class KeyBuffer {
public:
    explicit KeyBuffer(std::size_t size) : m_data{new Byte[size]{}}, m_size{size} {}

    KeyBuffer(const KeyBuffer& other)                     // copy constructor: deep
        : m_data{new Byte[other.m_size]}, m_size{other.m_size}
    {
        std::copy(other.m_data, other.m_data + m_size, m_data);
    }

    KeyBuffer& operator=(const KeyBuffer& other)          // copy assignment: deep
    {
        if (this == &other) return *this;                 // self-assignment check
        Byte* fresh = new Byte[other.m_size];             // allocate first (if this throws,
        std::copy(other.m_data, other.m_data + other.m_size, fresh);   // we're unchanged)
        secureWipe(m_data, m_size);                       // wipe old key
        delete[] m_data;                                  // free old key
        m_data = fresh;
        m_size = other.m_size;
        return *this;
    }

    ~KeyBuffer()
    {
        secureWipe(m_data, m_size);
        delete[] m_data;
    }

    Byte& operator[](std::size_t i) { return m_data[i]; }
    const Byte* data() const { return m_data; }

private:
    Byte* m_data;
    std::size_t m_size;
};

void lesson_21_12_13()
{
    std::cout << "--- 21.12 / 21.13 assignment, shallow vs deep ---\n";
    {
        ShallowKeyBuffer a{32};
        a.data()[0] = 0x42;
        ShallowKeyBuffer b = a;   // shallow: copies the pointer
        b.data()[0] = 0x99;       // changes a's key too!
        std::cout << std::boolalpha << "shallow: same memory? " << (a.data() == b.data())
                  << ", a[0] is now 0x" << std::hex << int{a.data()[0]} << std::dec << '\n';
        // WITHOUT the next line, a and b would both delete[] the same memory at
        // the '}' → DOUBLE FREE. We cheat here so this program stays clean.
        b.forgetPointer();
    }
    {
        KeyBuffer a{32};
        a[0] = 0x42;
        KeyBuffer b = a;          // copy constructor (deep)
        b[0] = 0x99;              // only b changes
        std::cout << "deep:    same memory? " << (a.data() == b.data()) << ", a[0] still 0x"
                  << std::hex << int{a[0]} << std::dec << '\n';

        KeyBuffer c{16};
        c = a;                    // copy assignment (deep)
        KeyBuffer& sameC = c;     // (writing 'c = c' directly makes clang warn)
        c = sameC;                // self-assignment: safe thanks to the check
        std::cout << "after c = a; c = c;  c[0] = 0x" << std::hex << int{c[0]} << std::dec
                  << "\n\n";
    }   // a, b, c each wipe + free their OWN memory. No double free.
}

// =============================================================================
// 21.14  Overloading operators and function templates
// =============================================================================
// IN PLAIN WORDS
//   A function template works for ANY type... as long as that type supports
//   every operation the template uses. isInRange below uses '<'. Since
//   IPv4Address has operator< (from <=>), the template just works for it.
//   If it didn't, you'd get a compile error inside the template.
//
// WHY A VPN CARES
//   One generic helper checks "port in 1..65535", "counter in window",
//   AND "address in pool range". Less code, fewer bugs.
//
// RULE
//   Give your types the normal operators, and generic code (yours and the
//   standard library's) works with them for free.
template <typename T>
bool isInRange(const T& x, const T& lo, const T& hi)
{
    return !(x < lo) && !(hi < x);   // only needs operator<
}

void lesson_21_14()
{
    std::cout << "--- 21.14 operators + templates ---\n";
    std::cout << std::boolalpha << "port 51820 in [1, 65535]? " << isInRange(51820, 1, 65535)
              << '\n';
    IPv4Address lo{10, 8, 0, 2};
    IPv4Address hi{10, 8, 0, 254};
    std::cout << "10.8.0.100 in pool? " << isInRange(IPv4Address{10, 8, 0, 100}, lo, hi)
              << "\n10.8.0.255 in pool? " << isInRange(IPv4Address{10, 8, 0, 255}, lo, hi) << '\n';
    std::cout << "std::max(lo, hi) = " << std::max(lo, hi) << "\n\n";   // std::max needs < too
}

int main()
{
    lesson_21_1();
    lesson_21_2();
    lesson_21_3();
    lesson_21_4();
    lesson_21_5();
    lesson_21_6();
    lesson_21_7();
    lesson_21_8();
    lesson_21_9();
    lesson_21_10();
    lesson_21_11();
    lesson_21_12_13();
    lesson_21_14();
    return 0;
}

// =============================================================================
// PRACTICE
//   1. Add 'IPv4Address& operator--()' and 'operator--(int)' (21.8). Test
//      that 10.8.1.0 decremented is 10.8.0.255.
//   2. Make IpPool (21.8) refuse to hand out the network address (.0) and
//      the broadcast address (.255). Use your Subnet functor (21.10).
//   3. Replace KeyBuffer's raw 'Byte* m_data' with 'Bytes m_data' (a vector).
//      Delete the copy constructor, copy assignment and destructor. Does it
//      still copy correctly? (Rule of zero.) What do you lose? (Hint: the wipe.)
//
// CHECK YOURSELF (answer out loud before moving on)
//   - Why do operator<< and operator>> have to be non-member functions?
//   - What goes wrong if a class owns a raw pointer and you let the
//     compiler write its copy constructor?
//   - Why is 'explicit operator bool' still usable in 'if (ip)'?
// =============================================================================
