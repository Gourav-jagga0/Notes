# Anycast

## What it is
Anycast is a network addressing and routing methodology where a single IP address is assigned to multiple interfaces (typically on different hosts in different locations), with routing protocols directing packets to the topologically nearest instance based on BGP path selection. Unlike unicast (one-to-one) or multicast (one-to-many), anycast delivers packets to **one** of multiple potential destinations sharing the same address, determined dynamically by the routing infrastructure.

## Why it matters
Anycast enables stateless horizontal scaling of UDP-based services (DNS, NTP, CDN edges) across geographically distributed infrastructure without client-side configuration changes, providing automatic failover and latency optimization through BGP convergence rather than application-layer logic.

## How it actually works

### Core mechanism
Anycast operates entirely at **Layer 3 (IP)** and **Layer 4 (transport)**. Multiple servers advertise the **same IP prefix** (typically `/32` for IPv4, `/128` for IPv6) into BGP from different autonomous systems (ASes) or different locations within the same AS. BGP's best-path selection algorithm (RFC 4271) chooses a single path per destination prefix per router, so traffic from any given source follows the "shortest" path per BGP policy — not necessarily geographic distance.

**BGP path selection order (simplified, Cisco-style):**
1. Highest `WEIGHT` (Cisco proprietary, local to router)
2. Highest `LOCAL_PREF` (default 100, propagated within AS)
3. Locally originated routes (network/aggregate/redistribute)
4. Shortest `AS_PATH` length
5. Lowest `ORIGIN` type (IGP < EGP < INCOMPLETE)
6. Lowest `MED` (Multi-Exit Discriminator, advertised to neighbors)
7. eBGP over iBGP paths
8. Lowest IGP metric to BGP next-hop
9. Oldest route (stability tiebreaker)
10. Lowest router ID of advertising neighbor
11. Lowest neighbor IP address

### IPv4 vs IPv6 anycast addressing
- **IPv4**: Anycast addresses are indistinguishable from unicast — same address space, same prefix length. Operational convention uses `/32` host routes.
- **IPv6**: RFC 4291 reserves the **subnet-router anycast address** (interface ID = all zeros) for each subnet prefix. General anycast addresses are syntactically identical to unicast; the distinction is purely operational (advertised from multiple locations).

### DNS anycast (authoritative)
Root servers (A–M) and TLD operators deploy anycast for authoritative DNS:
- Each logical root server (e.g., `k.root-servers.net`) maps to **one IPv4 `/32` + one IPv6 `/128`** advertised from 100+ physical sites globally
- Queries arrive via UDP/53 (primarily) — stateless, so routing changes mid-flow don't break transactions
- **RFC 4786** (Operation of Anycast Services) defines requirements: health checks, withdrawal on failure, monitoring
- **EDNS Client Subnet (ECS, RFC 7871)** partially mitigates anycast's opacity to recursive resolvers by passing client subnet, but adoption is incomplete

### CDN anycast (HTTP/HTTPS)
CDNs (Cloudflare, Fastly, CloudFront via Global Accelerator) use anycast for **TCP/80, TCP/443**:
- **Critical challenge**: TCP is stateful. BGP reconvergence (link failure, policy change) can shift a flow to a different physical server mid-connection → **RST** or silent blackhole
- Mitigations:
  - **Connection draining**: Stop advertising prefix, wait for existing TCP connections to finish (requires `BGP graceful shutdown` community `GRACEFUL_SHUTDOWN` 0xFFFF0000, RFC 8326)
  - **Anycast + L7 load balancer**: Anycast terminates at edge PoP, then L7 LB distributes to backend pool (preserves TCP state locally)
  - **QUIC/HTTP/3 (UDP/443)**: Connection ID survives path migration — anycast-friendly by design (RFC 9000)

### Health checking & withdrawal
Anycast has **no built-in health signaling**. Operators must:
1. Run active health checks (HTTP GET, DNS query, TCP SYN) from each PoP to local service instance
2. On failure: withdraw the anycast prefix from BGP (stop advertising) — typically via `bgp withdraw` or setting `LOCAL_PREF=0`
3. Withdrawal propagates at BGP convergence speed (**seconds to minutes**, not milliseconds)
4. **RFC 4786 §4.2**: "Anycast service operators SHOULD implement automated withdrawal"

### TCP anycast failure modes
| Scenario | Result |
|----------|--------|
| BGP reconvergence mid-flow | Packets route to new PoP → no TCP state → **RST** or drop |
| Asymmetric routing (ingress ≠ egress PoP) | Return packets hit different server → **RST** |
| Client IP changes (mobile handover) | New path selected → same as reconvergence |
| DoS on one PoP | Traffic shifts to others — **amplifies load** on healthy nodes |

### Key RFCs & standards
- **RFC 1546** (1993): Host Anycasting Service (obsolete, historical)
- **RFC 4291** (2006): IPv6 Addressing Architecture — defines subnet-router anycast
- **RFC 4786** (2006): Operation of Anycast Services — operational requirements
- **RFC 8326** (2018): BGP Graceful Shutdown — `GRACEFUL_SHUTDOWN` community
- **RFC 9000** (2021): QUIC — connection migration survives anycast shift

