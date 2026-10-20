# CIDR Notation

## What it is
CIDR (Classless Inter-Domain Routing) notation represents an IP address and its associated routing prefix using the format `a.b.c.d/n`, where `n` is the number of leading bits in the network portion of the address. The prefix length `n` ranges from 0 to 32 for IPv4 and 0 to 128 for IPv6, defining a contiguous block of `2^(32-n)` (IPv4) or `2^(128-n)` (IPv6) addresses. CIDR replaced classful addressing (RFC 791) as standardized in RFC 4632 (IPv4) and RFC 4291 (IPv6).

## Why it matters
CIDR enables route aggregation (supernetting) and variable-length subnet masking (VLSM), reducing global routing table size and allowing efficient address allocation — critical for internet scalability and private network design.

## How it actually works

### Binary representation and prefix matching
An IPv4 address is a 32-bit unsigned integer. The prefix length `n` defines a bitmask where the first `n` bits are `1` and the remaining `32-n` bits are `0`. The network address is computed as `address & mask`. The broadcast address is `address | ~mask`. Usable host addresses range from `network + 1` to `broadcast - 1` (except `/31` and `/32` special cases per RFC 3021).

**Mask calculation**: `mask = 0xFFFFFFFF << (32 - n)` for `n > 0`, else `0x00000000`.

**Host count formula**: `2^(32-n) - 2` for `n < 31`; `/31` yields 2 usable addresses (point-to-point links, RFC 3021); `/32` yields 1 address (single host).

### Supernetting and aggregation
Supernetting combines multiple contiguous prefixes into a single shorter prefix. Two prefixes `P1/n` and `P2/n` can be aggregated into `P/n-1` iff:
1. They share the same first `n-1` bits
2. They are adjacent (the `n`th bit differs: one has `0`, the other `1`)
3. Both have identical prefix length `n`

Aggregation reduces routing table entries. The longest prefix match (LPM) rule governs forwarding: for destination `D`, select the route with maximum `n` where `(D & mask_n) == network_n`.

### VLSM (Variable Length Subnet Masking)
VLSM allows recursive subdivision of a prefix into sub-prefixes of varying lengths. Given parent prefix `P/n`, a child prefix `C/m` (where `m > n`) is valid iff `(C & mask_n) == P`. Subnet allocation typically uses a binary tree (trie) structure for efficient overlap detection and space management.

**Subnet calculation algorithm** (for allocating `k` subnets of size `/m` from `/n`):
- Available subnets: `2^(m-n)`
- Each subnet increment: `2^(32-m)` addresses
- Subnet `i` network address: `P + i * 2^(32-m)`

### IPv6 specifics
IPv6 uses 128-bit addresses. CIDR notation: `xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx/n`. Standard allocation: `/48` per site (65,536 `/64` subnets), `/64` per subnet (interface identifier space). No broadcast address; uses multicast (`ff02::1` all-nodes). RFC 4291 defines addressing architecture.

### Key RFCs
- **RFC 4632**: CIDR (IPv4) — obsoletes RFC 1518, 1519
- **RFC 4291**: IPv6 Addressing Architecture
- **RFC 3021**: Using 31-bit prefixes on point-to-point links
- **RFC 1918**: Private address space (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16)
- **RFC 6598**: Shared address space (100.64.0.0/10) for CGN

## Architecture / flow

```mermaid
flowchart TD
    A[Input: IP/prefix] --> B{Parse prefix length n}
    B --> C[Compute mask: 0xFFFFFFFF << (32-n)]
    C --> D[Network = IP & mask]
    D --> E[Broadcast = IP | ~mask]
    E --> F{Prefix length?}
    F -->|n=32| G[Single host: network=IP, no broadcast]
    F -->|n=31| H[Point-to-point: 2 usable hosts, no broadcast]
    F -->|n<31| I[Usable: network+1 to broadcast-1]
    G --> J[Output: network, broadcast, host range, count]
    H --> J
    I --> J
```

