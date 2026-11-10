# Anycast

## What it is
Anycast is a network addressing and routing methodology where a single IP address is assigned to multiple geographically distributed nodes, with routing protocols (typically BGP) directing each client request to the topologically nearest instance based on routing table metrics. Unlike unicast (one-to-one) or multicast (one-to-many), anycast implements one-to-nearest semantics at the network layer without requiring client-side awareness or application-layer logic.

## Why it matters
Anycast enables stateless horizontal scaling of stateful services (DNS, CDN edges, NTP, root servers) by letting the routing infrastructure handle load distribution and failover automatically — eliminating the need for client-side load balancers, health-check APIs, or DNS-based geo-routing with their inherent TTL caching delays.

## How it actually works
Anycast operates by announcing the same IP prefix (typically /24 for IPv4, /48 for IPv6) from multiple autonomous systems (ASes) or multiple locations within a single AS via BGP. Each announcing router advertises the prefix with standard BGP attributes (AS_PATH, LOCAL_PREF, MED, COMMUNITY). The BGP best-path selection algorithm at each upstream router determines which announcement wins for a given ingress point, effectively partitioning the internet into "catchment areas" around each anycast node.

**BGP best-path selection order (Cisco implementation, widely mirrored):**
1. Highest WEIGHT (Cisco proprietary, local to router)
2. Highest LOCAL_PREF (default 100)
3. Locally originated routes (network/aggregate/redistribute)
4. Shortest AS_PATH length
5. Lowest ORIGIN type (IGP < EGP < INCOMPLETE)
6. Lowest MED (Multi-Exit Discriminator)
7. eBGP over iBGP paths
8. Lowest IGP metric to NEXT_HOP
9. Oldest route (stability)
10. Lowest router ID of advertising neighbor
11. Lowest neighbor IP address

**Key RFCs:**
- RFC 1546 (1993) — Host Anycasting Service (obsolete, historical)
- RFC 2526 (1999) — Reserved IPv6 Subnet Anycast Addresses
- RFC 4291 (2006) — IPv6 Addressing Architecture (defines subnet-router anycast)
- RFC 4786 (2006) — Operation of Anycast Services (best current practice)
- RFC 7816 (2016) — DNS Query Name Minimisation (relevant for anycast DNS privacy)

**IPv6 anycast address format (RFC 4291 §2.6.1):**
- Subnet-router anycast: `prefix::/n` where the interface identifier is all zeros
- Reserved anycast addresses: `::/128` (unspecified), `::1/128` (loopback), `ff02::1` (all-nodes multicast, not anycast)
- No dedicated anycast address range — anycast uses unicast address space

**DNS anycast specifics:**
- Root servers: 13 logical identifiers (a.root-servers.net through m.root-servers.net), each backed by 100+ anycast instances globally
- Authoritative operators (Verisign, ICANN, etc.) announce same NS IP prefixes from multiple POPs
- EDNS Client Subnet (ECS, RFC 7871) allows recursive resolvers to pass client subnet to anycast authoritative servers for better routing decisions
- TTL considerations: Low TTLs (30-60s) on A/AAAA records for anycast nodes enable faster failover but increase query volume

**Health checking & failover:**
- No standardized protocol — operators implement custom health daemons that withdraw BGP announcements (via `withdraw` UPDATE or setting COMMUNITY `no-export`/`no-advertise`) when local service fails
- Typical withdrawal propagation: 30-120 seconds globally (BGP convergence)
- Some operators use BGP FlowSpec (RFC 5575) for granular traffic steering during partial degradation

