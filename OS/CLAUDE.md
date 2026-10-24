# Linux for Developers — learning context

GJ is a Java developer learning how Linux works **as a software developer**: concepts and
"why it behaves this way", not command memorisation. Commands only appear as a way to *see*
a concept on a real machine (GJ's laptop runs Linux Mint).

## Files in this folder
- `linux_for_developers.xlsx` — GJ's master checklist. **GJ updates this sheet himself — do not edit it**
  unless explicitly asked. You may read it to see what's Done / Learning / Not started.
  - **Checklist** sheet: 335 topics in 26 sections (columns: #, Section, Topic, Priority, Status, Notes).
  - **Progress** sheet: per-section COUNTIFS summary (formulas).
  - **Book** sheet: companion book *Operating System Concepts* (10th ed., Silberschatz, Galvin, Gagne),
    chapter → section mapping.
- `CLAUDE.md` — this file: how to teach GJ + where we are.
- `reference/` — copies of system files we looked at (unistd_64.h, syscalls.h).
- `examples/` — small code we wrote to try things (e.g. `hello-module/`, a kernel module).
- `summaries/` — short revision summaries per batch of topics. `summaries/init.md` = topics 1–5 + side
  discussions. Add a new file (e.g. `topics-6-10.md`) as we finish more topics; keep the table at the top.
- `syscalls-reference.html` / `syscalls-reference-dark.html` — GJ's syscall cheat sheet (light / dark).
- `session-notes.md` — detailed record of what has been explained so far. Read it before continuing
  so you don't repeat or contradict earlier explanations. Append to it after each session.

## How GJ wants to learn
- Explain concepts first, then show how to observe them (`/proc`, `strace`, `ps`, `lscpu`…).
- Use Java examples where code helps (GJ is a Java developer).
- Diagrams are welcome for mental models (memory layout, thread/CPU mapping, flows).
- When GJ says "just list topics / don't explain", list only.
- End each topic by offering the next one in the checklist order.

## Sections (study order)
1 Fundamentals · 2 Everything Is a File · 3 Directory Structure (FHS) · 4 Users, Groups, Permissions ·
5 Processes · 6 Shell and Environment · 7 Memory · 8 Filesystems/Storage · 9 Boot & Init ·
10 Logging · 11 Networking · 12 Libraries/Binaries/Packages · 13 Debugging/Observability ·
14 Containers · 15 Security · 16 IPC & Concurrency · 17 Kernel Internals · 18 Developer Gotchas ·
19 Tooling · 20 Architecture & Trade-offs · 21 Concurrency Theory · 22 Performance & Capacity ·
23 Reliability & Operations · 24 Distributed Systems Basics · 25 Virtualization & Cloud ·
26 Hardware & OS Theory

## Where we are (as of 2026-10-20)
- **Covered:** Kernel vs userspace · Kernel mode vs user mode.
- **Covered (Session 2):** System calls (#3) · Distributions (#4) · Monolithic kernel & loadable modules (#5) · Kernel versions (#6, not yet marked in the sheet/summaries).
- **Side discussions (Session 2):** `close()`/try-with-resources meaning; how the kernel handles USB / new devices.
- **Side discussions covered:** CPU/core/logical-CPU and thread scheduling; multithreaded memory
  model (shared vs per-thread); process vs application; PID/TID limits. (Details in session-notes.md.)
- **Next (Section 1 remaining, sheet order #7–13):** Open source & licensing (GPL, MIT) · History of Unix and Linux ·
  Unix philosophy · POSIX standards · Monolithic vs microkernel vs hybrid · Linux vs BSD vs
  Windows vs macOS design · LTS vs rolling releases.
- **Then:** Section 2 "Everything Is a File".
- Sheet status (2026-10-20): at GJ's explicit request, #4 and #5 were marked **Done** in the xlsx
  (Progress cached totals updated too). #1–3 are still "Not started" in the sheet although covered —
  GJ can mark them himself. Check the sheet is not open in LibreOffice (`.~lock` file) before editing it.
- Open offer not yet taken: run `strace -f -c` on GJ's Java sorter program and read the output together.

## Saving context (do this every session)
- After each topic/side discussion, append a numbered entry to `session-notes.md` under the
  current session heading (key points, Java examples, observe-commands, anything GJ asked).
- Keep the "Where we are" block above current (Covered / Next / open offers).
