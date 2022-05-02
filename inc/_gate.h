/*
 * Serialization functions (spin locks)
 *
 * notes:
 *   https://barrgroup.com/Embedded-Systems/How-To/C-Volatile-Keyword
 */

#if MAX_CPUS > 1

// Two spin-lock gating mechanisms are implemented depending on
// which set of macros are used.  One is a single producer /
// multi-consumer lock (SPMC), the other a multi-producer /
// multi-consumer lock (MPMC).  spin-lock performance optimizations
// are included.
//
// Both use an Array Based Queuing Lock model where each thread
// spins on its own array element.
// https://wikipedia.org/wiki/Array_Based_Queuing_Locks
// Note that these fifo style locks only work when consumers
// cannot pause/suspend for other reasons (like when in a
// virtual machine) which can starve other comsumers or deadlock.
//
// The single producer / multi-consumer queue is based on the
// 'take a ticket' system found in stores/post-offices where
// people wait in a first-come first-served order.
// https://wikipedia.org/wiki/Ticket_lock
//
// Performance is achieved by reducing the inter-processor
// cache coherency traffic.  By polling on a location in the
// queue distant enough from other cpus, cache snoop or brodcast
// cycles can be avoided.  This distance may need to be an
// entire cache line (instead of a u64), but if an architecture
// has a 'monitor' opcode with sufficient address granularity
// the u64 may even be replaced with a u8.
//
// NOTE: pretty sure you don't want to declare the gate on a stack :)

#define QUEUE_SIZE_MASK(name)       (sizeof(name##_gate.queue)/sizeof(name##_gate.queue[0]) - 1)

// single producer / multi-consumer
// take turns with a single ticket (ie. a token)
//
// the queue starts with a single initial non-zero value (the token).
// arrival order is maintained.  here gate->end 'points' to the
// next ticket to be taken, ie. one past the end of the queue.
// note that gate->end is *not* a queue index, (gate->end & mask) is.

#define SPMC_GATE_INIT(name, size)  \
    struct name##_gate {            \
        volatile u64 queue[size];   \
        volatile long end;          \
    } name##_gate = {{1}, 0};


#define wait_for_token(name, t) {                                 \
    t = sync_fetch_inc(name##_gate.end) & QUEUE_SIZE_MASK(name);  \
    while (name##_gate.queue[t] == 0) pause(name##_gate);         \
    name##_gate.queue[t] = 0;                                     \
    (t)++;                                                        \
}

#define return_token(name, t)  {                             \
    name##_gate.queue[(t) & QUEUE_SIZE_MASK(name)] = 1;     \
    resume(name##_gate);                                    \
}

#define debug_spmc_gate(name) {                             \
    printf("%s end(%d)\n", #name, name##_gate.end);         \
    for (int k = 0; k <= QUEUE_SIZE_MASK(name); k++)        \
        printf("%s q(%lx)\n", #name, name##_gate.queue[k]); \
}


// multi-producer / multi-consumer
// message passing queue
//
// there are multiple consumers [cpus waiting], but nothing initially
// in the queue.  when a 64bit (non-zero) value is placed in the
// queue (by a producer at slot = gate->start & mask), the consumer
// polling that slot, picks up the value, zeros the queue slot, and
// advances gate->start.  new consumers get in line at gate->end & mask.
// gate->len might over report the count, but it will never under report.

#define MPMC_GATE_INIT(name, size)  \
    struct name##_gate {            \
        volatile u64 queue[size];   \
        volatile int end;           \
        volatile int start;         \
        volatile int len;           \
    } name##_gate = {{0}, 0, 0, 0};

#define mpmc_gate_count(name)       (name##_gate.len)

#define remove_item_wwait(name, item, type) {                               \
    int idx = atomic_fetch_inc(name##_gate.start) & QUEUE_SIZE_MASK(name);  \
    while (name##_gate.queue[idx] == 0) pause(name##_gate);                 \
    item = (type)name##_gate.queue[idx];                                    \
    name##_gate.queue[idx] = 0;                                             \
    atomic_dec_fetch(name##_gate.len);                                      \
}

#if 0
#define add_item(name, item) {                                              \
    atomic_fetch_inc(name##_gate.len);                                      \
    int idx = atomic_fetch_inc(name##_gate.end) & QUEUE_SIZE_MASK(name);    \
    if (name##_gate.queue[idx] == 0) {                                      \
        name##_gate.queue[idx] = (u64)(item);                               \
    } else {                                                                \
        atomic_dec_fetch(name##_gate.end);                                  \
        atomic_dec_fetch(name##_gate.len);                                  \
    }                                                                       \
}
#endif

// add_item must not be used when len is near full
// (suggestion: use water marks)
#define add_item(name, item) {                                              \
    atomic_fetch_inc(name##_gate.len);                                      \
    int idx = atomic_fetch_inc(name##_gate.end) & QUEUE_SIZE_MASK(name);    \
    name##_gate.queue[idx] = (u64)(item);                                   \
    resume(name##_gate);                                                    \
}

#define debug_mpmc_gate(name, fn) {                                         \
    u32 mask = QUEUE_SIZE_MASK(name);                                       \
    printf("%s 0x%lx len(%d) start(%d,%d) end(%d,%d)\n", #name,             \
           &name##_gate,      name##_gate.len,                              \
           name##_gate.start, name##_gate.start & mask,                     \
           name##_gate.end,   name##_gate.end   & mask);                    \
    for (int k = 0; k <= mask; k++)                                         \
        printf("%s slot(%d,%lx) %s\n", #name, k, name##_gate.queue[k],      \
                                       fn(name##_gate.queue[k]));           \
}

//    hexdump((void *)&name##_gate, sizeof(name##_gate), 0);

#else /* MAX_CPUS > 1 */

#define QUEUE_SIZE_MASK(name)
#define SPMC_GATE_INIT(name, size)
#define wait_for_token(name, t)
#define return_token(name, t)
#define debug_spmc_gate(name)
#define MPMC_GATE_INIT(name, size)
#define mpmc_gate_count(name)
#define remove_item_wwait(name, item, type)
#define add_item(name, item)
#define add_item(name, item)
#define debug_mpmc_gate(name, fn)

#endif /* MAX_CPUS > 1 */