**CDN anycast usage:**
- Edge POPs announce shared anycast prefixes (e.g., Cloudflare's 1.1.1.1, 1.0.0.1 for DNS; CDN hostnames resolve to anycast IPs)
- TCP connection state is local to the node that accepted the SYN — anycast works because HTTP/1.1 keep-alive and HTTP/2/3 connections remain pinned to that node for their lifetime
- QUIC (RFC 9000) connection IDs enable migration, but initial handshake still lands on nearest anycast node

## Architecture / flow
```mermaid
flowchart TD
    Client[Client] -->|SYN to anycast IP| ISP[ISP Edge Router]
    ISP -->|BGP best-path selection| POP1[POP A\n/24 announced\nAS_PATH: 13335]
    ISP -->|BGP best-path selection| POP2[POP B\n/24 announced\nAS_PATH: 13335]
    ISP -->|BGP best-path selection| POP3[POP C\n/24 announced\nAS_PATH: 13335]
    POP1 -->|Local health check\nOK| BGP1[BGP Announce\nLOCAL_PREF=200]
    POP2 -->|Local health check\nDEGRADED| BGP2[BGP Announce\nLOCAL_PREF=50\nCOMMUNITY=no-export]
    POP3 -->|Local health check\nDOWN| BGP3[BGP Withdraw\nor no announce]
    BGP1 --> ISP
    BGP2 --> ISP
    BGP3 --> ISP
    ISP -->|Forwards to\nbest path\n(POP A)| POP1
    POP1 -->|TCP handshake\n+ TLS + HTTP| Client
    Client -.->|Subsequent requests\nsame 5-tuple| POP1
```

## Key terms
**Anycast prefix** — An IP prefix (typically /24 IPv4, /48 IPv6) announced from multiple locations via BGP to implement one-to-nearest routing.
**Catchment area** — The set of network ingress points for which a given anycast node is the BGP best path; dynamic and topology-dependent.
**BGP community** — A transitive 32-bit tag (RFC 1997) attached to route announcements; used in anycast for traffic engineering (e.g., `no-export`, `no-advertise`, custom values for geo-fencing).
**Subnet-router anycast** — IPv6 anycast address formed by setting the interface identifier to all zeros within a subnet prefix (RFC 4291 §2.6.1).
**Anycast withdrawal** — BGP UPDATE message removing a prefix announcement, triggered by health-check failure to shift traffic to remaining nodes.
**ECS (EDNS Client Subnet)** — DNS extension (RFC 7871) allowing recursive resolvers to forward client subnet to authoritative servers, improving anycast routing accuracy for DNS.

## Example
```bash
# Bird2 BGP config snippet for anycast DNS service
protocol bgp upstream_provider {
    local as 64500;
    neighbor 192.0.2.1 as 64501;
    ipv4 {
        import filter { accept; };
        export filter {
            if net = 203.0.113.0/24 then {
                bgp_community.add((64500, 100));  # mark anycast prefix
                bgp_local_pref = 200;              # prefer this POP
                accept;
            }
            reject;
        };
    };
}

# Health check daemon withdraws on failure
protocol static {
    ipv4;
    route 203.0.113.0/24 via "lo" reject;  # blackhole when service down
}
```
Demonstrates BGP announcement of a /24 anycast prefix with LOCAL_PREF tuning and static route withdrawal for health-based failover.

## Common mistakes
- **Assuming anycast provides per-request load balancing** — BGP routes per-source-prefix, not per-flow; a single /24 client subnet hits one anycast node for all its traffic.
- **Using anycast for stateful TCP services without connection pinning** — Anycast works for DNS (stateless UDP) and HTTP (pinned TCP), but breaks for protocols requiring server-side state across packets from same client if routing shifts mid-session.
- **Setting TTL too high on anycast A/AAAA records** — 300s+ TTL delays failover; 30-60s is standard for anycast DNS/CDN to allow BGP convergence + DNS cache expiry.
- **Announcing anycast prefix without matching reverse path filtering (uRPF) config** — Asymmetric routing causes legitimate return traffic to be dropped at ingress if strict uRPF (RFC 3704) is enabled on upstream providers.

## Lesser-known internals
- **BGP convergence asymmetry**: Withdrawal propagation (path removal) is often slower than announcement propagation due to MRAI (Minimum Route Advertisement Interval, default 30s) timers and route flap damping — failover can take 2-3x longer than initial anycast deployment.
- **IPv6 anycast lacks dedicated address space**: Unlike multicast (ff00::/8), anycast uses global unicast space; this means anycast prefixes must be allocated from RIRs like unicast, and collision risk exists if two operators accidentally announce the same prefix.
- **TCP SYN flood amplification risk**: Anycast DNS over UDP is common, but anycast TCP services (e.g., HTTPS on 443) concentrate attack traffic on the nearest node — operators must deploy per-POP DDoS mitigation, not just upstream scrubbing, because the anycast prefix itself is the attack target.

## Topics to explore further
- **BGP FlowSpec (RFC 5575)** — Granular traffic filtering/steering at the routing layer for anycast DDoS mitigation
- **LISP (Locator/ID Separation Protocol, RFC 6830)** — Alternative anycast-like architecture separating identity from location
- **Anycast RP (RFC 4610)** — PIM anycast rendezvous point for multicast, distinct from unicast anycast
- **GeoDNS vs anycast tradeoffs** — When application-layer geo-routing (e.g., Route 53 latency-based routing) outperforms or complements network-layer anycast

## Learn next
- **BGP internals & path selection** — Anycast is fundamentally a BGP application; deep BGP knowledge is required to debug catchment areas and convergence
- **DNS authoritative server architecture** — Root/TLD/authoritative operators are the largest anycast deployments; understanding their patterns informs anycast design
- **QUIC connection migration** — QUIC's connection IDs enable surviving anycast routing changes mid-session, a significant evolution from TCP anycast constraints