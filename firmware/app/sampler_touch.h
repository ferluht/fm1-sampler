/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#pragma once

#include <stdint.h>

enum {
    SAMPLER_KEYS = 27,
    SAMPLER_ANALOG_KEYS = 20,
    SAMPLER_DEFAULT_VELOCITY = 100,
    SAMPLER_CURVE_NEUTRAL = 5, /* minus 5 yields the CTL-1 value */
    SAMPLER_CURVE_MAX = 10,
    SAMPLER_ADC_HARD_MIN = 201,
    SAMPLER_ADC_SOFT_MAX = 1020,
    SAMPLER_ADC_SPAN_MIN = 80,
    SAMPLER_ADC_HARD_MAX = SAMPLER_ADC_SOFT_MAX - SAMPLER_ADC_SPAN_MIN,
};

struct sampler_velocity_profile {
    uint8_t curve;
    uint16_t hard[SAMPLER_ANALOG_KEYS];
    uint16_t soft[SAMPLER_ANALOG_KEYS];
    uint8_t landings[SAMPLER_ANALOG_KEYS];
};

/* Control-task snapshot published by the most recent fm1_controls_read(). */
uint8_t sampler_key_velocity(unsigned key);

/* Control-task operations. The board driver protects values shared with the ISR. */
void sampler_velocity_preview(uint8_t curve, uint8_t learn);
void sampler_velocity_capture(struct sampler_velocity_profile *profile);
void sampler_velocity_reset(void);
int sampler_velocity_apply(const struct sampler_velocity_profile *profile);
