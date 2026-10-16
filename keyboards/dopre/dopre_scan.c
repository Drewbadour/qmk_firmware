// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "dopre_scan.h"
#include "dopre_filter.h"

// Capacitive "dopre" sensing for the STM32G0B1, ported from HONKS (STM32G0x1_DOPRE.h and
// keyboard_DOPRE.h).
//
// Each sense channel is a relaxation oscillator wired to a timer input-capture pin. The timer
// free-runs at SYSCLK, every rising edge stores the counter in CCRx, and a circular DMA channel
// moves each capture into a 16-entry ring buffer without CPU involvement. The period of the
// oscillator is the difference between the two newest captures. A 3-bit analog mux, shared by
// all channels, selects which of the keys on each channel is being measured.
//
// The timers, DMA and DMAMUX are driven directly through the device registers. That is safe because
// no ChibiOS driver claims them: the system tick is SysTick (see chconf.h), and the GPT/PWM/ICU/
// SPI/I2C/UART/ADC drivers that could use TIM2-4 or DMA are all disabled.

#define DOPRE_BUFFER_LENGTH 16U // Captures kept per channel; must be a power of two.
#define DOPRE_MAX_CHANNELS 12   // DMA1 has 7 channels and DMA2 has 5.
#define DOPRE_DMA1_CHANNELS 7

#define DOPRE_Y_COUNT(pin) +1
#define DOPRE_NUM_MUX_PINS (0 DOPRE_MUX_PINS(DOPRE_Y_COUNT))

_Static_assert(DOPRE_NUM_CHANNELS == MATRIX_ROWS, "DOPRE_SENSE_CHANNELS must have one entry per matrix row (keyboard.json matrix_size.rows)");
_Static_assert(DOPRE_NUM_CHANNELS <= DOPRE_MAX_CHANNELS, "This MCU has 12 DMA channels, so at most 12 sense channels");
_Static_assert(MATRIX_COLS <= 8, "matrix_row_t is 8 bits, so at most 8 mux states");
_Static_assert(MATRIX_COLS <= (1 << DOPRE_NUM_MUX_PINS), "Not enough DOPRE_MUX_PINS to address MATRIX_COLS mux states");
_Static_assert(DOPRE_THRESHOLD_ON > DOPRE_THRESHOLD_OFF, "DOPRE_THRESHOLD_ON must exceed DOPRE_THRESHOLD_OFF");
_Static_assert(DOPRE_SETTLE_TIMEOUT_TICKS < 65536, "DOPRE_SETTLE_TIMEOUT_TICKS is measured on a 16-bit window");

// No pin may be used twice. A sense pin that is also a mux select line would have the mux output
// fighting the oscillator. Every pin claims a slot (port, pad) in a table through a designated
// initializer, and claiming a slot twice makes -Woverride-init an error.
#define DOPRE_PIN_SLOT(pin) ((((pin) >> 10) & 0xFU) * 16U + ((pin) & 0xFU)) // GPIO ports sit 0x400 apart.
#define DOPRE_X_CLAIM(pin, af, tim, ch) [DOPRE_PIN_SLOT(pin)] = __COUNTER__ + 1,
#define DOPRE_Y_CLAIM(pin) [DOPRE_PIN_SLOT(pin)] = __COUNTER__ + 1,

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Woverride-init"
static const uint16_t dopre_pin_claims[6 * 16] __attribute__((unused)) = {
    DOPRE_SENSE_CHANNELS(DOPRE_X_CLAIM)
    DOPRE_MUX_PINS(DOPRE_Y_CLAIM)
};
#pragma GCC diagnostic pop

// ---- Tables generated from the pin map ------------------------------------------------------

#define DOPRE_X_TIM(pin, af, tim, ch) TIM##tim,
#define DOPRE_X_CCR(pin, af, tim, ch) &TIM##tim->CCR##ch,
#define DOPRE_X_REQ(pin, af, tim, ch) STM32_DMAMUX1_TIM##tim##_CH##ch,
#define DOPRE_X_TIMEN(pin, af, tim, ch) | RCC_APBENR1_TIM##tim##EN

