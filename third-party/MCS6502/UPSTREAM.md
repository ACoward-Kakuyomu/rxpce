# Source

Upstream: https://github.com/bzotto/MCS6502

Commit: a189879a7a21777213f6e3e22b340d2aac8d2dd6

LICENSE.TXT is the upstream MIT license with a binary-distribution attribution
requirement. Normal builds do not access the network.

Local changes to MCS6502.c / MCS6502.h:

* Add per-context directPageBase / stackPageBase. Init defaults to $0000 / $0100;
  Reset preserves the bases. Direct-data and stack accesses still use callbacks.
* Wrap indirect data pointers within the direct page at $ff, for both ($zz,X)
  and ($zz),Y. This also fixes the corresponding upstream NMOS behavior.
* Compute ($zz),Y page-cross timing from the dereferenced 16-bit address instead
  of comparing the pointer's 8-bit offset with the final address.

These changes alone do not make this NMOS core HuC6280-compatible. Instruction
extensions, status semantics, vectors and HuC timing require a separate adapter.
The original license and README are unchanged.

Original SHA-256:

* MCS6502.c: 626C7DC7171EBCA3D6D92E458A219566BFCCBC23EFFC60934CEE4FD1DFF63B67
* MCS6502.h: CC38AB07C1AE8944922B4E0CF6C1D071CF6F52CAA73FA35E140D9B00A2421C35
* LICENSE.TXT: 5A2F0D099861A603614EE5499C2D890F90F8B1817BFBBACB8FDAC713769763F7