## Architecture / flow

```mermaid
flowchart TD
    Client[Client\n192.0.2.100] -->|UDP/53 DNS Query| ISP[ISP Recursive Resolver]
    ISP -->|BGP Path Selection| RootA[Root Server K\n193.0.14.129/32\nPoP: Amsterdam]
    ISP -->|BGP Path Selection| RootB[Root Server K\n193.0.14.129/32\nPoP: Singapore]
    ISP -->|BGP Path Selection| RootC[Root Server K\n193.0.14.129/32\nPoP: San Jose]
    RootA -->|UDP/53 Response| ISP
    RootB -.->|Withdrawn\n(health check fail)| BGP[BGP RIB]
    RootC -->|UDP/53 Response| ISP
    BGP -->|Best Path: Amsterdam\nAS_PATH len 2| ISP
    Health[Health Checker\nper-PoP] -->|HTTP/TCP probe| RootA
    Health -->|FAIL → withdraw| RootB
    Health -->|PASS → advertise| RootC
```

## Key terms
**BGP Best Path Selection** — Deterministic algorithm (RFC 4271) choosing one route per prefix per router based on ordered attributes (WEIGHT, LOCAL_PREF, AS_PATH, ORIGIN, MED, etc.)

**Prefix Withdrawal** — BGP UPDATE message removing a previously advertised prefix; triggers reconvergence to next-best path

**Anycast Prefix** — IP prefix (typically `/32` IPv4, `/128` IPv6) advertised from multiple geographically dispersed locations

**PoP (Point of Presence)** — Physical location where anycast service instance terminates traffic; runs local health checker

**Connection Draining** — Graceful cessation of new connections while allowing in-flight TCP flows to complete before prefix withdrawal

**ECS (EDNS Client Subnet)** — DNS extension (RFC 7871) passing client /24 (IPv4) or /56 (IPv6) to authoritative server for geo-aware responses

## Example
```bash
# Bird 2.x BGP config: anycast prefix advertisement with health-check-driven withdrawal
protocol bgp anycast_upstream {
    local as 64500;
    neighbor 192.0.2.1 as 64501;
    ipv4 {
        import filter { reject; }  # don't learn full table
        export filter {
            if net = 198.51.100.0/24 then accept;  # anycast prefix
            else reject;
        };
    };
    graceful restart on;
}

# Health check script (systemd timer, runs every 10s)
# /usr/local/bin/anycast-health.sh
#!/bin/bash
if ! curl -sf --max-time 2 http://127.0.0.1:8080/health >/dev/null; then
    birdc "configure soft" <<'EOF'
    protocol bgp anycast_upstream {
        ipv4 { export filter { reject; }; };
    }
EOF
fi
```
Demonstrates BGP prefix withdrawal via Bird socket API when local health endpoint fails.

## Common mistakes
1. **Assuming anycast = geographic proximity** — BGP selects by policy (AS_PATH, LOCAL_PREF, MED), not latitude/longitude. A client in London may route to New York if AS_PATH is shorter.
2. **Running TCP anycast without connection draining** — BGP withdrawal during active flows causes RST storms. Must implement `GRACEFUL_SHUTDOWN` community (RFC 8326) and wait for `TCP_TIME_WAIT` (default 60s Linux) before full withdrawal.
3. **Omitting per-PoP health checks** — Without local probes, a failed service instance continues attracting traffic (blackhole). Health check must validate **application-layer** liveness, not just interface up.
4. **Expecting sub-second failover** — BGP convergence is typically **3–30 seconds** (MRAI timers, route flap damping). Anycast failover is not a substitute for L7 load balancer health checks.

## Lesser-known internals
- **IPv6 subnet-router anycast** (RFC 4291 §2.6.1): Every `/64` subnet has a reserved anycast address (interface ID `::`). Packets sent to it reach **one** router on that subnet — used for mobile IPv6 home agents, not general service anycast.
- **BGP ADD-PATH (RFC 7911)** allows advertising **multiple paths** for the same prefix to a single neighbor. Can be used to pre-install backup anycast paths, reducing reconvergence time, but requires both speakers to support it.
- **Anycast + RPKI/ROA**: If your anycast prefix is covered by a ROA with `maxLength` matching your `/32`, all PoPs must originate from the **same ASN** (or ROA must list all originating ASNs). Cross-AS anycast requires careful ROA design.

## Topics to explore further
- **BGP FlowSpec (RFC 5575)** — Propagate firewall rules via BGP; can mitigate DDoS on anycast prefixes dynamically
- **QUIC Connection Migration (RFC 9000 §9)** — How connection IDs enable seamless anycast path changes for HTTP/3
- **RPKI/ROA for Anycast Prefixes** — Origin validation challenges when same prefix announced from multiple ASNs

## Learn next
- **BGP Internals & Path Selection** — Anycast is built on BGP; understanding attribute manipulation (LOCAL_PREF, MED, communities) is essential for traffic engineering
- **QUIC/HTTP/3 Transport** — Solves TCP anycast's statefulness problem via connection IDs; the future of anycast web services
- **DNS Architecture (Recursive vs Authoritative)** — Anycast is the standard deployment model for authoritative DNS; understanding query flow clarifies why it works