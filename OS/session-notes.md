# Session notes — Linux for Developers

Record of what has been explained, in order. Newest at the bottom.

---

## Session 1 (2026-10-20, claude.ai — moved across accounts)

### 1. Topic list created
- GJ asked for a complete topic sheet: "everything from file permissions to kernels, directory
  structure to handling groups" — as a developer, not commands.
- First produced a tiered overview (Tier 1 must-know, Tier 2 should-know, Tier 3 good-to-know),
  a suggested learning order, and "10 concepts that explain the most bugs":
  permissions/ownership · file descriptors & limits · env vars & PATH · signals & exit codes ·
  127.0.0.1 vs 0.0.0.0 · disk vs inode exhaustion · memory limits & OOM killer ·
  dynamic linking & glibc vs musl · systemd service behaviour · namespaces/cgroups.
- GJ: "don't explain for now, just list topics" → turned into a plain checklist (sections 1–19),
  later expanded to 26 sections (20–26: Architecture & Trade-offs, Concurrency Theory,
  Performance & Capacity, Reliability & Operations, Distributed Systems Basics,
  Virtualization & Cloud, Hardware & OS Theory) and exported to `linux_for_developers.xlsx`
  with the companion book (Silberschatz *Operating System Concepts*, 10th ed.).

### 2. Kernel vs userspace / kernel mode vs user mode  ✅
- Linux = only the kernel. A distro = kernel + tools + package manager + defaults.
- Programs run in user mode and ask the kernel for everything (files, network, memory) via
  system calls. Every language runtime (Java, Python, Node, Go) ultimately makes syscalls.
- (The detailed explanation from the original chat was not recovered; only these points survive.)

### 3. Threads and CPUs (side discussion)
- "CPU" means three things: **socket** (physical chip) → **core** (physical unit) →
  **logical CPU** (hardware thread; with hyperthreading 1 core = 2 logical CPUs). Linux's
  "CPU" in `top`, `nproc`, `taskset` = logical CPU.
- Logical CPU → core mapping is fixed by hardware; software threads are scheduled onto logical
  CPUs freely, so a process's threads can be on different cores while sibling logical CPUs run
  other programs, or two of its own threads can share one core's siblings.
- Scheduler behaviour: spread across physical cores first → use siblings when cores are busy →
  prefer staying on the same logical CPU (cache warmth) → migrate when load-balancing needs it.
- For GJ's Java sorting program (4 threads on a 4-core/8-thread CPU, idle machine): usually one
  thread per physical core. Speed depends on free physical cores, not logical CPU count.
- CPU affinity: `taskset -c 0,2,4,6 java Sorter`; inspect with `lscpu -e` and
  `ps -L -o pid,tid,psr,comm -p <pid>` (PSR = current logical CPU; changes over time).

### 4. Multithreaded memory model (diagram was drawn in original chat)
- **Shared by all threads (one copy):** code, heap (everything from `new` in Java),
  globals/static fields, open files (fd table).
- **Private per thread:** stack (locals, parameters, call chain), registers + program counter.
- Threads share one address space, so a heap object has the same address in every thread —
  no copying; they just follow the reference.
- Java example: `int[] data = new int[n]` on the heap shared by 4 `SortTask` threads, each
  sorting its own slice `data[from..to]`; locals inside `run()` are on each thread's stack.
  Disjoint slices ⇒ no locking needed. Shared counter written by many threads ⇒ race ⇒ sync.
- Table: shared = accessible by other threads, needs locking when written by >1 thread, lives
  until unreferenced (GC); private = not accessible, never needs locking, lives until method returns.
- Rule: keep data thread-private where you can.

### 5. Process size, applications, PIDs (side discussion)
- An application is not always one process: Java sorter = 1 process; Chrome = many (per tab +
  helpers); PostgreSQL / nginx = master + workers. Threads vs processes = sharing/cheapness vs isolation.
