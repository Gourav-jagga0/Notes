# init — Topics 1–5 summary (Section 1: Fundamentals)

Quick-revision sheet for everything covered up to and including Session 2 (2026-10-20).
Full explanations live in `../session-notes.md`; this file is the short version to re-read.

| # | Topic | Status |
|---|-------|--------|
| 1 | Kernel vs userspace | covered |
| 2 | Kernel mode vs user mode | covered |
| 3 | System calls | covered |
| 4 | Distributions (distro) and what they include | Done |
| 5 | Monolithic kernel and loadable modules | Done |

> Note: the detailed explanations of #1–2 were not recovered from the original chat; the summaries
> below for those two are the short form only.

---

## 1. Kernel vs userspace
- **Linux is only the kernel.** Everything else (shell, `ls`, Java, browsers) is **userspace**.
- The kernel manages the hardware and shared resources: CPU time, memory, files, devices, network.
- Userspace programs never touch hardware directly; they ask the kernel.

## 2. Kernel mode vs user mode
- The CPU has privilege levels. **Kernel mode** can do anything (talk to devices, change page tables,
  switch threads). **User mode** can only compute and touch its own memory.
- This split is enforced by the **hardware**, not by good manners.

## 3. System calls
- A **syscall is the only legal door from user mode into kernel mode.** Needed for files, network, screen,
  threads, memory growth, the clock (except vDSO).
- Flow for `read`: Java `in.read()` → JVM → glibc `read()` wrapper → syscall number (read = 0 on x86_64)
  + args in registers → `syscall` instruction → hardware switches to kernel mode → kernel **validates
  everything** → does the work (may sleep) → result in register → back to user mode → bytes or `-1` + errno.
- ~350–450 numbered syscalls; defined in `reference/unistd_64.h` (numbers) and `reference/syscalls.h`
  (kernel prototypes, `__user` pointer marker). See also `../syscalls-reference.html` / `-dark.html`.
- Java → syscall: `FileInputStream`→`openat`/`read`; `println`→`write(1,…)`; `Socket`→`socket`/`connect`;
  `Thread.start`→`clone`; contended lock→`futex`; heap growth→`mmap`/`brk`; `ProcessBuilder`→`clone`+`execve`.
- **Why developers care:** (1) cost — far more than a function call ⇒ buffering (`BufferedReader`);
  (2) blocking — thread sleeps inside the kernel ⇒ NIO/`epoll`, virtual threads;
  (3) errno → exceptions (`EACCES`, `ENOENT`, `EMFILE`, `ECONNREFUSED`, `EADDRINUSE`);
  (4) `strace` shows what any program does.
- **vDSO:** `clock_gettime` (`System.nanoTime`) runs in user mode, so strace often doesn't show it.
- Observe: `strace -e trace=openat,read,write cat /etc/hostname`, `strace -c ls /`, `strace -f -c java Sorter`.

## 4. Distributions
- **Distro = kernel + C library (glibc/musl) + core userland (bash, coreutils, systemd) + package manager and
  repositories + installer + desktop/defaults + release policy and security updates.**
- Same kernel everywhere; distros differ in packaging, versions, defaults, support lifetime.
- Families: **Debian** (apt/.deb) → Ubuntu → Mint · **Red Hat** (dnf/.rpm) → Fedora, RHEL, Rocky ·
  **Arch** (pacman) · **SUSE** (zypper) · **Alpine** (apk, musl, tiny, Docker favourite).
- **GJ's laptop:** Linux Mint 22 "Wilma" ← Ubuntu 24.04 "noble" ← Debian; kernel `6.8.0-138-generic`;
  glibc 2.39; PID 1 = systemd; ~2993 packages.
- **Why it matters:** glibc version decides binary compatibility (`GLIBC_2.xx not found`); Docker base image
  choice (debian-slim vs alpine/musl); package names and paths; support window.
- Observe: `cat /etc/os-release`, `dpkg -S /usr/bin/ls`, `apt-cache policy bash`, `ldd --version`.

