// SPDX-License-Identifier: MIT
//
// blink.cpp — the "Hello, World!" of bare-metal microcontrollers.
//
// Blinks the on-board LED of an Arduino Uno / Nano (ATmega328P) with NO Arduino
// and NO library at all — just two hardware registers taken from the chip's
// datasheet. This is the smallest possible complete bare-metal program: if you
// understand every line here, you understand what `digitalWrite()` really does.
//
// On these boards the on-board LED is wired to digital pin 13, which is port B
// bit 5 (PB5) on the chip. To blink it we only need to:
//
//   1. make PB5 an OUTPUT     -> set bit 5 of DDRB  (Data Direction Register B)
//   2. drive it HIGH / LOW    -> flip bit 5 of PORTB (Port B output register)
//   3. wait a little between   -> burn some CPU cycles
//
// Side by side with the Arduino sketch you may already know:
//
//     pinMode(13, OUTPUT);      ->  DDRB  bit 5 = 1   (output)
//     digitalWrite(13, HIGH);   ->  PORTB bit 5 = 1   (LED on)
//     digitalWrite(13, LOW);    ->  PORTB bit 5 = 0   (LED off)
//     delay(500);               ->  the busy-wait loop below
//
// Build it with the AVR toolchain (see the README). No <avr/io.h>, no Arduino
// core — the entire dependency list is <stdint.h> for fixed-width integers.
//
// ---- Coming from C? You already know this. ----------------------------------
// If you've written AVR C, this is the exact same hardware dance you've always
// done — just wearing C++ clothes. Here's the Rosetta Stone:
//
//     Classic AVR C (with <avr/io.h>)      This file (raw, no header)
//     --------------------------------     ---------------------------------
//     DDRB  |= (1 << PB5);                 setBit(DDRB,  kLedBit);
//     PORTB |= (1 << PB5);                 setBit(PORTB, kLedBit);
//     PORTB &= ~(1 << PB5);                clearBit(PORTB, kLedBit);
//     sbi(PORTB, PB5); / cbi(...);         setBit(...) / clearBit(...)
//
// Same bytes, same addresses, same `|=` and `&= ~` you've typed a thousand
// times. The only differences: we spell the register addresses out ourselves
// (instead of letting <avr/io.h> #define them), and the helpers are inline
// functions instead of macros — so the compiler type-checks them, but they
// still compile down to the identical SBI/CBI/OUT instructions. Zero overhead,
// zero magic. Read on and you'll recognise every move.

#include <stdint.h>  // freestanding only: uint8_t / uint16_t / uint32_t

namespace {

// ---- Memory-mapped registers (ATmega328P datasheet) -------------------------
// A "register" is just a special memory address that is wired to the silicon.
// Writing to it changes real hardware — here, pin direction and pin voltage.
// These two addresses are all the blink needs.
//
// In classic AVR C, <avr/io.h> hands you these for free as macros, e.g.
//     #define DDRB  (*(volatile uint8_t*)0x24)
// We're just doing that ourselves so there's literally nothing hidden. Same
// addresses (0x24 / 0x25), straight from the datasheet's register map.
constexpr uint16_t DDRB = 0x24;   // Data Direction Register B: 1 = output
constexpr uint16_t PORTB = 0x25;  // Port B output register:    1 = drive high

// Turn a bare address into something you can read/write like a variable. This
// is the C++ spelling of the classic C macro `(*(volatile uint8_t*)addr)`:
// reinterpret the integer address as a pointer to a byte, then dereference it.
// `volatile` tells the compiler this memory can change outside normal program
// flow (the hardware touches it), so it must genuinely read/write every time
// and never cache or optimise the access away — exactly like in C.
[[nodiscard]] inline volatile uint8_t& reg(uint16_t addr) noexcept {
    return *reinterpret_cast<volatile uint8_t*>(addr);
}

constexpr uint8_t kLedPin = 5;                              // PB5 = Arduino "13"

// Build a "bit mask": a byte with a single 1 in the position we care about.
// `1u << kLedPin` takes the value 1 (binary 0000'0001) and shifts it left by
// kLedPin (5) places, so the 1 lands in bit 5:
//
//     1u           = 0000'0001
//     1u << 5      = 0010'0000   <- only bit 5 is set  (0x20)
//
// This mask is how we name "the LED pin" in the byte-wide register: every
// other bit is 0, so operations using it touch bit 5 and leave the rest alone.
constexpr uint8_t kLedBit = static_cast<uint8_t>(1u << kLedPin);

// Set the masked bit(s) to 1 without disturbing the other bits (read-modify-write).
//
// This is just `reg |= mask` — the C one-liner `PORTB |= (1 << PB5);` wrapped in
// a named function. Use OR (|) to TURN BITS ON. OR-ing with 1 forces a bit to 1;
// OR-ing with 0 leaves a bit unchanged. Because the mask is 1 only at bit 5,
// exactly that bit is forced high and the other seven keep their current values:
//
//     current   = abcd'efgh        (a..h = whatever was already there)
//     mask      = 0010'0000
//     a | mask  = ab1d'efgh        <- bit 5 is now 1, the rest untouched
inline void setBit(uint16_t addr, uint8_t bit) noexcept {
    reg(addr) = static_cast<uint8_t>(reg(addr) | bit);
}

// Clear the masked bit(s) to 0 without disturbing the other bits (read-modify-write).
//
// This is just `reg &= ~mask` — the C one-liner `PORTB &= ~(1 << PB5);` wrapped
// in a named function. Use AND with the inverted mask (& ~) to TURN BITS OFF.
// `~mask` flips every bit, giving 0 only at the position we want to clear and 1
// everywhere else. AND-ing with 0 forces a bit to 0; AND-ing with 1 leaves a
// bit unchanged, so only bit 5 is cleared:
//
//     mask        = 0010'0000
//     ~mask       = 1101'1111
//     current     = abcd'efgh
//     a & ~mask   = ab0d'efgh      <- bit 5 is now 0, the rest untouched
inline void clearBit(uint16_t addr, uint8_t bit) noexcept {
    reg(addr) = static_cast<uint8_t>(reg(addr) & ~bit);
}

// ---- A deliberately crude delay ---------------------------------------------
// No library and no timer — we simply keep the CPU busy for a while. `volatile`
// on the counter stops the optimiser from deleting this "useless" empty loop.
// The timing is only approximate; ~1,000,000 iterations is roughly half a
// second at 16 MHz. (The flagship repo, mcu-baremetal, shows the *precise* way
// using a hardware timer and an interrupt.)
void busyWait(uint32_t loops) noexcept {
    for (volatile uint32_t i = 0; i < loops; ++i) {
        // intentionally empty — just burning CPU cycles
    }
}

}  // namespace

int main() {
    setBit(DDRB, kLedBit);  // 1) make PB5 an output  (pinMode 13, OUTPUT)
                            //    == DDRB |= (1 << PB5);

    for (;;) {                      // loop forever (your classic embedded `while(1)`):
        setBit(PORTB, kLedBit);     //   LED on   (digitalWrite 13, HIGH) | PORTB |=  (1<<PB5)
        busyWait(1000000);          //   wait ~0.5 s
        clearBit(PORTB, kLedBit);   //   LED off  (digitalWrite 13, LOW)  | PORTB &= ~(1<<PB5)
        busyWait(1000000);          //   wait ~0.5 s
    }
}