- PIDs are finite: `/proc/sys/kernel/pid_max` (32768 older, up to 4194304 on 64-bit),
  `/proc/sys/kernel/threads-max`, `ulimit -u`. Threads draw TIDs from the same pool; main thread
  TID == PID. Exhaustion ⇒ "Resource temporarily unavailable". IDs are reused.
- Observe: `ps -L -p <pid>`, `ls /proc/<pid>/task`, `cat /proc/<pid>/maps` (one heap, one stack per thread).

### 6. System calls — NOT actually covered with GJ (correction, Session 2)
> GJ confirmed in Session 2 that system calls had not been started. The points below were
> written into the notes earlier but never discussed; the real explanation is in Session 2.
- User mode can only compute and touch its own memory; anything else (disk, network, screen,
  other processes, clock) needs the kernel. A syscall is the only door between modes.
- Flow for `read(fd, buf, 100)`: Java `in.read()` → JVM native → glibc `read()` wrapper → syscall
  number (0 = read on x86_64) + args in registers → `syscall` CPU instruction → hardware switch to
  kernel mode → kernel validates everything (fd, permissions, buffer pointer) → does work (may sleep
  waiting for disk) → result in register → back to user mode → returns bytes or -1 + errno.
- Kernel trusts nothing; ~350–450 syscalls, each identified by number; usually called via glibc.
- Java mapping: `FileInputStream.read`, `Socket.connect`, `Thread.start`, time calls → syscalls.
- Families: files (open/read/write/close/stat/lseek), processes (fork/clone/execve/exit/wait4/kill),
  memory (mmap/munmap/brk), network (socket/bind/listen/accept/connect/send/recv),
  multiplexing (epoll_wait/poll), threads (clone — Linux threads are processes sharing memory; futex for locks).
- Why devs care: (1) cost — far more than a function call ⇒ buffering (`BufferedReader`) exists;
  (2) blocking — sleeping in the kernel ⇒ leads to non-blocking I/O & epoll; (3) errors — Java
  exceptions map to errno: EACCES, EMFILE ("Too many open files"), ECONNREFUSED, EADDRINUSE;
  (4) strace shows what any program actually does.
- Observe: `strace -c ls`, `strace -e trace=openat,read cat /etc/hostname`, `strace -f -c java Sorter`
  (JVM makes many mmap/openat calls before main()).
- vDSO: `clock_gettime`/`gettimeofday` (behind `System.nanoTime()`) run without entering the kernel,
  so strace often doesn't show them.

### Left off
Offered next: system calls, then finish Section 1 (distros, LTS vs rolling, monolithic kernel & modules, kernel versions,
licensing) or trace GJ's Java sorter with strace together.

---

## Session 2 (2026-10-20, Claude Code in the Notes/OS folder)
- Context reloaded from CLAUDE.md, session-notes.md and the xlsx (all rows still "Not started").
- Agreed: everything discussed gets appended here; CLAUDE.md "Where we are" kept current.
- Correction from GJ: only #1–2 (kernel vs userspace, kernel/user mode) were covered.
  System calls had not been started, so this session starts there.

### 1. System calls (#3) — explained
- Why: user mode can only compute + touch its own memory; files, network, screen, threads, memory
  growth, clock all need the kernel. Syscall = the only door from user mode to kernel mode.