static TIM_TypeDef *const       dopre_timers[]      = {DOPRE_SENSE_CHANNELS(DOPRE_X_TIM)};
static volatile uint32_t *const dopre_ccr[]         = {DOPRE_SENSE_CHANNELS(DOPRE_X_CCR)};
static const uint8_t            dopre_dma_request[] = {DOPRE_SENSE_CHANNELS(DOPRE_X_REQ)};

// DMA channel N serves sense channel N: DMA1 channels 1-7 first, then DMA2 channels 1-5. The
// DMAMUX channel numbering follows the same order.
static DMA_Channel_TypeDef *const dopre_dma[DOPRE_MAX_CHANNELS] = {
    DMA1_Channel1, DMA1_Channel2, DMA1_Channel3, DMA1_Channel4, DMA1_Channel5, DMA1_Channel6, DMA1_Channel7,
    DMA2_Channel1, DMA2_Channel2, DMA2_Channel3, DMA2_Channel4, DMA2_Channel5,
};

// ---- State ----------------------------------------------------------------------------------

static volatile uint32_t capture_buffer[DOPRE_NUM_CHANNELS][DOPRE_BUFFER_LENGTH] __attribute__((aligned(DOPRE_BUFFER_LENGTH * sizeof(uint32_t))));

static dopre_key_t keys[MATRIX_ROWS][MATRIX_COLS];

// Mux state that is currently selected, i.e. the one the next reading belongs to.
static uint8_t mux_state = 0;

// DMA write position of each channel when the mux last changed. Used to let MUX_SETTLE_CAPTURES
// captures through before a reading is trusted.
static uint16_t write_snapshot[DOPRE_NUM_CHANNELS];

static uint32_t settle_timeouts = 0;
static uint32_t scan_count      = 0;

// ---- Hardware access ------------------------------------------------------------------------

// Position the DMA will write next, i.e. one past the newest capture.
static inline uint16_t write_index(uint8_t channel) {
    uint32_t remaining = dopre_dma[channel]->CNDTR;
    return (uint16_t)((DOPRE_BUFFER_LENGTH - remaining) & (DOPRE_BUFFER_LENGTH - 1));
}

// Oscillator period in timer ticks: the difference between the two newest captures. TIM3 and TIM4
// are 16-bit, so the difference is always taken modulo 2^16.
static inline uint32_t read_period(uint8_t channel) {
    uint16_t write  = write_index(channel);
    uint16_t latest = (uint16_t)((write - 1U) & (DOPRE_BUFFER_LENGTH - 1));
    uint16_t before = (uint16_t)((write - 2U) & (DOPRE_BUFFER_LENGTH - 1));
    return (capture_buffer[channel][latest] - capture_buffer[channel][before]) & 0xFFFFU;
}

static inline void snapshot_write_indices(void) {
    for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
        write_snapshot[channel] = write_index(channel);
    }
}

static inline void write_mux_pin(pin_t pin, bool level) {
    gpio_write_pin(pin, level);
}

#define DOPRE_Y_SELECT(pin) write_mux_pin(pin, (state >> bit++) & 1U);

static inline void select_mux(uint8_t state) {
    uint8_t bit = 0;
    DOPRE_MUX_PINS(DOPRE_Y_SELECT);
}

// Blocks until every channel has captured MUX_SETTLE_CAPTURES edges since the mux last changed.
// Normally the time spent computing and running the rest of QMK already covers this. A channel
// whose oscillator has stopped never gets there, so give up after a timeout and reuse its stale
// reading rather than hang the keyboard.
static void wait_for_settle(void) {
    TIM_TypeDef *clock = dopre_timers[0];
    uint16_t     start = (uint16_t)clock->CNT;

    for (;;) {
        bool settled = true;
        for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
            uint16_t captured = (uint16_t)((write_index(channel) - write_snapshot[channel]) & (DOPRE_BUFFER_LENGTH - 1));
            if (captured < DOPRE_MUX_SETTLE_CAPTURES) {
                settled = false;
                break;
            }
        }
        if (settled) {
            return;
        }

        if ((uint16_t)((uint16_t)clock->CNT - start) > DOPRE_SETTLE_TIMEOUT_TICKS) {
            ++settle_timeouts;
            return;
        }
    }
}

// ---- Initialization -------------------------------------------------------------------------

