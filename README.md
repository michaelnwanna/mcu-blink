# mcu-blink

> The **"Hello, World!" of bare-metal microcontrollers** — blink an LED on an
> Arduino board with **no Arduino, no library, no magic**. Just two hardware
> registers and modern C++.

[![CI](https://github.com/michaelnwanna/mcu-blink/actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![AVR](https://img.shields.io/badge/target-AVR%20(avr--gcc)-orange)
![License: MIT](https://img.shields.io/badge/License-MIT-green)

This is the **first step** into bare-metal microcontroller programming. If
you've ever run the Arduino `Blink` sketch, this repo shows you what that sketch
*actually does* underneath — by doing the exact same thing with the chip's raw
registers and nothing else.

> **Already write embedded C?** You'll feel right at home — this is the same
> `DDRB |= (1 << PB5)` register poking you already know, just in tidy,
> zero-overhead C++. Jump to [**Coming from C?**](#coming-from-c-) for the
> one-table Rosetta Stone.

The entire program is one short, heavily-commented file:
**[`blink.cpp`](blink.cpp)** — and it builds with **one `avr-g++` command**, no
build system to learn first.

## What "bare-metal" means

Your code talks to the chip **directly**. There's no Arduino framework, no
operating system, and no vendor library in between — you write to the chip's
**registers** (special memory addresses wired to real hardware) using values
from the chip's datasheet. That's it.

## The whole idea, in 4 lines

Blinking pin 13's LED on an ATmega328P (Arduino Uno / Nano) comes down to this:

| Arduino sketch            | What it really is        | Bare-metal (this repo)         |
| ------------------------- | ------------------------ | ------------------------------ |
| `pinMode(13, OUTPUT);`    | mark the pin an output   | set bit 5 of register **DDRB** |
| `digitalWrite(13, HIGH);` | drive the pin high       | set bit 5 of register **PORTB**|
| `digitalWrite(13, LOW);`  | drive the pin low        | clear bit 5 of **PORTB**       |
| `delay(500);`             | wait                     | burn some CPU cycles in a loop |

Pin 13 on those boards is **port B, bit 5 (PB5)** on the chip — that's the only
"secret" the datasheet tells us. Everything else is just setting and clearing
that one bit.

## Coming from C? 🦾

If you've written AVR C with `<avr/io.h>`, there is **nothing new to learn** —
it's the identical hardware dance, just in C++. Here's the whole translation:

| Classic AVR C (`<avr/io.h>`)   | This repo                  | What happens         |
| ------------------------------ | -------------------------- | -------------------- |
| `DDRB  \|= (1 << PB5);`         | `setBit(DDRB,  kLedBit);`   | PB5 → output         |
| `PORTB \|= (1 << PB5);`         | `setBit(PORTB, kLedBit);`   | LED on  (drive high) |
| `PORTB &= ~(1 << PB5);`         | `clearBit(PORTB, kLedBit);` | LED off (drive low)  |
| `(*(volatile uint8_t*)0x25)`   | `reg(0x25)`                 | raw register access  |
| `while (1) { ... }`            | `for (;;) { ... }`          | the forever loop     |

The only two things that are *different* from your usual C file:

- **No `<avr/io.h>`.** We spell out the register addresses (`0x24`, `0x25`)
  ourselves so nothing is hidden behind a vendor macro — same numbers, straight
  from the datasheet.
- **`inline` functions instead of macros.** `setBit` / `clearBit` are tiny
  `inline` helpers, so the compiler **type-checks** them — but they still emit
  the exact same `SBI` / `CBI` instructions as your `|=` / `&= ~` one-liners.
  **Zero overhead, zero magic.**

If `PORTB |= (1 << PB5)` is muscle memory for you, you already understand this
entire repo. 🎯

## What you need

| | |
| --- | --- |
| **Board** | Any ATmega328P board: Arduino **Uno** or **Nano** (or a bare chip at 16 MHz). |
| **Toolchain** | `avr-g++` + `avr-objcopy` + `avr-size` (the AVR GNU toolchain). |
| **Flashing** | `avrdude` (already included with the Arduino IDE). |

No CMake, no Makefile, no Ninja — just the compiler and the flasher. That's the
whole point: nothing between you and the chip.

> No hardware handy? You don't need any to build it — CI compiles the firmware
> on every push. Grab the `.hex` from the run's artifacts and flash it later.

## Build it

One command compiles the firmware; one more turns it into a flashable image:

```bash
# 1. Compile to an ELF
avr-g++ -mmcu=atmega328p -Os -std=c++17 -Wall -Wextra -o blink.elf blink.cpp

# 2. Convert to a flashable Intel-HEX image
avr-objcopy -O ihex -R .eeprom blink.elf blink.hex

# 3. (optional) See how tiny it is
avr-size -B blink.elf
```

That's it — `blink.hex` is your firmware. (`-mmcu=atmega328p` picks the chip;
change it to target a different AVR.)

## Flash it

With an Arduino Uno on, say, `COM3` (Windows) or `/dev/ttyACM0` (Linux):

```bash
avrdude -c arduino -p m328p -P COM3 -b 115200 -U flash:w:blink.hex:i
```

The on-board LED should start blinking about once a second. That's bare-metal
firmware *you wrote*, running on the chip with nothing else underneath.

## Read it

Open **[`blink.cpp`](blink.cpp)** — every line is commented, and each
register write is shown next to the Arduino call it replaces. If you follow it
all the way through, you already understand the core idea behind *all* embedded
programming: read the datasheet, set the right bits.

## Next step 👉

Ready for the real thing? The flagship repo builds on exactly this foundation —
a portable, allocation-free **matrix-keypad driver** running as true bare-metal
firmware on **two different architectures** (8-bit AVR *and* 32-bit ARM
Cortex-M), with a non-blocking debounce state machine, host unit tests, and CI:

➡️ **[mcu-baremetal](https://github.com/michaelnwanna/mcu-baremetal)**

It picks up where this blink leaves off: precise timing with hardware timers and
interrupts, a clean hardware-abstraction seam, and the same no-vendor-library
discipline at a larger scale.

## License

MIT — see [`LICENSE`](LICENSE).
