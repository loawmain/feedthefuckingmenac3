# design notes

how the thing actually works, and where it doesn't. no marketing, just the mechanics.

## decode model

bytecode is stored encrypted. every byte is xor'd with a keystream byte derived from
`(build_seed, byte_offset)`:

```
ksb(seed, pos) = high8( splitmix64( seed ^ pos*GOLDEN ) )
```

crucially the key is a function of **position**, not of execution history. an early attempt
fed the running instruction digest into the key — that breaks the instant control flow
merges: a point reached through two different paths gets two different key states and one
of them decodes to garbage. position/seed keying is path-independent, so branches and
loops just work.

the opcode byte carries its own per-position permutation on top: the plaintext opcode is
`(op + dselof(seed,pos)) & 0x3F`, so the same op is a different byte at every site even
before the keystream xor.

## register file

registers aren't stored as their logical values. each slot holds `value ^ maskof(mseed,i)`,
where `mseed` is derived from the seed and `maskof` is a murmur-style finalizer over the
index. there is no point at which a clean 16-qword register array exists in memory to dump.
`rget`/`rset` mask/unmask on the fly.

## dispatch

the handler table is not indexed by opcode directly. at startup the vm derives an affine
map `slot(op) = (A*op + B) mod S`, where `S` is the table size and `A` is forced coprime to
`S` (so the map is a bijection — every op lands in a unique slot, every slot filled). `A,B`
come from the seed. handlers are also stored xor'd with a pointer mask. net effect: dumping
the table gives you `S` masked pointers in an order that has nothing to do with opcode
numbering.

on top of that, arithmetic ops have **multiple handler implementations** and the seed picks
which one this build uses:

```
add : (x^y) + ((x&y)<<1)      or   (x|y) + (x&y)
sub : x + ~y + 1              or   (x^y) - 2*(~x & y)
xor : (x|y) & ~(x&y)          or   (x|y) - (x&y)
and : (x|y) - (x^y)           or   x + y - (x|y)
or  : (x&y) + (x^y)           or   (x+y) - (x&y)
```

all exact in Z/2^64. so "the add handler" isn't a fixed code signature across builds.

## poly engine

`asm_polymorph` walks the source program and, in the gaps between real instructions,
inserts:

- **junk** on registers reserved as scratch (`r8..r15` by convention), never touching the
  program's own registers or the flags
- **opaque predicates** whose "always taken" branch rests on an MBA parity identity
  (`x*(x+1)` and `2x` are always even), then a dead junk block behind the branch. only
  emitted where the zero-flag is dead — a small liveness tracker guards the `cmp -> jz`
  window so a predicate never clobbers a real comparison
- **traps** — `push r0; vmcall(guard); mov s,r0; pop r0; xor poison,s`. r0 is preserved;
  the guard returns 0 clean and nonzero under a debugger, so `poison` is untouched when
  clean and corrupted when watched. no control-flow tell
- **integrity checks** on the main path

register discipline is the subtle correctness constraint: the program (including the
expression compiler's temporaries) lives in `r0..r7`, the poly engine owns `r8..r15`. if
those overlap, inserted junk silently overwrites a live temporary and the output stops
depending on the input — found and fixed exactly that during bring-up.

## packaging

blob layout: `[u32 magic][u64 obf_seed][u32 len][code...]`.

```
obf_seed = code_seed ^ splitmix64(pack_key ^ magic ^ len) ^ hash(code)
```

the seed is bound to a compiled-in `pack_key` and to the code hash, so it never sits in the
clear and any edit to the code changes the recovered seed. the loader then does
`seed ^= guard()` before running — clean guard is 0 (no change), a debugger makes it nonzero
and the entire bytecode decodes to noise. no "if debugged" branch exists to remove.

## the keygate

standard key checks end in a compare and a conditional jump; that jump is the whole attack
surface. the keygate has neither.

```
k        = xform(input)                 # cheap invertible pre-mix
CHKX k                                   # fold integrity into k (no-op when intact)
stream   = splitmix64(k)                 # full-avalanche, run inside the vm
out      = cipher ^ stream               # cipher = payload ^ splitmix64(expected)
emit out
```

when `input` is correct, `k == expected`, `stream == splitmix64(expected)`, and `out` is the
real payload. one bit wrong anywhere in the input and splitmix64 avalanches it — the output
looks random, with no partial-credit gradient a bruteforcer could climb.

- **CHKX** folds `hash(code_prefix) ^ expected_hash` into `k`. intact → xor 0. tampered →
  `k` corrupted → garbage out, and unlike the old halting CHK there is no status code or
  early exit announcing it
- the trap poison target is `k` as well, so a debugger corrupts the same key

there is no compare and no conditional branch anywhere in the gate. a patch attack has
nothing to flip; the only way through is to actually produce the correct input.

note the honest caveat: `xform` and `splitmix64` are bijective, so a known-payload attacker
could in principle invert back toward the input. that's an algorithm choice, not a structural
limit — swap the pre-mix for a one-way, iterated KDF and even a known payload doesn't help.
the *structure* (no patchable check, checks-as-data) is what this layer delivers.

## threat model

what raises cost against the realistic attacker:

- **static / automated:** no static opcode table, masked registers, per-build polymorphism
  and table permutation, seed bound to key+integrity. there's no obvious foothold and no two
  builds match, so signatures and canned deobfuscators don't bite
- **patching:** the keygate and the folded checks mean there's no branch to flip and no
  detection routine to nop. tamper anywhere → garbage, silently

where it still falls, honestly:

- **dynamic tracing.** defeat the anti-debug (or run in an instrumented emulator) and you can
  log the dispatch loop and recover the decoded instruction stream. this is the classic weak
  point of every single-loop vm obfuscator, and it's what the roadmap targets

## roadmap

- **layer 2:** duplicate / thread the dispatch so no single breakpoint captures the stream;
  decrypt bytecode regions on demand and re-seal them, so a dump only ever shows a slice
- **layer 3:** nested vm — handlers become inner-vm bytecode, doubling the lifting work