## Key terms
**Longest Prefix Match (LPM)** — Routing lookup algorithm selecting the route with the greatest prefix length matching the destination address.

**Supernet** — A prefix formed by aggregating multiple contiguous child prefixes of equal length into a shorter prefix.

**VLSM** — Variable Length Subnet Masking: recursive subdivision of a prefix into sub-prefixes of differing lengths.

**Prefix length** — The number of leading bits (`n` in `/n`) defining the network portion of an address.

**Host bits** — The trailing `32-n` (IPv4) or `128-n` (IPv6) bits available for host addressing within a prefix.

**CIDR block** — A contiguous range of IP addresses represented by a single `address/prefix` notation.

## Example
```bash
# ipcalc output for 10.0.0.0/22
Address:   10.0.0.0           00001010.00000000.00000000.00000000
Netmask:   255.255.252.0 = 22 11111111.11111111.11111100.00000000
Wildcard:  0.0.3.255          00000000.00000000.00000011.11111111
Network:   10.0.0.0/22        00001010.00000000.00000000.00000000
HostMin:   10.0.0.1           00001010.00000000.00000000.00000001
HostMax:   10.0.3.254         00001010.00000000.00000011.11111110
Broadcast: 10.0.3.255         00001010.00000000.00000011.11111111
Hosts/Net: 1022                (2^(32-22) - 2)
```
Demonstrates network/broadcast calculation, usable host range, and host count for a `/22` supernet aggregating four `/24` networks.

## Common mistakes
- **Assuming `/24` boundary alignment**: Treating `10.0.1.0/24` and `10.0.2.0/24` as aggregatable into `10.0.0.0/23` — they are not contiguous at `/23` (requires `10.0.0.0/24` + `10.0.1.0/24`).
- **Ignoring `/31` and `/32` special cases**: Applying `-2` host formula to `/31` (yields 0, but RFC 3021 allows 2 usable) or `/32` (yields -1, but represents single host).
- **Overlapping VLSM allocations**: Assigning `10.0.0.0/24` and `10.0.0.128/25` simultaneously — the `/25` falls inside the `/24`, causing routing ambiguity.
- **Confusing wildcard mask with netmask**: Using `0.0.0.255` (wildcard for `/24`) where `255.255.255.0` (netmask) is required, e.g., in ACLs or OSPF network statements.

## Lesser-known internals
- **LPM implementation uses binary tries or Patricia trees**: Hardware routers use TCAM (Ternary Content-Addressable Memory) for O(1) LPM; software uses radix trees (Linux `fib_trie`) with O(prefix length) worst-case.
- **`/31` prefixes have no broadcast address**: Both addresses are usable hosts; the "network" and "broadcast" concepts don't apply — RFC 3021 explicitly defines this for point-to-point links to conserve address space.
- **IPv6 `/127` prefixes are deprecated for point-to-point**: RFC 6164 recommends `/127` only for inter-router links with specific security properties; `/64` remains standard for all other links to preserve SLAAC and ND operation.

## Topics to explore further
- **BGP route aggregation and AS_PATH manipulation** — How CIDR blocks are advertised/aggregated at inter-domain level with community attributes
- **IPv6 ULA (Unique Local Addresses, fc00::/7)** — Private addressing analog with global uniqueness probability via RFC 4193
- **Prefix delegation (DHCPv6-PD, RFC 3633)** — Automated `/48` or `/56` delegation to downstream routers for hierarchical VLSM

## Learn next
- **BGP (Border Gateway Protocol)** — CIDR blocks are the atomic unit of inter-domain routing; BGP path selection operates on prefix/length pairs
- **IPv6 addressing architecture** — 128-bit CIDR with fixed `/64` subnet boundary changes allocation strategy fundamentally
- **Routing table lookup algorithms (LC-trie, DIR-24-8, TCAM)** — LPM implementation details that determine forwarding performance at scale