// Rising-edge input capture, no prescaler and no filter, with a DMA request on every capture.
#define DOPRE_TIM_IC_1(t)                                                                                   \
    do {                                                                                                    \
        (t)->CCMR1 = ((t)->CCMR1 & ~(TIM_CCMR1_CC1S | TIM_CCMR1_IC1PSC | TIM_CCMR1_IC1F)) | TIM_CCMR1_CC1S_0; \
        (t)->CCER  = ((t)->CCER & ~(TIM_CCER_CC1P | TIM_CCER_CC1NP)) | TIM_CCER_CC1E;                        \
        (t)->DIER |= TIM_DIER_CC1DE;                                                                        \
    } while (0)
#define DOPRE_TIM_IC_2(t)                                                                                   \
    do {                                                                                                    \
        (t)->CCMR1 = ((t)->CCMR1 & ~(TIM_CCMR1_CC2S | TIM_CCMR1_IC2PSC | TIM_CCMR1_IC2F)) | TIM_CCMR1_CC2S_0; \
        (t)->CCER  = ((t)->CCER & ~(TIM_CCER_CC2P | TIM_CCER_CC2NP)) | TIM_CCER_CC2E;                        \
        (t)->DIER |= TIM_DIER_CC2DE;                                                                        \
    } while (0)
#define DOPRE_TIM_IC_3(t)                                                                                   \
    do {                                                                                                    \
        (t)->CCMR2 = ((t)->CCMR2 & ~(TIM_CCMR2_CC3S | TIM_CCMR2_IC3PSC | TIM_CCMR2_IC3F)) | TIM_CCMR2_CC3S_0; \
        (t)->CCER  = ((t)->CCER & ~(TIM_CCER_CC3P | TIM_CCER_CC3NP)) | TIM_CCER_CC3E;                        \
        (t)->DIER |= TIM_DIER_CC3DE;                                                                        \
    } while (0)
#define DOPRE_TIM_IC_4(t)                                                                                   \
    do {                                                                                                    \
        (t)->CCMR2 = ((t)->CCMR2 & ~(TIM_CCMR2_CC4S | TIM_CCMR2_IC4PSC | TIM_CCMR2_IC4F)) | TIM_CCMR2_CC4S_0; \
        (t)->CCER  = ((t)->CCER & ~(TIM_CCER_CC4P | TIM_CCER_CC4NP)) | TIM_CCER_CC4E;                        \
        (t)->DIER |= TIM_DIER_CC4DE;                                                                        \
    } while (0)

// Free-run at SYSCLK (prescaler 0) and start the counter.
#define DOPRE_X_TIM_INIT(pin, af, tim, ch) \
    TIM##tim->PSC = 0UL;                   \
    TIM##tim->ARR = 0xFFFFFFFFUL;          \
    DOPRE_TIM_IC_##ch(TIM##tim);           \
    TIM##tim->CR1 |= TIM_CR1_CEN;

// The sense pins are timer inputs; the pull is left off because the oscillator drives them.
#define DOPRE_X_PIN_INIT(pin, af, tim, ch) \
    palSetLineMode(pin, PAL_STM32_MODE_ALTERNATE | PAL_STM32_ALTERNATE(af) | PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING);

#define DOPRE_Y_PIN_INIT(pin)                                                                                                    \
    palSetLineMode(pin, PAL_STM32_MODE_OUTPUT | PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_LOWEST | PAL_STM32_PUPDR_PULLDOWN); \
    palClearLine(pin);

static void dma_init(void) {
    // A channel must be disabled before it can be configured.
    for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
        dopre_dma[channel]->CCR &= ~DMA_CCR_EN;
    }
    for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
        for (uint32_t guard = 0; (dopre_dma[channel]->CCR & DMA_CCR_EN) && (guard < 100000UL); ++guard) {
        }
    }

    for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
        // Writing a flag bit clears it; clear all four flags (global, complete, half, error).
        if (channel < DOPRE_DMA1_CHANNELS) {
            DMA1->IFCR = 0xFUL << (4U * channel);
        } else {
            DMA2->IFCR = 0xFUL << (4U * (channel - DOPRE_DMA1_CHANNELS));
        }

        // Circular copy of the capture register into the ring buffer, 32 bits at a time.
        dopre_dma[channel]->CPAR  = (uint32_t)dopre_ccr[channel];
        dopre_dma[channel]->CMAR  = (uint32_t)capture_buffer[channel];
        dopre_dma[channel]->CNDTR = DOPRE_BUFFER_LENGTH;
        dopre_dma[channel]->CCR   = DMA_CCR_CIRC | DMA_CCR_MINC | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_PL_0 | DMA_CCR_PL_1;

        // Route this timer channel's capture request to the DMA channel.
        volatile uint32_t *mux = &DMAMUX1_Channel0->CCR + channel;
        *mux                   = (*mux & ~DMAMUX_CxCR_DMAREQ_ID) | dopre_dma_request[channel];
    }

    for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
        dopre_dma[channel]->CCR |= DMA_CCR_EN;
    }
}