## 5. Monolithic kernel and loadable modules
- **Monolithic:** scheduler, memory manager, filesystems, network and drivers all share **one address space
  in kernel mode**. Fast (plain function calls), but **no walls between kernel parts** — a bad driver can
  panic the whole machine. (Microkernel alternative: drivers in userspace, safer but slower — topic #11.)
- **Loadable modules (`.ko`):** kernel code loaded/unloaded at runtime without reboot, running with full
  kernel privilege. Linux = monolithic *design*, modular *packaging*. Config per feature: `=y` built in,
  `=m` module, off. Modules live in `/lib/modules/<version>/` and must match the running kernel exactly.
- **Auto-loading:** device appears → uevent with a modalias → udev runs `modprobe` → alias/dependency files
  → `finit_module` syscall → driver binds → `/dev` node appears.
- **Two walls, don't confuse them:** (a) user program ↔ kernel = strong, hardware enforced, only via the
  checked syscall gate; (b) kernel part ↔ kernel part = none. Many CPUs run kernel code at the same time
  (like many Java threads in one `ConcurrentHashMap`); kernel data is protected by internal locks.
- **Why developers care:** out-of-tree drivers (NVIDIA, VirtualBox) rebuilt per kernel via DKMS; containers
  share the host kernel and can't load their own modules; modules are a security risk ⇒ Secure Boot and
  module signing.
- Build one: `build-essential` + `linux-headers-$(uname -r)`; example in `../examples/hello-module/`.
  Load `sudo insmod hello.ko`, read `dmesg`, unload `sudo rmmod hello`. Try in a VM first.
- Observe: `lsmod`, `modinfo usb_storage`, `/proc/modules`, `/sys/module/`.

---

## Side discussions (Session 1 and 2)

### CPUs and threads
- "CPU" = **socket** (chip) → **core** (physical unit) → **logical CPU** (hardware thread; 1 core = 2 logical
  CPUs with hyperthreading). Linux tools (`top`, `nproc`, `taskset`) count logical CPUs.
- The scheduler spreads threads across physical cores first, then uses sibling logical CPUs; it prefers
  keeping a thread on the same CPU (cache warmth) and migrates when load-balancing needs it.
- Pin with `taskset -c 0,2,4,6 java Sorter`; inspect with `lscpu -e`, `ps -L -o pid,tid,psr,comm -p <pid>`.

### Memory of a multithreaded program
- **Shared by all threads:** code, heap (`new`), static fields, open files. **Private per thread:** stack
  (locals, call chain), registers, program counter.
- Disjoint slices of a shared array need no locking; a shared counter written by many threads does.

### Processes, applications, IDs
- One application ≠ always one process (Chrome = many; PostgreSQL/nginx = master + workers; Java sorter = 1).
- PIDs and TIDs come from one finite pool: `/proc/sys/kernel/pid_max`, `threads-max`, `ulimit -u`.
  Exhaustion ⇒ "Resource temporarily unavailable".

### What `close()` / try-with-resources really does
Not only kernel resources. It (1) releases kernel fds (leak ⇒ `EMFILE`), (2) **flushes user-space buffers**
(`BufferedWriter`, gzip/zip trailers — forgetting loses or corrupts data), (3) has protocol meaning (TCP FIN,
JDBC pool return, release of `FileLock`), (4) frees non-kernel resources (`Arena`, class loaders).
try-with-resources closes in reverse order, null-safe, and keeps the original exception (close errors become
*suppressed*). `finally` is more general ("always undo", e.g. `lock.unlock()`).

### How a USB device reaches you
Plug → kernel **enumerates** (descriptors: vendor ID, product ID, class) → matches a driver (loads the module
if needed) → block device `/dev/sdb1` → uevent → `udev` makes the node → `udisks2` mounts → file manager.
The kernel stops at the block device; mounting and showing it is **userspace**. A new custom device: follow a
standard class (no driver needed), or write a userspace driver with `libusb`, or write a kernel module.

### Secure Boot
Verifies only the **boot chain**. Doesn't stop root loading a module afterwards, kernel bugs, or userspace
malware — so it is combined with kernel lockdown and signed modules. Layers, not one feature.
Check with `mokutil --sb-state`.

### Keyboard with several CPUs and programs
Key press → interrupt on one CPU → driver queues the event under a lock → wakes readers. Normally the display
server reads `/dev/input/event*` and routes keys to the focused window. On a tty, two readers split the bytes.
On the raw input device each opener gets its own copy of every key (how keyloggers work ⇒ permissions).

---

## Where to go next
Section 1 remaining: #6 Kernel versions · #7 Open source and licensing · #8 History of Unix and Linux ·
#9 Unix philosophy · #10 POSIX · #11 Monolithic vs microkernel vs hybrid · #12 Linux vs BSD vs Windows vs
macOS design · #13 LTS vs rolling. Then Section 2: Everything Is a File.

Open offers: run `strace -f -c` on the Java sorter and read it together; try the hello-module build in a VM.
