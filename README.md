# chip-8-emulator
A Chip8 interpreter and emulator written in C using SDL3

# Features


# Tests and Validations
[Chip-8 Test Suite by Timendus](https://github.com/Timendus/chip8-test-suite)
<details>
    <summary> Click to show </summary>

1. ✅ [1-chip8-logo.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/1-chip8-logo.ch8)
2. ✅ [2-ibm-logo.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/2-ibm-logo.ch8)
3. ✅ [3-corax+.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/3-corax%2B.ch8)
4. ✅ [4-flags.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/4-flags.ch8)
5. ⚠️ [5-quirks.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/5-quirks.ch8)
    * Not all quirks are implemented
6. ✅ [6-keypad.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/6-keypad.ch8)
7. ⚠️ [7-beep.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/7-beep.ch8)
    * Sound is produced, but randomly jumps. Likely due to improper square wave generation.
8. ❌ [8-scrolling.ch8](https://github.com/Timendus/chip8-test-suite/blob/main/bin/8-scrolling.ch8)
    * Not implemented

</details>


## Sources:
* https://en.wikipedia.org/wiki/CHIP-8
* https://austinmorlan.com/posts/chip8_emulator
* https://github.com/Timendus/chip8-test-suite
* https://tobiasvl.github.io/blog/write-a-chip-8-emulator