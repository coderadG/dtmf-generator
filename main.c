/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"

/*
 * DTMF keypad.
 *
 * One row tone and one column tone are added for the key that is down.
 * The speaker is a GPIO, so each sample only asks whether that sum is
 * above or below zero. The sign is taken from two phase counters.
 * sin() and a sine table are not used.
 *
 * Samples run at SAMPLE_RATE. SysTick counts CPU cycles so the time
 * spent on the phase update sits inside the sample period.
 */

#define LED_TOGGLE_DELAY 0.5
#define LED_TOGGLE_CYC (CPUCLK_FREQ * LED_TOGGLE_DELAY)
#define SAMPLE_RATE 32000

/*
 * One turn of a phase word is 2^32 counts.
 * The sum and difference tones run at half of (f1+f2) and (f1-f2),
 * so their step size is f * 2^31 / SAMPLE_RATE.
 */
#define PHASE_HALF_SCALE 2147483648ULL

int active_row = -1;
int active_col = -1;

const float row_freq[4] = {697, 770, 852, 941};
const float col_freq[4] = {1209, 1336, 1477, 1633};

/* Previous SysTick value at the start of a sample. */
static uint32_t sample_mark = 0;
static int sample_mark_valid = 0;

/*
 * TASK-1
 * Toggle the user LED once, then wait.
 */
void toggle_led(void) {
  DL_GPIO_togglePins(USER_LEDS_PORT, USER_LEDS_RED_LED_PIN);
  delay_cycles(LED_TOGGLE_CYC / 4);
}

/*
 * Scan the keypad. Drive one row high, then read the four columns.
 * A high column means that key is pressed. Columns need pulldowns.
 *
 * Rows:    PA15, PA16, PA17, PA24
 * Columns: PA18, PB18, PA22, PB19
 *
 * Keeps the first key found. Both indexes stay -1 if no key is down.
 */
void get_key(void) {

  active_row = -1;
  active_col = -1;

  DL_GPIO_setPins(GPIOA, (1 << 15));
  delay_cycles(100);

  if (DL_GPIO_readPins(GPIOA, (1 << 18))) {
    active_row = 0;
    active_col = 0;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 18))) {
    active_row = 0;
    active_col = 1;
  } else if (DL_GPIO_readPins(GPIOA, (1 << 22))) {
    active_row = 0;
    active_col = 2;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 19))) {
    active_row = 0;
    active_col = 3;
  }

  DL_GPIO_clearPins(GPIOA, (1 << 15));

  if (active_row != -1)
    return;

  DL_GPIO_setPins(GPIOA, (1 << 16));
  delay_cycles(100);

  if (DL_GPIO_readPins(GPIOA, (1 << 18))) {
    active_row = 1;
    active_col = 0;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 18))) {
    active_row = 1;
    active_col = 1;
  } else if (DL_GPIO_readPins(GPIOA, (1 << 22))) {
    active_row = 1;
    active_col = 2;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 19))) {
    active_row = 1;
    active_col = 3;
  }

  DL_GPIO_clearPins(GPIOA, (1 << 16));

  if (active_row != -1)
    return;

  DL_GPIO_setPins(GPIOA, (1 << 17));
  delay_cycles(100);

  if (DL_GPIO_readPins(GPIOA, (1 << 18))) {
    active_row = 2;
    active_col = 0;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 18))) {
    active_row = 2;
    active_col = 1;
  } else if (DL_GPIO_readPins(GPIOA, (1 << 22))) {
    active_row = 2;
    active_col = 2;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 19))) {
    active_row = 2;
    active_col = 3;
  }

  DL_GPIO_clearPins(GPIOA, (1 << 17));

  if (active_row != -1)
    return;

  DL_GPIO_setPins(GPIOA, (1 << 24));
  delay_cycles(100);

  if (DL_GPIO_readPins(GPIOA, (1 << 18))) {
    active_row = 3;
    active_col = 0;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 18))) {
    active_row = 3;
    active_col = 1;
  } else if (DL_GPIO_readPins(GPIOA, (1 << 22))) {
    active_row = 3;
    active_col = 2;
  } else if (DL_GPIO_readPins(GPIOB, (1 << 19))) {
    active_row = 3;
    active_col = 3;
  }

  DL_GPIO_clearPins(GPIOA, (1 << 24));
}

/*
 * Wait until CPUCLK_FREQ / SAMPLE_RATE cycles have passed since the
 * previous sample. SysTick counts down. The mask keeps the subtract
 * correct when the 24-bit counter rolls over.
 */
static void wait_next_sample(void) {
  uint32_t period = CPUCLK_FREQ / SAMPLE_RATE;

  if (sample_mark_valid) {
    while (((sample_mark - SysTick->VAL) & 0x00FFFFFFu) < period) {
    }
  }

  sample_mark = SysTick->VAL;
  sample_mark_valid = 1;
}

/*
 * One sample of the DTMF pair f1 (row) and f2 (column) on PA25.
 *
 * sin(2*pi*f1*t) + sin(2*pi*f2*t)
 *   = 2 * sin(2*pi*(f1+f2)/2*t) * cos(2*pi*(f1-f2)/2*t)
 *
 * Only the sign of that product is required:
 *   high bit of the sum phase     -> sign of sin
 *   top two bits of the diff phase -> sign of cos
 *
 * The phase step is (f/2) / SAMPLE_RATE turned into a 32-bit word.
 * Adding that word each sample replaces both the angle and fmod().
 */
void play_tone(float f1, float f2) {
  static float prev_f1 = -1.0f;
  static float prev_f2 = -1.0f;
  static uint32_t step_sum = 0;
  static uint32_t step_diff = 0;
  static uint32_t phase_sum = 0;
  static uint32_t phase_diff = 0;

  uint32_t quadrant;
  int sign_sin;
  int sign_cos;

  wait_next_sample();

  if (f1 != prev_f1 || f2 != prev_f2) {
    prev_f1 = f1;
    prev_f2 = f2;
    step_sum =
        (uint32_t)(((uint64_t)(f1 + f2) * PHASE_HALF_SCALE) / SAMPLE_RATE);
    step_diff =
        (uint32_t)(((uint64_t)(f1 - f2) * PHASE_HALF_SCALE) / SAMPLE_RATE);
  }

  phase_sum += step_sum;
  phase_diff += step_diff;

  /* sin >= 0 on the first half cycle. */
  if ((phase_sum & 0x80000000u) == 0u) {
    sign_sin = 1;
  } else {
    sign_sin = -1;
  }

  /* cos >= 0 in quadrant 0 and quadrant 3. */
  quadrant = phase_diff >> 30;
  if (quadrant == 0u || quadrant == 3u) {
    sign_cos = 1;
  } else {
    sign_cos = -1;
  }

  if ((sign_sin * sign_cos) > 0) {
    DL_GPIO_setPins(GPIOA, (1 << 25));
  } else {
    DL_GPIO_clearPins(GPIOA, (1 << 25));
  }
}

int main(void) {
  SYSCFG_DL_init();

  /*
   * Sample clock. play_tone() reads SysTick->VAL and does not use
   * the SysTick interrupt.
   */
  SysTick->LOAD = 0x00FFFFFF;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

  while (1) {
    get_key();

    if (active_row != -1) {
      int row = active_row;
      int col = active_col;

      while (1) {
        play_tone(row_freq[row], col_freq[col]);
        static int check_count = 0;

        check_count++;

        if (check_count >= 64) {
          check_count = 0;

          get_key();

          if (active_row != row || active_col != col) {
            break;
          }
        }
      }

      DL_GPIO_clearPins(GPIOA, (1 << 25));
    }
  }
}
