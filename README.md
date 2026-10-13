# DTMF keypad generator

Press a key and the speaker pin plays that key until the key is released. Each key is one row frequency plus one column frequency.

| | 1209 Hz | 1336 Hz | 1477 Hz | 1633 Hz |
| --- | --- | --- | --- | --- |
| 697 Hz | 1 | 2 | 3 | A |
| 770 Hz | 4 | 5 | 6 | B |
| 852 Hz | 7 | 8 | 9 | C |
| 941 Hz | * | 0 | # | D |

Samples are taken at 32 kHz. The speaker pin is PA25 and is only driven high or low.

## Three approaches

### 1. Add the two sine waves

Compute `sin(2π f1 t) + sin(2π f2 t)` on every sample and send that value to the pin (or to a PWM duty cycle).

The waveform is the real DTMF tone. On this MSPM0 the core has no floating-point unit, so `sin()` from `math.h` is a software routine. It does not finish inside one sample at 32 kHz, so the played frequencies come out low.

### 2. Sine lookup table

Store one cycle of a sine wave in flash. A phase accumulator picks the sample for `f1` and the sample for `f2`, and the program adds those two values.

The reads are fast enough for 32 kHz. The table is the cost: a 256-entry table of 16-bit samples is 512 bytes, and a longer table grows from there. This board's speaker pin cannot reproduce the in-between amplitudes, so the stored samples are mostly thrown away at the last comparison.

### 3. Sign of the sine (this program)

The pin only needs one bit: is the sum above zero, or below zero?

```
sin(2π f1 t) + sin(2π f2 t)
  = 2 · sin(2π · (f1+f2)/2 · t) · cos(2π · (f1−f2)/2 · t)
```

The sign of that product is the sign of the sum.

- `sign(sin)` is the high bit of a phase counter running at `(f1+f2)/2`
- `sign(cos)` is the top two bits of a phase counter running at `(f1−f2)/2`

No `sin()`, no `cos()`, no table. The old angle line used `2.0 * 180 * (f1 ± f2) * t`, which is the full sum and difference, not half of them, so every tone played at twice the keypad frequency. The phase step here is `f * 2^31 / 32000` for those half-frequencies.

SysTick spaces the samples at `CPUCLK_FREQ / 32000` cycles, including the time spent updating the phase. A fixed `delay_cycles()` after the math would stretch every period and pull both frequencies off the DTMF values.

## Hardware

Rows are outputs. Columns are inputs with pulldowns. The pressed key is the column that reads high while its row is driven.

| Signal | Pin |
| --- | --- |
| Row 1–4 | PA15, PA16, PA17, PA24 |
| Column 1–4 | PA18, PB18, PA22, PB19 |
| Speaker | PA25 |
| User LED | `USER_LEDS_PORT` / `USER_LEDS_RED_LED_PIN` |

`get_key()` keeps the first closed contact. The tone runs until a later scan sees a different key or no key. The speaker pin is cleared when the tone stops.

## Build

Replace `main.c` in the Code Composer Studio project that already contains `ti_msp_dl_config.h` from SysConfig. `CPUCLK_FREQ` comes from that header. SysTick is used as a free-running counter and does not take an interrupt.
