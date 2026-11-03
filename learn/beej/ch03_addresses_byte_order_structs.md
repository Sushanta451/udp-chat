# Beej Chapter 3, in depth: IP addresses, ports, byte order, structs

Read the matching section of the Beej PDF first, then the section here. Each section here has:

- **The idea**: what it is, in plain words.
- **Examples**: real numbers you can check by hand.
- **Scenarios**: where it bites you in a real program.
- **Check yourself**: answer out loud. The answers are hidden; open them only after you've tried.

The whole chapter answers one question: **how do you write down "where a packet should go"?**
The answer is always three things: **(address family, IP address, port)**. Everything below is about those three.

---

## 3.1 IP addresses (IPv4 and IPv6)

### The idea

An IP address identifies a **network interface**, not a computer. Your Mac has several interfaces at once, each with its own address:

| Interface | Example address | What it is |
|---|---|---|
| `lo0` | `127.0.0.1` | loopback: "this machine", never leaves it |
| `en0` | `192.168.1.50` | your Wi-Fi |
| `utun3` | `10.8.0.2` | a VPN tunnel (what you're going to build!) |

See yours with: `ifconfig | grep "inet "` (IPv4) and `ifconfig | grep inet6`.

### IPv4: 32 bits

Written as 4 numbers ("octets"), each 0 to 255, joined by dots. Each octet is **one byte**.

Worked example: your TV at `192.168.1.174`

| Octet | Decimal | Hex | Binary |
|---|---|---|---|
| 1 | 192 | `C0` | `11000000` |
| 2 | 168 | `A8` | `10101000` |
| 3 | 1 | `01` | `00000001` |
| 4 | 174 | `AE` | `10101110` |

So on the wire, that address is exactly four bytes: `C0 A8 01 AE`.

32 bits gives 2³² ≈ **4.3 billion** addresses. That's fewer than the number of devices on Earth, so we ran out. That shortage is why NAT exists (section 3.4.1) and why IPv6 was invented.

### IPv6: 128 bits

That's 16 bytes, written as **8 groups of 4 hex digits** (each group is 16 bits), joined by colons:

```
2001:0db8:0000:0000:0000:ff00:0042:8329
```

Two shortening rules:

1. **Drop leading zeros in a group:** `0db8` becomes `db8`, `0042` becomes `42`, `0000` becomes `0`.
2. **Replace ONE run of all-zero groups with `::`:**

```
2001:0db8:0000:0000:0000:ff00:0042:8329
2001:db8:0:0:0:ff00:42:8329       (rule 1)
2001:db8::ff00:42:8329            (rule 2)
```

Why is `::` allowed only once? Take `2001::1::2`. You know 5 zero groups are missing, but not how many go in each gap (1+4? 2+3?). The address is ambiguous.

2¹²⁸ ≈ 3.4 × 10³⁸ addresses. We won't run out.

### Special addresses you'll meet

| IPv4 | IPv6 | Meaning | Where you use it |
|---|---|---|---|
| `127.0.0.1` | `::1` | loopback | testing the chat with two terminals on one Mac |
| `0.0.0.0` | `::` | "any of my addresses" | `bind()` on a server: "accept packets on every interface" |
| `255.255.255.255` | none | broadcast | Beej 7.7. IPv6 has no broadcast. |
| `169.254.x.x` | `fe80::...` | link-local | auto-assigned when DHCP fails. If you see 169.254, your Wi-Fi didn't get a real address. |
| none | `::ffff:192.0.2.33` | an IPv4 address written as IPv6 | shows up if an IPv6 socket receives from an IPv4 sender |
| `192.0.2.x`, `198.51.100.x`, `203.0.113.x` | `2001:db8::/32` | reserved for documentation | examples in docs (like Beej's). Never routed. |

### Scenario

You run the chat. Your friend across town types your Wi-Fi address `192.168.1.50`. **Nothing arrives.** Why? That's a private address. It only means something inside your home network. See 3.4.1.

### Check yourself

1. How many bytes is an IPv4 address? An IPv6 address?
2. Write `10.0.0.1` as four hex bytes.
3. Expand `2001:db8::1` to its full 8-group form.
4. You want the chat to receive on Wi-Fi AND on loopback. What address do you bind to?

<details><summary>Answers</summary>

1. 4 and 16.
2. `0A 00 00 01`
3. `2001:0db8:0000:0000:0000:0000:0000:0001`
4. `0.0.0.0` (`INADDR_ANY`), or `::` for IPv6.
</details>

---

## 3.1.1 Subnets

### The idea

An IP address has two parts: **network** and **host**. It works like a phone number: the area code (network) gets you to the right region, and the rest (host) gets you to one phone.

The **netmask** says where the split is. It's a row of 1-bits (the network part) followed by 0-bits (the host part). **CIDR notation** just counts the 1-bits: `/24` means "the first 24 bits are the network".

| CIDR | Netmask | Host bits | Usable hosts (2^hostbits − 2) |
|---|---|---|---|
| /8 | 255.0.0.0 | 24 | 16,777,214 |
| /16 | 255.255.0.0 | 16 | 65,534 |
| /24 | 255.255.255.0 | 8 | 254 |
| /26 | 255.255.255.192 | 6 | 62 |
| /30 | 255.255.255.252 | 2 | 2 |

Why subtract 2? The all-zeros host is the **network address** (the name of the subnet), and the all-ones host is the **broadcast address**. Neither can be given to a device.

### Worked example: `192.168.1.174/26`

`/26` means 24 + 2 bits, so the split is inside the last octet. Do it in binary:

```
address  174 = 1010 1110
mask     192 = 1100 0000     (/26 → two 1-bits in the last octet)
AND          = 1000 0000 = 128   → network   = 192.168.1.128
set host bits to 1:  1011 1111 = 191   → broadcast = 192.168.1.191
usable hosts: 192.168.1.129 … 192.168.1.190   (62 of them)
```

### Why a programmer cares

Your OS uses subnets to decide **where to send every packet**:

- Is the destination in my subnet? Send it **directly** on the local network.
- No? Send it to the **default gateway** (your router).

You can watch this decision: `route get 8.8.8.8` versus `route get 192.168.1.1`. The full table is `netstat -rn`.

**VPN connection:** a VPN works by adding a route like "send `10.8.0.0/24` to `utun3`". The OS then hands every packet for that subnet to *your program* instead of to the Wi-Fi. That's Phase 4 of your roadmap, and it's subnets and nothing more.

### Check yourself

1. `10.0.5.77/16`: what are the network address, the broadcast address, and the usable host count?
2. `172.20.3.9/20`: same three. (Hint: the split is inside the 3rd octet. `/20` means the mask's 3rd octet is `11110000` = 240.)
3. Are `192.168.1.100` and `192.168.2.100` on the same `/24`? On the same `/16`?

<details><summary>Answers</summary>

1. Network `10.0.0.0`, broadcast `10.0.255.255`, 65,534 hosts.
2. 3rd octet: `3 AND 240 = 0`. Network `172.20.0.0`, broadcast `172.20.15.255` (the low 4 bits of the 3rd octet become 1111 = 15, and the last octet becomes 255). Host bits = 12, so 4,094 hosts.
3. Not on the same /24 (the third octets differ). On the same /16, yes.
</details>

---

## 3.1.2 Port numbers

### The idea

The IP address gets the packet to the **machine** (the hotel). The port gets it to the **program** (the room). A port is a **16-bit unsigned number**, 0 to 65535, so in C++ it's `std::uint16_t`.

When your program calls `bind()` with port 9000, it's telling the kernel: "any UDP packet for port 9000 on this machine, give it to me."

### The three ranges

| Range | Name | Who uses it |
|---|---|---|
| 0–1023 | well-known | DNS 53, HTTP 80, HTTPS 443. Linux needs root to bind these. Recent macOS doesn't enforce that, but don't rely on it. |
| 1024–49151 | registered | apps pick a fixed one: WireGuard 51820 is close by, Beej uses 3490 and 4950. **Your chat goes here.** |
| 49152–65535 | ephemeral | the OS hands these out when *you* don't pick: when you `sendto()` without `bind()`, or bind to port 0 |

Proof from the run earlier in this chat: net3 printed `listener bound to 127.0.0.1:56378` and `from 127.0.0.1:64315`. Both are ephemeral ports the OS picked.

### Facts that trip people up

- **TCP port 9000 and UDP port 9000 are different ports.** Each protocol has its own set of 65536.
- **One UDP socket per port** (normally). Start the chat twice with `--port 9000` and the second one gets `bind: Address already in use`. That's why your chat takes `--port`: Alice uses 9000, Bob uses 9001.
- **Port 0 isn't a real port.** It means "OS, choose one for me".
- A UDP packet is identified by **(source IP, source port, destination IP, destination port)**. When you reply to someone, you send back to *their* source IP and port. That's exactly what `recvfrom()` tells you (chapter 5).

### Commands

- `lsof -nP -iUDP`: which programs have which UDP ports open right now.
- `grep -w 53 /etc/services`: look up a well-known port.

### Check yourself

1. Why is the port a `uint16_t` and not an `int`?
2. Your chat binds port 9000. A friend's packet arrives for port 9001. What happens?
3. You call `sendto()` without ever calling `bind()`. What is your source port?

<details><summary>Answers</summary>

1. The header field is exactly 16 bits. An `int` could hold 70000, which isn't a valid port.
2. The kernel delivers it to whoever bound 9001. If nobody did, it's dropped silently (UDP has no error back to the sender by default).
3. An ephemeral port (49152–65535) the OS picked automatically.
</details>

---

## 3.2 Byte order (in depth, no code yet)

### The idea

Memory is a row of single bytes. A one-byte number fits in one slot. But a 2-byte or 4-byte number has to be **split across slots**, and there are two ways to order the pieces.

Take the 4-byte number `0x12345678`:

| Memory address → | +0 | +1 | +2 | +3 |
|---|---|---|---|---|
| **Big-endian** (big end first) | `12` | `34` | `56` | `78` |
| **Little-endian** (little end first) | `78` | `56` | `34` | `12` |

Big-endian reads the way you write numbers on paper. Little-endian stores them backwards.

**Who uses which:**

| Little-endian | Big-endian |
|---|---|
| Intel/AMD (x86), Apple Silicon (your Mac), almost all phones, Windows PCs | **The internet's protocols** ("network byte order"), old PowerPC/SPARC, Java's `DataOutputStream` |

So your machine is little-endian, and **the network is big-endian**. Every multi-byte number crosses that boundary.

### "I'm only going Mac to Mac, so I don't need it." Not true, and here's why

You're right about one thing: two little-endian machines that both skip conversion would agree with each other. Windows is little-endian too, so even Mac to Windows would "work". **But the conversion was never about the other computer.** It's about three other parties that require big-endian:

**Party 1: your own kernel.** The `sin_port` field in `sockaddr_in` *must* be in network byte order. Here is the most common socket bug in the world:

```
You write:            addr.sin_port = 4950;          // forgot htons
4950 in hex:          0x1356
Your Mac stores it:   56 13                          (little-endian)
Kernel reads bytes as big-endian:  0x5613 = 22035
```

Your program **binds to port 22035**. Your friend sends to 4950. Nothing arrives. **There's no error message**, because 22035 is a perfectly valid port. You'd lose an hour to this. With `htons(4950)`, the bytes become `13 56` and the kernel reads 4950.

Same story for addresses:

```
addr.sin_addr.s_addr = INADDR_LOOPBACK;   // forgot htonl; INADDR_LOOPBACK = 0x7F000001
Bytes in memory: 01 00 00 7F   → kernel sees 1.0.0.127, not 127.0.0.1
```

Why does code with `INADDR_ANY` (`0.0.0.0`) "work" without `htonl`? Because 0 is the same in both orders. **Bugs like that hide until you change the value.**

**Party 2: every protocol spec, including yours.** Your assignment says `Packet{300, "hi"}` becomes `00 00 01 2c 68 69`. That's big-endian. Someday you'll open Wireshark to debug the chat. Wireshark, and every other tool, reads protocol numbers as big-endian.

**Party 3: real packets inside your VPN.** In Phase 4 your program reads raw IP packets from the TUN device. Byte 2–3 of an IPv4 header is the **total length**, in big-endian. Say a packet is 84 bytes long, `00 54`:

```
Read correctly (big-endian):  0x0054 = 84 bytes
Read as little-endian:        0x5400 = 21,504 bytes
```

Now your code thinks the packet is 21 KB and reads far past the end of an 84-byte buffer. That's a **buffer over-read**, which is the class of bug behind real security holes. Byte order is a security issue, not just a portability one.

### When you need to convert, and when you don't

| Convert (`htons`/`htonl` going out, `ntohs`/`ntohl` coming in) | Don't convert |
|---|---|
| `sin_port` / `sin6_port`, always | single bytes (`uint8_t`): one byte has no order |
| `s_addr` when **you** set it from a number (`INADDR_LOOPBACK`) | text (`std::string`): characters are already one byte each, in order |
| every 2/4/8-byte integer in **your** packet format (the seq number) | byte arrays: IPv6 addresses, keys, nonces, hashes |
| reading fields from real protocol headers (IP, UDP) | anything from `inet_pton()` or `getaddrinfo()`: already network order |
| | numbers that never leave your program (loop counters, sizes) |

### The rules a pro follows

1. **Convert at the boundary.** Keep normal (host-order) numbers everywhere inside your program. Convert at the exact moment the number is written into a packet or `sockaddr`, and at the exact moment it's read out. Don't keep network-order numbers lying around in ordinary variables, because you'll forget which is which.
2. **Convert exactly once.** `htons(htons(x))` swaps twice, so you're back to host order on your Mac: the bug comes back. It looks fine on a big-endian machine, where both calls do nothing.
3. **For packet buffers, shifting is even better than `htonl`.** Your assignment's `encode` does `value >> 24`, `>> 16`, `>> 8`, `>> 0` and writes each byte. Shifts work on *numbers*, not memory, so they produce big-endian bytes on **every** machine, with no memory tricks. Use `htons`/`htonl` for the `sockaddr` fields, and shifts for your own packets.
4. **Never send floats raw.** They have byte order *and* format problems. If you ever need them, see Beej 7.5. For the chat and the VPN, you won't.

(Modern C++ also has `std::endian` in C++20, which tells you your machine's order at compile time, and `std::byteswap` in C++23. Good to know about. The rules above are what matter.)

### Check yourself

1. Port 3490 is `0x0DA2`. What two bytes go on the wire? What port would the kernel see if you forgot `htons` on your Mac?
2. Do you `htonl` the bytes of a 32-byte encryption key before sending? Why or why not?
3. Your friend's chat is written in Java, which writes ints big-endian. Your chat uses `htonl` for the seq number. Do they agree?
4. `inet_pton` gave you `sin_addr`. Should you `htonl(addr.sin_addr.s_addr)` before `bind()`?
5. Why might a missing `htons` go unnoticed in testing?

<details><summary>Answers</summary>

1. `0D A2`. Without `htons`, memory holds `A2 0D`, so the kernel sees `0xA20D` = 41485.
2. No. A key is a byte array: 32 separate bytes with no "big end".
3. Yes. Both are big-endian on the wire. That's the whole point of a standard order.
4. No! `inet_pton` already wrote network order. Converting again would scramble it (rule 2).
5. If both sides make the same mistake, they agree with each other. It's also invisible with values like 0 or `INADDR_ANY`. It only shows up against a correct program, a tool, or a different value.
</details>

---

## 3.3 The `sockaddr` structs

### The idea

The socket API was designed in C in the early 1980s, to work with *many* kinds of addresses (IPv4, IPv6, Unix files, and others) through **one** set of functions. C has no classes or inheritance, so the designers used this trick:

- One **generic** type, `struct sockaddr`, that every function takes a pointer to. It's like a base class.
- Several **specific** types (`sockaddr_in`, `sockaddr_in6`) that hold the real data. They're like derived classes.
- **Every** one of them starts with the **family** field. The kernel reads the family first, then knows which shape the rest has.

So you fill in a specific struct, then pass its address cast to the generic pointer type. You **also** pass the size, because a `sockaddr*` alone doesn't say how big the real struct is.

### The structs, drawn out (macOS sizes)

```
sockaddr_in  (IPv4) — 16 bytes
 ┌────────┬──────────┬──────────┬──────────────┬─────────────────┐
 │sin_len │sin_family│ sin_port │   sin_addr   │    sin_zero     │
 │ 1 byte │  1 byte  │ 2 bytes  │   4 bytes    │    8 bytes      │
 │  (Mac  │ AF_INET  │ NETWORK  │ NETWORK      │ padding, must   │
 │  only) │          │  ORDER   │  ORDER       │ be zero         │
 └────────┴──────────┴──────────┴──────────────┴─────────────────┘

sockaddr_in6 (IPv6) — 28 bytes
 len(1) family(1) port(2) flowinfo(4) sin6_addr(16 raw bytes) scope_id(4)

sockaddr_storage — 128 bytes: big enough for any of the above.
```

On **Linux**, `sin_len` doesn't exist and `sin_family` is 2 bytes. **That's why you never write your own version of these structs.** The layout must match what *this* OS's kernel expects, byte for byte. The system headers (`<netinet/in.h>`) always have it right for the machine you compile on.

Your earlier attempt in `src/main.cpp`:

```cpp
struct sockaddr_in4 {
   Addrfamily sin4_family;   // you used uint32_t: 4 bytes. The real one is 1 byte on Mac
   Port sin4_port;           // uint16_t: correct size!
   Ipv4addr sin_             // you used uint16_t: but an IPv4 address is 32 bits
};
```

Even with the sizes fixed, the kernel would read it wrong, because the `sin_len` byte is missing. Delete this struct and use the real one.

### Which struct, when

| Situation | Use |
|---|---|
| You build an IPv4 address yourself | `sockaddr_in` |
| You build an IPv6 address yourself | `sockaddr_in6` |
| **Receiving** (`recvfrom`, `accept`): you don't know v4 or v6 yet | `sockaddr_storage`, then check `ss_family` |
| You got it from `getaddrinfo()` | just use `p->ai_addr` and `p->ai_addrlen` as given (chapter 5) |

### The code (this is worth seeing)

Filling one by hand:

```cpp
sockaddr_in addr{};                  // {} zeroes EVERYTHING, including sin_zero and sin_len
addr.sin_family = AF_INET;
addr.sin_port   = htons(9000);       // 3.2: network order!
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);   // 3.4: already network order

bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
//       ^ cast to the generic type    ^ and say how big the real one is
```

Reading one you received:

```cpp
sockaddr_storage from{};
socklen_t len = sizeof from;         // IN: how much room you have. OUT: how much was used.
recvfrom(fd, buf, size, 0, reinterpret_cast<sockaddr*>(&from), &len);

if (from.ss_family == AF_INET) {
    auto* v4 = reinterpret_cast<sockaddr_in*>(&from);
    std::uint16_t port = ntohs(v4->sin_port);   // back to a normal number
}
```

`reinterpret_cast` means "treat these bytes as a different type". In most C++ it's a red flag. In socket code it's normal and expected, because that is how the C API was designed.

### Common mistakes

| Mistake | What happens |
|---|---|
| No `{}` (leaving the struct uninitialized) | garbage in `sin_zero`/`sin_len`, so `bind` fails with `EINVAL` some of the time |
| `sizeof(sockaddr)` (16) for an IPv6 address (28) | `EINVAL`: the kernel thinks the address got cut off |
| Forgot `htons` on the port | the silent wrong-port bug from 3.2 |
| `len` not reset before each `recvfrom` | after a v4 packet `len` is 16, so the next v6 sender's address gets cut off |
| Storing a `sockaddr*` that points into a local variable that went out of scope | a dangling pointer. Store the whole `sockaddr_storage` by value instead. |

### Check yourself

1. Why do `bind` and `sendto` need a length argument as well as the pointer?
2. Why receive into `sockaddr_storage` instead of `sockaddr_in`?
3. What breaks if you copy Linux's `sockaddr_in` definition into code you compile on your Mac?

<details><summary>Answers</summary>

1. A `sockaddr*` doesn't tell you the real struct's size (16 for v4, 28 for v6). The kernel needs to know how many bytes to read.
2. The sender might be IPv6 (28 bytes). A 16-byte `sockaddr_in` is too small, so the address would be cut off.
3. The field positions shift (no `sin_len`, 2-byte family). The kernel reads your family byte as the length, and the rest as nonsense.
</details>

---

## 3.4 `inet_pton` and `inet_ntop`

### The idea

Humans write `"192.168.1.174"`. The kernel wants the 4 bytes `C0 A8 01 AE`. These two functions translate:

- **`inet_pton`**, "presentation to network": text → bytes. Use it when you're parsing a user's `--peer 192.168.1.174`.
- **`inet_ntop`**, "network to presentation": bytes → text. Use it when you're printing who sent you a message.

Both take the family (`AF_INET` or `AF_INET6`) first, so they handle both IP versions.

### The details that matter

**`inet_pton` has THREE possible results.** Check for exactly `1`:

| Returns | Meaning | Example input |
|---|---|---|
| `1` | success | `"127.0.0.1"` |
| `0` | not a valid address in that family | `"999.1.1.1"`, `"localhost"`, `"::1"` with `AF_INET` |
| `-1` | the family itself is invalid (`errno` is set) | a wrong `AF_` constant |

Writing `if (!inet_pton(...))` misses the `-1` case, because -1 counts as true. Write `if (inet_pton(...) != 1)`.

**It doesn't do DNS.** `inet_pton(AF_INET, "localhost", ...)` returns 0. Names need `getaddrinfo()` (chapter 5).

**`inet_ntop` needs a buffer that's big enough.** Use the provided size constants:

| Constant | Value | Why |
|---|---|---|
| `INET_ADDRSTRLEN` | 16 | `"255.255.255.255"` is 15 characters, plus the terminating `'\0'` |
| `INET6_ADDRSTRLEN` | 46 | the longest IPv6 form, plus `'\0'` |

In C++: `std::array<char, INET6_ADDRSTRLEN> buf{};`. Using the v6 size works for both families.

**Pass the address field, not the whole struct:**

```cpp
inet_ntop(AF_INET, &v4->sin_addr, buf.data(), buf.size());   // ✓ the 4 address bytes
inet_ntop(AF_INET, v4,            buf.data(), buf.size());   // ✗ starts at sin_len: prints garbage
```

### The old functions (you'll see them in old code; don't use them)

| Old | Problem |
|---|---|
| `inet_addr("...")` | returns -1 on error, but -1 as 32 bits is `255.255.255.255`, a valid address. You can't tell the two apart. |
| `inet_ntoa(addr)` | returns a pointer into **one shared internal buffer**. `printf("%s → %s", inet_ntoa(a), inet_ntoa(b))` prints the same address twice. |
| both | IPv4 only |

### Check yourself

1. A user types `--peer localhost`. What does `inet_pton(AF_INET, ...)` return, and what should you use instead?
2. Why is `INET_ADDRSTRLEN` 16 and not 15?
3. Why does `if (!inet_pton(...))` count as a bug?

<details><summary>Answers</summary>

1. `0` (it's not numeric). Use `getaddrinfo`, which does the name lookup.
2. 15 visible characters plus the `'\0'` that ends a C string.
3. It treats `-1` (an error) as success, because `!(-1)` is false.
</details>

---

## 3.4.1 Private networks and NAT

### The idea

Since IPv4 ran out, homes get **one** public address from the ISP. Every device inside uses a **private** address that's only meaningful inside that network:

| Private range | CIDR | Typical use |
|---|---|---|
| `10.0.0.0` – `10.255.255.255` | `10.0.0.0/8` | companies, VPNs (WireGuard often uses `10.x`) |
| `172.16.0.0` – `172.31.255.255` | `172.16.0.0/12` | Docker uses `172.17.x.x` |
| `192.168.0.0` – `192.168.255.255` | `192.168.0.0/16` | home routers (your TV: `192.168.1.174`) |
| `100.64.0.0` – `100.127.255.255` | `100.64.0.0/10` | ISP-level NAT ("CGNAT"). Tailscale borrows it. |

The **router** translates between private and public addresses. That's **NAT** (Network Address Translation).

### How NAT handles one UDP packet, step by step

You (`192.168.1.50:9000`) send a chat message to a friend whose public address is `203.0.113.7:9000`:

```
1. Your Mac sends:     src 192.168.1.50:9000  → dst 203.0.113.7:9000
2. Router rewrites:    src 198.51.100.4:61234  → dst 203.0.113.7:9000
                       and REMEMBERS:  61234 ↔ 192.168.1.50:9000
3. Friend replies to:  198.51.100.4:61234
4. Router looks up 61234, rewrites dst back to 192.168.1.50:9000 → your chat gets it
```

The friend's `recvfrom()` reports the sender as `198.51.100.4:61234`. That's your **public** address, not the private one your Mac knows about.

### The consequences for your chat (and VPN)

- **Same Wi-Fi:** works fine with `192.168.x.x` addresses. No NAT is involved.
- **Across the internet, if your friend sends first:** the router has **no table entry** for an unexpected incoming packet, so it **drops it**. NAT works like a one-way door.
- **The fixes:** port forwarding on the router; or one side has a public IP (a cheap cloud server); or "hole punching" (both sides send at the same time to create table entries).
- **Table entries expire.** For UDP, often after about 30 seconds of silence. That's why WireGuard has a `PersistentKeepalive = 25` setting: it sends a tiny packet every 25 seconds so the router doesn't forget the mapping. Your VPN will need the same thing.

**IPv6** mostly doesn't need NAT (there are enough addresses for everyone). Its private range is `fd00::/8`.

### Check yourself

1. Your friend's `recvfrom` says your message came from `198.51.100.4:61234`, but your program bound to port 9000. Explain the difference.
2. The chat works on the same Wi-Fi, but not between two houses. Name two possible fixes.
3. Your VPN tunnel works, then dies after 1 minute of idle time and recovers when you send something. What's the likely cause and fix?

<details><summary>Answers</summary>

1. Your router's NAT rewrote the source address and port to its public IP and a port it chose.
2. Port forwarding on one router, or run one side on a server with a public IP. (Hole punching also works.)
3. The NAT mapping expired, so incoming packets were dropped. Send a keepalive every ~25 seconds.
</details>

---

## Chapter 4 in one minute (IPv4 → IPv6)

Beej's chapter 4 is a checklist, and it all comes down to: **use `getaddrinfo()` and write code that doesn't care which version it's using.** If you hard-code `sockaddr_in` and `AF_INET` everywhere, supporting IPv6 later means rewriting. If you use `getaddrinfo` + `sockaddr_storage` + `p->ai_addrlen`, IPv6 support comes almost for free. Chapter 5 shows how.

---

**Next:** chapter 5, the system calls, in the same format. Practice for this chapter: [ch03_practice.cpp](ch03_practice.cpp).