void dopre_init(void) {
    for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
        for (uint8_t slot = 0; slot < DOPRE_BUFFER_LENGTH; ++slot) {
            capture_buffer[channel][slot] = 0;
        }
    }

    RCC->APBENR1 |= (0 DOPRE_SENSE_CHANNELS(DOPRE_X_TIMEN));
    RCC->AHBENR |= RCC_AHBENR_DMA1EN | ((DOPRE_NUM_CHANNELS > DOPRE_DMA1_CHANNELS) ? RCC_AHBENR_DMA2EN : 0U);
    (void)RCC->APBENR1; // Read back so the clocks are running before the peripherals are touched.
    (void)RCC->AHBENR;

    DOPRE_SENSE_CHANNELS(DOPRE_X_PIN_INIT)
    DOPRE_MUX_PINS(DOPRE_Y_PIN_INIT)

    // The DMA is armed before the timers start, so no capture is missed.
    dma_init();
    DOPRE_SENSE_CHANNELS(DOPRE_X_TIM_INIT)

    // Start on mux state 0 and let it settle before the first reading.
    mux_state = 0;
    select_mux(mux_state);
    snapshot_write_indices();
}

// ---- Scanning -------------------------------------------------------------------------------

bool dopre_scan(matrix_row_t current_matrix[]) {
    bool     changed = false;
    uint32_t periods[DOPRE_NUM_CHANNELS];

    for (uint8_t step = 0; step < MATRIX_COLS; ++step) {
        // Read the mux state selected last time round, once every channel has settled on it.
        wait_for_settle();
        for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
            periods[channel] = read_period(channel);
        }
        uint8_t measured = mux_state;

        // Switch the mux now, so the next state settles while this one is filtered below.
        mux_state = (uint8_t)(mux_state + 1U);
        if (mux_state >= MATRIX_COLS) {
            mux_state = 0;
        }
        select_mux(mux_state);
        snapshot_write_indices();

        const matrix_row_t bit = (matrix_row_t)(1U << measured);
        for (uint8_t channel = 0; channel < DOPRE_NUM_CHANNELS; ++channel) {
            dopre_key_t *key = &keys[channel][measured];
            dopre_filter_update(key, periods[channel]);

            matrix_row_t row     = current_matrix[channel];
            matrix_row_t updated = key->is_on ? (matrix_row_t)(row | bit) : (matrix_row_t)(row & ~bit);
            if (updated != row) {
                current_matrix[channel] = updated;
                changed                 = true;
            }
        }
    }

    ++scan_count;
    return changed;
}

// ---- Tuning readout -------------------------------------------------------------------------

uint16_t dopre_get_period(uint8_t row, uint8_t col) {
    return (row < MATRIX_ROWS && col < MATRIX_COLS) ? keys[row][col].last_period : 0;
}

uint16_t dopre_get_baseline(uint8_t row, uint8_t col) {
    return (row < MATRIX_ROWS && col < MATRIX_COLS) ? (uint16_t)(keys[row][col].baseline_fp >> 8) : 0;
}

bool dopre_get_is_on(uint8_t row, uint8_t col) {
    return (row < MATRIX_ROWS && col < MATRIX_COLS) ? keys[row][col].is_on : false;
}

uint16_t dopre_take_peak(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return 0;
    }
    uint16_t peak = keys[row][col].peak_rise;
    keys[row][col].peak_rise = 0;
    return peak;
}

uint32_t dopre_get_scan_count(void) {
    return scan_count;
}

uint32_t dopre_get_settle_timeouts(void) {
    return settle_timeouts;
}
