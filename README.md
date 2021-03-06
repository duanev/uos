# &mu;os

### Micro OS

Big operating systems try to use a single source base for all supported
architectures and machines, with reams of `#ifdef` statements,
making them difficult to understand, debug, maintain, and use.  **&mu;os**
tries (yet another) minimal approach, favoring to entertain a few duplicate
files which tend to be architecture or machine specific. Assembly files
and inline-asm are found in the arch/\* and arch/\*/mach/\* directories,
where there may be some duplication across architectures or across machines:
instead of forcing a source file to compile on multiple archs and machs,
we let the code for each to be clear and well optimized for that arch+mach
pair.

The goal is not to be pedantic about re-use to the point of making it
un-readable, and we tolerate patching the same bug/upgrade in several
places.

### Quick start

On a Linux system with QEMU installed, checkout the &mu;os repo, cd to it,
then type `./umake` and it will show you what works (green), what has
issues (yellow), and what is broken (red).  Pick an architecture (the QEMU
machine is a good choice ;) and an app:

    ./umake armv8 qemu hello
    cd bulid-armv8-qemu-hello

and run the qemu command offered at the bottom. Then maybe edit
`../apps/hello/main.c` or something and run `../umake` to try your
change.

For now &mu;os is allways a static monolithic image.  [^1]

### Features

- **SMP** cores can be launched and given a thread via `smp_start_thread()`.
  See `appd/testsmp/main.c`.  An ARMv8 hypervisor or secure monitor is
  needed (QEMU has one builtin).  Can someone please point me to an HVC
  or SMC for rpi400's (compute module 4) start4.elf?

- No interrupts. Just use another core (you will like the improved latency).

- **Multitasking** is cooperative, *not preemptive* (no interrupts),
  and there are likely still some bugs here despite the much more invloved
  tests.  Still a single binary, so task code is all linked with &mu;os and
  kicked off from `main()`.  A true general purpose multi-*application*
  system with loadable binaries is not yet in the cards ...

- Pretty near zero **device** support. Console i/o is there of course, but
  opportunities abound.  ;)

- There is no **ABI** in a monolithic kernel, (no system calls) only an
  application programming interface.

- The **MMU** is not yet turned on so it's a flat logical = physical address
  space, *without any protection* between tasks. Your code is disciplined,
  right? Keep it lean and straight forward and it won't be too hard.

- **Memory management**, if needed, is a simple slab-like arrangement: a
  limited number of user selected power-of-2 sizes, and quantity of 'pages'
  of that size.  You configure it all. It is very fast, cannot fragment,
  and always aligned, but yes it does waste space. However when you have
  megs or yeah gigs of RAM, _time-efficient_ heap management is usually
  better than space-efficiency.

- **Multiacritecture** is supported ... but only a few for the moment.
  And features are not present in all architectures - some are ahead of
  others. For a new architecture a QEMU version tends to be the first
  machine implemented here ... because it is way easier to debug. [^2]

- `#define` and `#ifdef` use is *minimal*.  No layers upon layers of macro
  definitions, or an impossible to browse code configuration system.

- _NONE_ of the makefiles are insane.

### Build

I prefer the vpath approach to multi-arch build environments documented two
decades ago by
[Paul Smith](http://make.mad-scientist.net/papers/multi-architecture-builds).
From a build-\* directory, the following uses GNU make directly to build the
&mu;os hello app for armv8 to run in QEMU:

    $ make APP=hello -f ../arch/armv8/mach/qemu/mk hello

A GNU make issue (the _$MAKECMDGOAL_ variable cannot be used in the same way
as a command line variable), makes the above messy, and we want some extra
features (build directory creation, tab completion) anyway, so the `./umake`
script keeps build syntax straight forward:

    $ source umake.bash
    $ ./umake armv8 qemu hello

You can run it in the &mu;os root to create build directories, and then
you can run it from the build directories with `../umake` (no args).

### Philosophy

Software should be (1) reliable, (2) easy to understand, use, and fix,
(3) secure (from external influence), and *then* (4) lean and efficient.

Complexity degrades all the above.  The Linux developers have done an
amazing job: despite Linux's popularity and everyone requesting support
for everything, they've kept it relatively easy to build and use.  Maybe
it takes an army to manage complexity ... or maybe we just need an even
more critical attitude toward unnecessary complexity.

Rewrite it all, learn from the past, only bring what we really need.

### Notes/Bugs

- QEMU -smp: Currently `MAX_CPUS` needs to be changed in the
  apps/\*/mkvars file in addition to changing the -smp value.

- The multitasking code is fragile, you likely want to cut-n-paste from
  testtask/test.c, or hey, just use a core.

- And did it seem like `volatile` never really did anything? Well here
  it does! And finding a missing one can be a real test.

---

[^1] Eventually I want the entire OS and all shared libraries to be
*runtime up-gradable* ... yes, that's right, *swap* the kernel or any
library out from underneath a running system ... but that will take
some thought.

[^2] QEMU debug is pretty nice with one of my other projects: **pgdb**.

- `mkdir ~/git`
- `pushd ~/git`
- `git clone https://github.com/duanev/pgdb.git --branch v0 --single-branch`
- `popd`
- `./umake armv8 qemu hello`
- `cd build-armv8-qemu-hello`
- `qemu-system-aarch64 ..(use the cmdline shown by umake)..` **-s -S**
- (qemu will wait for a remote debugger, so in another BIG terminal type:)
- `~/git/pgdb/pgdb.py -gccmap hello.map`
- (press 'h' and 'l' to close the help and log windows)
- (press 'J' to run all cpus and skip to the hilighted-in-white address - in this case skipping the qemu boot loader)
- (press 's' to single-step the active cpu, or 'S' for all of them)
- (when done press 'Q' to exit both pgdb and qemu)
- (if qemu is left running: ctrl-a,c will switch to the qemu monitor and 'quit' will exit)

I'm noticing that lowercase 'j' is pretty spotty when walking through
multi-task apps (can hang easily).  Uppercase 'J' appears to work more
frequently (lets all the cpus run until one hits the breakpoint).  To get
back to your cpu of choice, tab through the cpu windows and press 'enter'
to change cpu contexts.

---
_Markdown to HTML:_
`python -m markdown README.md > /tmp/md.html`
