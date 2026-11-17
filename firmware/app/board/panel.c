/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 珠海笙科智能科技有限公司 (M-VAVE).
 * Connect the FM1 control API to the board scanner. */
#include "fm1.h"
#include "fm1_device.h"
#include "fm1_input.h"
#include "fm1_lcd_hw.h"
#include "lcd.inc"

static uint8_t controls_started;
static uint8_t key_velocities[SAMPLER_KEYS];
uint8_t fm1_io_trs_midi_enabled(void) { return 1U; }

void fm1_controls_init(void)
{
    if (!controls_started) {
        fm1_input_init();
        fm1_device_start(fm1_input_tick);
        controls_started = 1U;
    }
}

void fm1_controls_read(struct fm1_controls *state)
{
    if (!state) return;
    struct fm1_controls snapshot;
    uint32_t irq_state = fm1_device_irq_save();
    const uint32_t digitalKeys = fm1_in.notes;
    const int adcWorking = samplerAdc.haveAnalogSample &&
        SAMPLER_TIMER4_COUNT - samplerAdc.lastAnalogSample < samplerAdcTimeoutTicks;
    snapshot.keys = adcWorking ? digitalKeys & ~samplerAnalogMask : digitalKeys;
    for (unsigned key = 0; key < SAMPLER_ANALOG_KEYS; ++key) {
        const sense_key_t* state = &sn_key[key];
        key_velocities[key] = SAMPLER_DEFAULT_VELOCITY;
        if (adcWorking && (state->st == SN_DOWN || state->st == SN_LIFT)) {
            snapshot.keys |= 1u << key;
            key_velocities[key] = state->vel;
        }
    }
    /* Copy the raw top-row peaks before releasing the scanner; scale them
     * outside the IRQ guard using the same probe curve as fm1-base. */
    sense_probe_t probes[SAMPLER_KEYS - SAMPLER_ANALOG_KEYS];
    for (unsigned key = SAMPLER_ANALOG_KEYS; key < SAMPLER_KEYS; ++key)
        probes[key - SAMPLER_ANALOG_KEYS] = sn_probe[key];
    snapshot.knob = samplerAdc.master;
    snapshot.battery = samplerAdc.battery;
    snapshot.buttons = fm1_in.buttons;
    unsigned index = FM1_NENC;
    while (index) {
        --index;
        snapshot.encoders[index] = fm1_in.enc_steps[index];
        fm1_in.enc_steps[index] = 0;
    }
    fm1_device_irq_restore(irq_state);
    for (unsigned key = SAMPLER_ANALOG_KEYS; key < SAMPLER_KEYS; ++key) {
        const sense_probe_t* probe = &probes[key - SAMPLER_ANALOG_KEYS];
        key_velocities[key] = adcWorking && (snapshot.keys & (1u << key)) ?
            sampler_top_velocity(probe) : SAMPLER_DEFAULT_VELOCITY;
    }
    *state = snapshot;
}

uint8_t sampler_key_velocity(unsigned key)
{
    return key < SAMPLER_KEYS && key_velocities[key] ? key_velocities[key] : SAMPLER_DEFAULT_VELOCITY;
}

void sampler_velocity_preview(uint8_t curve, uint8_t learn)
{
    const uint16_t exponent = sampler_curve_exponent(curve);
    const uint32_t irq_state = fm1_device_irq_save();
    sampler_touch_preview(curve, exponent, learn);
    fm1_device_irq_restore(irq_state);
}

void sampler_velocity_capture(struct sampler_velocity_profile *profile)
{
    const uint32_t irq_state = fm1_device_irq_save();
    sampler_touch_capture(profile);
    fm1_device_irq_restore(irq_state);
}

void sampler_velocity_reset(void)
{
    const uint32_t irq_state = fm1_device_irq_save();
    sampler_touch_reset();
    fm1_device_irq_restore(irq_state);
}

int sampler_velocity_apply(const struct sampler_velocity_profile *profile)
{
    if (!sampler_touch_valid(profile))
        return 0;
    const uint16_t exponent = sampler_curve_exponent(profile->curve);
    const uint32_t irq_state = fm1_device_irq_save();
    sampler_touch_apply(profile, exponent);
    fm1_device_irq_restore(irq_state);
    return 1;
}

void fm1_led_set(unsigned key, int on)
{
    fm1_led_key(key, on);
}