- Flow diagram (Java `in.read(buf)`): Java → JVM native → glibc `read()` wrapper → syscall number
  (read=0 on x86_64) + args in registers → `syscall` instruction → hardware mode switch → kernel
  validates everything (fd, buffer is caller's memory) → does work, may sleep → result in register →
  back to user mode → glibc returns bytes or -1 + errno → JVM returns / throws IOException.
- ~350–450 numbered syscalls; called via glibc; kernel trusts nothing; not a normal function call.
- Java → syscall table: FileInputStream→openat; read/write; println→write(1,…); Socket→socket+connect;
  accept; Thread.start→clone; contended locks→futex; heap growth→mmap/brk; ProcessBuilder→clone/fork+execve;
  System.exit→exit_group.
- Why devs care: (1) cost ~100ns–µs vs ~1ns call ⇒ buffering (BufferedReader); (2) blocking — thread
  sleeps in kernel ⇒ thread-per-connection, NIO/epoll, virtual threads; (3) errno→exceptions
  (EACCES, ENOENT, EMFILE, ECONNREFUSED, EADDRINUSE); (4) strace sees any program.
- vDSO: clock_gettime (System.nanoTime) runs in user mode, no real syscall ⇒ often absent in strace.
- Observe: `strace -e trace=openat,read,write cat /etc/hostname`, `strace -c ls /`,
  `strace -f -c java Sorter`. Suggested GJ paste output to read together.
- Where syscalls are defined (copied to `reference/`): `unistd_64.h` (number table, `__NR_read 0`;
  origin /usr/include/x86_64-linux-gnu/asm/) and `syscalls.h` (kernel prototypes e.g. `sys_read` at line 462,
  `__user` pointer marker; origin /usr/src/linux-headers-6.8.0-138/include/linux/). Kernel .c source
  (ksys_read, vfs_read) is not installed locally; could be read from kernel.org.

### 2. Side question: is `close()` / try-finally / try-with-resources only for kernel resources?
- No. `close()` is "finish and release", which covers four different things:
  1. **Kernel resources** — file/socket/pipe fds → `close(fd)` syscall. Not closing ⇒ fd leak ⇒ `EMFILE`
     ("Too many open files"). GC/Cleaner is only a nondeterministic safety net, don't rely on it.
  2. **Flushing user-space buffers** — `BufferedWriter`/`BufferedOutputStream.close()` calls `flush()`,
     which does the real `write` syscall. Forgetting close ⇒ data silently lost. `GZIPOutputStream` /
     `ZipOutputStream` write trailer / central directory on close ⇒ otherwise corrupt file.
  3. **Protocol / logical meaning** — `Socket.close()` sends TCP FIN; JDBC `Connection.close()` on a pool
     returns the connection (the kernel socket stays open!); `ResultSet`/`Statement` free server cursors;
     HTTP response body close returns the connection to the pool; `FileChannel.close()` releases `FileLock`s;
     `Files.list()/walk()` streams hold a directory fd.
  4. **Non-kernel resources** — `Arena.close()` frees native memory (Java 22 FFM), `URLClassLoader.close()`.
- try-with-resources = syntax for `AutoCloseable`: closes in reverse order, null-safe, and keeps the
  original exception (close() failures become *suppressed*; in plain try-finally a throwing close() can
  hide the real exception).
- `finally` is more general: "always undo" — e.g. `lock.lock(); try { … } finally { lock.unlock(); }`
  (a lock is not a kernel resource; uncontended it is just a CPU atomic).
- Observe: `ls -l /proc/<pid>/fd`, `strace -e trace=close,openat`, `ulimit -n`.

### 3. Side question: how does the kernel handle a USB device, and a brand-new device GJ might build?
- Kernel talks to hardware through **drivers** (kernel code, often loadable modules). Kernel is responsible
  for detecting + talking to the device; showing files in the file manager is **userspace** (udev + udisks2
  + desktop automount), not the kernel.
- Plug-in flow: hub detects voltage change → kernel **enumerates**: resets port, assigns address, asks
  device for its **descriptors** (vendor ID, product ID, device class, interfaces, endpoints) → kernel
  matches IDs/class against driver tables → loads module if needed (modalias) → driver binds.
  For a flash drive: `usb-storage`/`uas` driver → SCSI disk layer → **block device** `/dev/sdb` (+`sdb1`)
  → kernel emits **uevent** → `udev` creates the device node / symlinks → `udisks2` mounts it
  (mount point `/media/<user>/<label>`) → file manager shows it. Kernel itself did not "show" it.
- Standard **device classes** (mass storage, HID keyboard/mouse, CDC serial, audio, video/UVC) have generic
  drivers: a new device that follows the class spec works with no new driver.
- Truly custom device: (1) follow a class → works; (2) userspace driver via **libusb** / usbfs
  (`/dev/bus/usb/...`) — easiest, usable from Java via usb4java/hid4java, needs a udev rule for permissions;
  (3) write a kernel driver/module (C), match on your VID:PID, expose a `/dev` node or sysfs files.
  Vendor ID must be allocated by USB-IF (hobby: pid.codes / test VIDs).
- Observe: `lsusb`, `lsusb -v -d VID:PID`, `sudo dmesg -w` (while plugging), `udevadm monitor`,
  `ls /sys/bus/usb/devices/`, `lsblk`, `lsmod`, `modinfo usb_storage`.
- Links to later topics: loadable modules (#5), everything is a file (Section 2), udev/uevents (Kernel Internals).

### 4. Distributions (#4) — explained
- Linux = kernel only. A **distribution** = kernel + C library (glibc/musl) + core userland (GNU coreutils,
  bash, systemd/init) + **package manager & repositories** + installer + desktop/config defaults +
  release policy & security updates + community/support.
- Same kernel everywhere; distros differ in packaging, versions, defaults, support lifetime, philosophy.
- Families: Debian (apt/.deb) → Ubuntu → Linux Mint; Red Hat (dnf/.rpm) → Fedora, RHEL, Rocky, Alma;
  Arch (pacman); SUSE (zypper); Alpine (apk, musl + busybox — tiny, common in Docker).
- GJ's machine: Linux Mint 22 "Wilma" ← Ubuntu 24.04 "noble" ← Debian (`/etc/os-release`: ID=linuxmint,
  ID_LIKE="ubuntu debian", UBUNTU_CODENAME=noble); kernel 6.8.0-138-generic (Ubuntu's kernel build);
  glibc 2.39 (Ubuntu); PID 1 = systemd; ~2993 packages installed (`dpkg -l`).
- Why devs care: distro decides glibc version (binary compatibility: build on new glibc ⇒ fails on old
  with `GLIBC_2.xx not found`), package names/paths, default shell/init, Docker base image choice
  (debian-slim vs alpine/musl vs distroless), patched kernel, support window.
- Observe: `cat /etc/os-release`, `uname -r`, `ldd --version`, `dpkg -S /usr/bin/ls` (which package owns a file),
  `apt-cache policy bash`, `ps -p 1 -o comm=`.

### 5. Monolithic kernel and loadable modules (#5) — explained
- **Monolithic kernel**: scheduler, memory manager, filesystems, network stack and drivers all run in ONE
  address space in kernel mode. Pro: fast (internal calls are plain function calls, no messaging between
  components). Con: a bug in any driver can crash the whole kernel (kernel panic).
- **Loadable kernel modules (`.ko`)**: pieces of kernel code loaded/unloaded at runtime without reboot.
  They run in kernel mode with full privileges (not sandboxed, not userspace). So Linux = monolithic *design*,
  modular *packaging*. Kernel stays small; drivers load only when hardware/feature is needed.
- Kernel config per feature: built-in (`=y`), module (`=m`) or off; see `/boot/config-$(uname -r)`.
  Modules live in `/lib/modules/$(uname -r)/` and must match the exact running kernel version.
- Auto-loading: device appears → uevent with a **modalias** (e.g. `usb:v0781p5581…`) → udev runs
  `modprobe` → modprobe uses `modules.alias`/`modules.dep` to find the module and its dependencies →
  `init_module`/`finit_module` syscall loads it. (Ties to the USB discussion.)
- Tools: `lsmod` (= `/proc/modules`), `modinfo <name>`, `sudo modprobe <name>` / `modprobe -r`,
  `sudo insmod file.ko` (no dependency handling), `/sys/module/<name>/`, `dmesg`.
- Dev angles: out-of-tree drivers (NVIDIA proprietary, VirtualBox) must be rebuilt per kernel (DKMS) —
  a kernel update can break them; Secure Boot requires signed modules; containers share the HOST kernel,
  so a container can't load its own module.
- GJ's machine: `/proc/modules` lists loaded modules, e.g. ipt_REJECT, nf_reject_ipv4, nft_ct, xt_mark
  (netfilter/firewall modules, each with a "used by" count).
- Contrast: microkernel (drivers as userspace processes) — covered later in #11.

### 6. Follow-up questions on #5 (Secure Boot, building a .ko, monolithic confusion)
- **Why not rely on Secure Boot alone?** Secure Boot only verifies the *boot chain* (firmware → shim →
  bootloader → kernel signature). It says nothing about what runs afterwards. Root (or an exploit) could still
  load a module = arbitrary code in kernel mode, so Ubuntu/Mint also enforce **kernel lockdown** + **module
  signature checking** when Secure Boot is on. It doesn't stop kernel bugs, userspace malware, or signed-but-
  vulnerable drivers; many people disable it (dual boot, NVIDIA/VirtualBox modules need MOK enrollment).
  Security = layers. Check: `mokutil --sb-state`.
- **Building a .ko:** needs `build-essential` (gcc, make) + `linux-headers-$(uname -r)` (GJ's machine already has
  `/usr/src/linux-headers-6.8.0-138`). Kbuild Makefile `obj-m += hello.o` + `make -C /lib/modules/$(uname -r)/build
  M=$PWD modules`. Example saved in `examples/hello-module/` (hello.c + Makefile). Load: `sudo insmod hello.ko`,
  see `sudo dmesg | tail`, `lsmod | grep hello`, unload `sudo rmmod hello`. Secure Boot ON ⇒ unsigned module
  refused (sign with MOK key or test in a VM). A buggy module can panic the machine — try in a VM first.
- **Monolithic misconception cleared up:** two different boundaries:
  (a) user program ↔ kernel: hardware-enforced, only via syscall gate, every request permission-checked;
  a program never gets "access to the whole device", kernel code runs on its behalf.
  (b) kernel component ↔ kernel component (scheduler, fs, drivers): NO walls in a monolithic kernel — that is
  all "monolithic" means. Every logical CPU can run kernel code simultaneously (like many Java threads
  executing the same ConcurrentHashMap code); kernel data is protected by internal locks (spinlocks, mutexes,
  atomics), each thread has its own kernel stack.
- **Keyboard with two CPUs/programs:** key press → IRQ on one CPU → driver puts event in queue under a lock →
  wakes readers. Normally a *display server* (Xorg/Wayland) reads `/dev/input/event*` and routes keys to the
  focused window only. On a tty, two readers split the bytes (each byte consumed once; background jobs get
  SIGTTIN). On evdev each opener gets its own copy of all events (why keyloggers work ⇒ access is permission-
  controlled, Wayland isolates clients).

### 7. Housekeeping at end of Session 2
- GJ asked to mark #4 and #5 Done → done in `linux_for_developers.xlsx` (backup taken first).
- Created `summaries/init.md`: short summary of topics 1–5 plus all side discussions from Sessions 1–2.
- Next: #6 Kernel versions.

### 8. Kernel versions (#6) — explained
- GJ's kernel string: `6.8.0-138-generic`, `#138-Ubuntu SMP PREEMPT_DYNAMIC Fri Jul 31 22:41:49 UTC 2026`,
  built with gcc 13.3 (`/proc/version`). Anatomy: **6.8.0** = upstream kernel.org version (MAJOR.MINOR.PATCH);
  **-138** = Ubuntu's package/build number (ABI bump); **generic** = Ubuntu flavour (others: lowlatency, aws,
  azure, oem); `#138-Ubuntu` build number; `SMP` = multi-CPU support; `PREEMPT_DYNAMIC` = preemption mode
  chosen at boot.
- Version numbers have **no semantic-versioning meaning**: Linus bumps the major number when the minor gets
  big (3.19→4.0, 4.20→5.0, 5.19→6.0). Don't read "6 → 7" as a breaking change. (`apt` on GJ's machine
  offers an HWE 7.0 kernel candidate.)
