
// pull in every .h file for every source file.
// compilers now days can handle it, and then they can tell
// you about namespace collisions before the link phase.
// a leading '_' in a .h file name means you need to use uos.h
// to include it - anything without a leading '_ is as-needed.

#ifndef _TYPES_H
#   include "_types.h"
#define _TYPES_H 1
#endif

#ifndef _SMP_H
#   include "_smp.h"
#define _SMP_H 1
#endif

#ifndef _TASK_H
#   include "_task.h"
#define _TASK_H 1
#endif

// uos source is never compiled outside of an arch+mach context.
// there needs to always be an arch/xxxx/arch.h and a
// arch/xxxx/mach/yyyy/mach.h

#ifndef _MACH_H
#   include "mach.h"
#define _MACH_H
#endif

#ifndef _ARCH_H
#   include "arch.h"
#define _ARCH_H
#endif

#ifndef _GATE_H
#   include "_gate.h"
#define _GATE_H 1
#endif

#ifndef _LIBBITMAP_H
#   include "_bitmap.h"
#define _LIBBITMAP_H 1
#endif

#ifndef _LIBC_H
#   include "_c.h"
#define _LIBC_H
#endif

#ifndef _LIBMEM_H
#   include "_mem.h"
#define _LIBMEM_H
#endif

