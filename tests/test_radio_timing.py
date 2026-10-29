#!/usr/bin/env python3
"""Conservative PIC18 CFG bound from Timer3 flag clear to the next poll.

Every instruction in this routine is one word. Conditional branches/skips take
both alternatives, deliberately ignoring infeasible flag combinations. The
only allowed cycle is the outer sample loop through the Timer3 wait. An inner
loop (e.g. a per-bit shift loop) fails the audit even if bounded at runtime.
"""
from functools import lru_cache
from pathlib import Path

ops, labels = [], {}
for line in Path("src/radio_capture.S").read_text().splitlines():
    line = line.strip()
    if not line or line.startswith(("/", "*", "#", "GLOBAL", "PSECT")):
        continue
    if line.endswith(":"):
        labels[line[:-1]] = len(ops)
    else:
        ops.append(line.split(None, 1))
start = ops.index(["bcf", "PIR2,1,c"]) + 1
stop = labels["capture_wait"]
visiting = set()

@lru_cache(None)
def longest(index):
    if index == stop:
        return 0
    assert index not in visiting, "inner loop makes the sample timing variable"
    visiting.add(index)
    instruction = ops[index]
    opcode = instruction[0]
    argument = instruction[1] if len(instruction) > 1 else ""
    if opcode == "return":
        result = -10000  # Finished capture does not need another timed sample.
    else:
        if opcode == "bra":
            edges = [(labels[argument], 2)]
        elif opcode in ("bz", "bnz", "bc", "bnc"):
            edges = [(labels[argument], 2), (index + 1, 1)]
        elif opcode in ("btfss", "btfsc", "tstfsz"):
            edges = [(index + 2, 2), (index + 1, 1)]
        else:
            assert opcode in ("movlb", "movlw", "subwf", "subwfb", "setf",
                "movwf", "bcf", "incf", "addwfc", "movf", "addwf", "bsf",
                "andwf", "xorlw", "clrf", "decf", "rrcf", "iorwf"), opcode
            edges = [(index + 1, 1)]
        result = max(cost + longest(target) for target, cost in edges)
    visiting.remove(index)
    return result

cycles = longest(start)
# TMR3L load precedes flag clear by one cycle. Reserve extra polling/reload
# overhead rather than accepting a body that just fits the 96-cycle timer.
assert cycles + 8 < 96, (cycles, "Timer3 sample processing overruns")
print(f"radio timing: every sample path reaches Timer3 poll within {cycles} cycles; no inner loops")