- Release rhythm: ~2-week **merge window** → `-rc1…-rc7/8` weekly → release; a new minor (6.8) every ~9–10
  weeks. After release Greg Kroah-Hartman's **stable** team ships 6.8.1, 6.8.2… (bug/security fixes only).
- Trees: **mainline** (Linus), **linux-next** (integration), **stable**, **longterm (LTS)** (kernel.org
  maintains selected ones for years, e.g. 5.15, 6.1, 6.6, 6.12) — don't confuse with distro LTS.
- Distros **backport**: Ubuntu 24.04's "6.8" carries many patches from later kernels, RHEL even more (its
  "4.18"/"5.14" kernels are far newer in features). So the number alone doesn't tell you which features or
  bugs exist. Ubuntu LTS has GA kernel (6.8) and **HWE** kernels (newer, opt-in/rolling for LTS point releases).
- **"Don't break userspace"**: syscall numbers and behaviour are never changed — old binaries keep running on
  new kernels. Internal kernel APIs/module ABI change freely (⇒ modules must match version, see #5).
- Why devs care: feature availability by version (examples: `epoll` 2.6, `pidfd` 5.3, `io_uring` 5.1,
  `openat2` 5.6, cgroup v2 unified hierarchy 4.5+); Docker/Kubernetes/eBPF tools have minimum kernel versions;
  `uname -r` inside a container shows the **host** kernel; glibc/JVM check kernel features at runtime
  (JVM container awareness uses cgroups); kernel updates need reboot + may break DKMS modules.
- Observe: `uname -r`, `uname -a`, `cat /proc/version`, `dpkg -l 'linux-image-*'`, `ls /boot`, Mint Update
  Manager → View → Linux kernels; GRUB "Advanced options" to boot an older kernel as fallback.
- GJ's machine: only linux-image 6.8.0-138 installed (+ `vmlinuz.old`); leftover headers for 5.15.0-91/-190
  in `/usr/src` from old kernels.

### 9. Side question: what is a modalias string?
- **modalias** = a one-line string that encodes a device's identity (bus type + IDs + class), built by the
  kernel per device; visible as `/sys/bus/<bus>/devices/<dev>/modalias` and in the uevent as `MODALIAS=`.
- Each module declares the patterns it supports (`MODULE_DEVICE_TABLE`); `depmod` collects them into
  `/lib/modules/$(uname -r)/modules.alias` (35,181 lines on GJ's machine; `usb_storage` alone has 418 aliases).
  Patterns use `*` wildcards. `modprobe <modalias>` matches the string against these patterns and loads
  the module(s) (+ dependencies).
- USB format: `usb:v<vendor>p<product>d<bcdDevice>dc<devClass>dsc<sub>dp<proto>ic<ifClass>isc<sub>ip<proto>in<ifNum>`.
  PCI format: `pci:v<vendor>d<device>sv<subvendor>sd<subdevice>bc<baseclass>sc<subclass>i<progif>`.
- GJ's real examples: `usb:v1D6Bp0002…dc09` = Linux Foundation 2.0 root hub (class 09 = hub);
  `usb:v413Cp2514…ic03isc01ip01` = HID keyboard interface (ic03 = HID, ip01 = keyboard, ip02 = mouse);
  `usb:v5986p212B…ic0E` = webcam (ic0E = video class); PCI `pci:v00001022d00001630sv000017AAsd00003820bc06sc00i00`
  = AMD host bridge on a Lenovo (sv 17AA) board.
- Verified with `modprobe -R <modalias>` (resolve only, loads nothing): mass-storage alias → `uas`,
  `usb_storage`; keyboard interface → `usbhid` (+ `usbkbd`); webcam → `uvcvideo`.
- Observe: `cat /sys/bus/usb/devices/*/modalias`, `modinfo -F alias usb_storage`, `modprobe -R '<modalias>'`,
  `udevadm monitor --property` (shows MODALIAS on plug-in).
