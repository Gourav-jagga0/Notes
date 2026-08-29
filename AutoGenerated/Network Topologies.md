# Network Topologies

## What it is
Network topology defines the physical and logical arrangement of nodes and links in a network. Physical topology describes actual cabling and device placement; logical topology describes data flow paths regardless of physical layout. Both determine failure domains, bandwidth sharing, and routing complexity.

## Why it matters
Topology directly constrains latency bounds, fault tolerance, and scaling characteristics — choices made at design time propagate to every protocol layer above. Misaligned topology causes broadcast storms, single points of failure, or O(n²) cabling costs that no protocol tuning can fix.

## How it actually works

**Bus** — All nodes share a single coaxial or twisted-pair backbone (10BASE2/10BASE5). CSMA/CD governs access: nodes transmit when carrier sense detects idle; collisions trigger exponential backoff (binary exponential backoff, max 16 attempts per IEEE 802.3). Terminators (50 Ω) at both ends prevent signal reflection. Maximum segment length: 185 m (10BASE2) or 500 m (10BASE5). Bandwidth is half-duplex, shared — aggregate throughput drops linearly with node count.

**Star** — Each node connects point-to-point to a central switch/hub via dedicated link (typically UTP Cat5e+). Switches operate at Layer 2, learning MAC addresses via source inspection, building forwarding tables (CAM table). Full-duplex operation eliminates collisions; no CSMA/CD needed. Failure domain limited to single link. Uplink bandwidth between switches becomes bottleneck — typically aggregated via LACP (IEEE 802.3ad). Maximum cable run: 100 m per TIA/EIA-568.

**Ring** — Nodes form closed loop; each forwards frames to next neighbor. Token Ring (IEEE 802.5) uses 3-byte token circulating at 4/16 Mbps; only token holder transmits. FDDI (ANSI X3T9.5) dual-ring at 100 Mbps over fiber — primary ring active, secondary provides wrap-around on failure (50 ms typical). Deterministic latency: max wait = token rotation time (TRT). No collisions, but single break partitions network unless dual-ring.

**Mesh** — Every node connects to every other (full mesh: n(n-1)/2 links) or subset (partial mesh). Routing uses link-state (OSPF, IS-IS) or distance-vector (BGP) protocols. Full mesh provides n-1 disjoint paths — survives any n-2 failures. OSPF SPF calculation O(E log V) per area; LSDB sync floods LSAs every 30 min (refresh) or on change. BGP path-vector avoids loops via AS_PATH attribute. Partial mesh trades path diversity for link count — common in WAN/backbone.

**Hybrid** — Combinations: star-of-stars (core/distribution/access layers), star-ring (FDDI backbone with star drops), mesh-star (data center spine-leaf). Spine-leaf: every leaf connects to every spine (full bipartite). ECMP (Equal-Cost Multi-Path) hashes flows across spines — 5-tuple hash (src/dst IP, src/dst port, protocol) per RFC 2992. Oversubscription ratio (typically 3:1) determines leaf-to-spine uplink count.

## Architecture / flow
```
Host --(100m UTP)--> Access Switch --(10/40/100G)--> Spine Switch --(ECMP)--> Remote Leaf --> Host
                    |                    |
                    +-- L2 domain        +-- L3 routed (OSPF/IS-IS/BGP)
                    |  (STP/RSTP)        |  (ECMP hash)
                    v                    v
              Broadcast domain      No broadcast
              limited to VLAN       across spines
```

## Key terms
**Collision domain** — Segment where CSMA/CD contention occurs; eliminated per-port by switches.
**Broadcast domain** — Set of nodes receiving Layer 2 broadcasts; bounded by VLANs (IEEE 802.1Q).
**Spanning Tree Protocol (STP/RSTP/MSTP)** — Loop prevention in L2 topologies; RSTP (802.1w) converges in ~1 sec vs 30-50 sec.
**ECMP** — Per-flow load balancing across equal-cost paths; hash stability requires consistent 5-tuple.
**Oversubscription ratio** — Ratio of downlink to uplink bandwidth; 3:1 typical for spine-leaf.
**Token rotation time (TRT)** — Max latency bound in token ring; TRT = ring latency + sum of node hold times.

## Example
```bash
# Spine-leaf ECMP verification (Linux)
ip route show 10.0.0.0/8
# 10.0.0.0/8 nexthop via 10.1.1.1 dev eth1 weight 1
#     nexthop via 10.1.2.1 dev eth2 weight 1
#     nexthop via 10.1.3.1 dev eth3 weight 1
# Demonstrates equal-cost multipath across 3 spine uplinks
```

## Common mistakes
- Assuming star topology eliminates all contention — head-of-line blocking at switch egress queues still causes latency spikes under oversubscription.
- Configuring STP priorities without root bridge placement — default bridge ID (32768 + MAC) often elects suboptimal root, creating asymmetric paths.
- Using LACP without matching hash algorithms on both ends — causes flow polarization (all traffic on one physical link).
- Deploying full mesh in large clusters — O(n²) links exceed switch port density; partial mesh with route reflectors (BGP) or areas (OSPF) required.

## Lesser-known internals
- **RSTP proposal/agreement handshake** converges point-to-point links in 3 BPDU exchanges (~100 ms) but falls back to 802.1D timers on shared-media ports — half-duplex links disable fast transition.
- **ECMP hash polarization** — identical 5-tuple hashes across multiple tiers cause flow collapse; mitigate with per-tier hash seeds (RFC 7424) or adaptive hashing.
- **FDDI dual-ring wrap** — on single break, stations adjacent to break loop back; wrap completes in <50 ms but doubles traffic on remaining ring — capacity planning must account for 2x load during failure.

## Learn next
- **TCP/UDP** — Transport protocols operate atop topology; congestion control reacts to loss patterns topology creates.
- **Load balancing** — ECMP and LACP are topology-aware load distribution; higher-layer LB (L4/L7) builds on these primitives.
- **Distributed Systems: CAP theorem** — Network partitions are topology failures; topology design defines partition probability and blast radius.