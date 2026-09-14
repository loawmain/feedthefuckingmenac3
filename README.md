# feedthefuckingmenac3

a polymorphic, vm-based code protector for windows/x64. you feed it a routine, it hands
back something a reverser really doesn't want to sit through.

this is anti-tamper / anti-RE **research** — same family as vmprotect / themida, written
from scratch in c to actually learn what makes protection expensive to break and what
makes it fall over. it's defensive tooling: protection you put on your *own* code.

> **status: work in progress.** the core vm + polymorph + packaging + a patch-proof key
> gate are up and validated end to end. the heavier hardening layers are landing one at a
> time — see the roadmap at the bottom. to be continued.

## what it does

takes a routine, lowers it to bytecode for a custom register vm, and per build:

- re-encrypts the bytecode with a position/seed-derived keystream — there is no static
  opcode table sitting in memory for someone to find
- runs a polymorphic pass so no two builds look the same
- seals bytecode + seed into one blob and wraps it so a debugger *silently corrupts the
  result* instead of tripping an obvious "you're being watched" branch

## the vm

- 16-register machine, function-pointer dispatch, ~30 ops
- **position/seed keystream** — every byte decodes from its own offset plus the build
  seed, not from execution history. branches reaching the same point down different paths
  still decode identically (history-fed keys break exactly here — learned that the hard way)
- **masked register file** — registers are stored xor'd with a runtime-derived mask; no
  clean register array to dump out of memory
- **MBA arithmetic** — add/sub/xor/and/or as mixed boolean-arithmetic identities, exact
  mod 2^64
- **handler variants** — each arithmetic op has more than one implementation, chosen per
  build from the seed. the same opcode lowers to different machine code across builds
- **permuted dispatch** — the handler table is placed through a seed-derived affine
  permutation (a bijection mod the table size). dumping the table doesn't tell you which
  slot is which opcode

## poly engine

everything below is emitted into a register range disjoint from the protected program, so
it can't clobber real values:

- junk on dead scratch registers
- opaque predicates built on MBA identities (`x*(x+1)` is always even, `2x` is always
  even, ...) so the "always true" isn't staring the analyst in the face
- silent traps — the anti-debug result folds into a live register: zero when clean,
  garbage when watched. no halt, no message, no tell
- distributed integrity checks (CHK / CHKX) scattered through the main path

## packaging

- bytecode + seed go into a single blob
- the seed is **never stored in the clear**: `obf = code_seed ^ f(pack_key) ^ hash(code)`.
  it's bound to a compiled-in key *and* to code integrity at once
- the loader folds the anti-debug result into the recovered seed. run it under a debugger
  and the whole thing decodes to noise. there is no branch to patch out

## the keygate — patch-proof checks

the part worth reading. a license/key check normally ends in `cmp; jz ok` — one
conditional a cracker flips and it's game over. this one doesn't have that.

- the correct input **is** the key. it derives a keystream (splitmix64 run *inside the
  vm*, full avalanche) and decrypts the payload with it
- right input → real payload. wrong input → fully avalanched garbage, no resemblance to
  the answer, nothing to gradient toward
- there is **no compare and no conditional jump anywhere in the gate**
- integrity (CHKX) and anti-debug fold into that same key as plain arithmetic. tamper a
  byte → wrong key → garbage. there's no "integrity failed" for someone to find and nop

patch attacks die here because there's nothing to patch. you'd have to actually solve for
the input.

## build

msvc / x64. no gcc/clang, no cmake, no dependencies.

```
build.bat
```

it locates visual studio via vswhere, calls vcvars64, and builds the demos + the cli:

- `ftm_demo.exe`  — hand-written key-check, plain + polymorphed + packed
- `ftm_lift.exe`  — the expression compiler lifting a routine into the vm, then poly+pack
- `ftm_gate.exe`  — the patch-proof keygate
- `ftmc.exe`      — the cli

## usage — ftmc

protect an expression into a blob, then run it:

```
ftmc build "(rol(x,7) ^ 0xDEADBEEFCAFEBABE) + 0x0123456789ABCDEF" --key 0xBBBB --out f.ftmb
ftmc run f.ftmb --key 0xBBBB --in 0x1234ABCDF00D
```

`build` flags: `--var name=reg  --dst R  --seed HEX  --key HEX  --junk N  --opaque N
--traps N  --chk N  --scratch LO  --raw`. the expression compiler does register
allocation and constant folding, supports `| ^ & << >> + - *`, unary `~ -`, parens,
hex/dec literals, input variables, and `rol()/ror()`.

## honest scope

this is obfuscation, not a cryptographic wall. everything needed to run is present in the
binary — a determined analyst with a debugger and enough time can dynamically trace it.
what this buys you is **cost**: the trace is polymorphic, table-shuffled, register-masked,
trap-laced, and there is no cheap patch anywhere. the point is to make it not worth
somebody's afternoon.

what it is **not**: a spoofer, a bypass, or anything for touching software that isn't
yours. don't.

## roadmap

- **layer 2** — thread/duplicate the dispatch so there's no single breakpoint that catches
  the whole instruction stream; decrypt bytecode regions on demand and wipe them after, so
  a memory dump never shows the whole program
- **layer 3** — nested vm: the handlers themselves become bytecode for an inner vm

## license

MIT — see [LICENSE](LICENSE).
