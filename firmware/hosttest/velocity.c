#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FM1_AUDIO_CODE
#include "model.h"

void sampler_velocity_preview(uint8_t curve, uint8_t learn)
{
    sampler_touch_preview(curve, sampler_curve_exponent(curve), learn);
}

void sampler_velocity_capture(struct sampler_velocity_profile *profile)
{
    sampler_touch_capture(profile);
}

void sampler_velocity_reset(void)
{
    sampler_touch_reset();
}

int sampler_velocity_apply(const struct sampler_velocity_profile *profile)
{
    if (!sampler_touch_valid(profile))
        return 0;
    sampler_touch_apply(profile, sampler_curve_exponent(profile->curve));
    return 1;
}

static uint8_t usb_reply[128];
static unsigned usb_reply_size, usb_replies;
static int usb_busy;

static unsigned n14(const uint8_t *p)
{
    return ((p[0] & 127u) << 7) | (p[1] & 127u);
}

static int sysex_reply(const uint8_t *body, unsigned n)
{
    if (usb_busy)
        return -2;
    assert(n <= sizeof usb_reply);
    memcpy(usb_reply, body, n);
    usb_reply_size = n;
    ++usb_replies;
    return 0;
}

#include "sampler_calibration.inc"

static uint32_t ticks;

static void sample(unsigned key, unsigned value)
{
    ticks += 79200; /* Three 11-column scans at 10 kHz. */
    sense_sample(key, value, ticks, sense_want_drive(key));
}

static void land(unsigned key, unsigned driven)
{
    assert(sn_key[key].st == SN_OPEN);
    sample(key, 300);
    sample(key, driven);
    sample(key, 240);
    sample(key, driven);
    sample(key, 240);
    assert(sn_key[key].st == SN_DOWN);
}

static void release(unsigned key)
{
    for (unsigned samples = 0; samples < 8; ++samples) sample(key, 1023);
    assert(sn_key[key].st == SN_OPEN);
}

static void test_calibration(void)
{
    struct sampler_velocity_profile profile;
    sampler_touch_capture(&profile);
    assert(profile.curve == SAMPLER_CURVE_NEUTRAL && sampler_touch_valid(&profile));
    assert(profile.hard[3] == 530 && profile.soft[3] == 896 && profile.landings[3] == 0);
    assert(sampler_curve_exponent(0) == 35);
    assert(sampler_curve_exponent(SAMPLER_CURVE_NEUTRAL) == 70);
    assert(sampler_curve_exponent(SAMPLER_CURVE_MAX) == 140);

    sampler_velocity_preview(0, 0);
    land(3, 710);
    const uint8_t easy = sn_key[3].vel;
    release(3);
    sampler_velocity_preview(SAMPLER_CURVE_MAX, 0);
    land(3, 710);
    assert(sn_key[3].vel < easy);
    release(3);

    sampler_velocity_preview(SAMPLER_CURVE_NEUTRAL, 1);
    land(3, 476);
    release(3);
    sampler_touch_capture(&profile);
    assert(sampler_touch_valid(&profile));
    assert(profile.hard[3] < 530 && profile.soft[3] < 896 && profile.landings[3] == 1);
    assert(profile.hard[4] == 530 && profile.soft[4] == 896 && profile.landings[4] == 0);

    for (unsigned i = 0; i < 10; ++i) {
        land(3, 855);
        release(3);
    }
    assert(sn_key[3].dfloor == profile.hard[3] && sn_key[3].dceil == profile.soft[3]);
    assert(sn_key[3].nland == 1);

    sampler_velocity_preview(SAMPLER_CURVE_NEUTRAL, 1);
    assert(sampler_velocity_apply(&profile));
    assert(!sn_cfg.learn);
    assert(sn_key[3].dfloor == profile.hard[3] && sn_key[3].dceil == profile.soft[3]);
    struct sampler_velocity_profile invalid = profile;
    invalid.hard[3] = SAMPLER_ADC_HARD_MIN - 1;
    assert(!sampler_velocity_apply(&invalid));
    invalid = profile;
    invalid.soft[3] = invalid.hard[3] + SAMPLER_ADC_SPAN_MIN - 1;
    assert(!sampler_velocity_apply(&invalid));
    assert(sn_key[3].dfloor == profile.hard[3] && sn_key[3].dceil == profile.soft[3]);

    sampler_velocity_preview(SAMPLER_CURVE_NEUTRAL, 1);
    for (unsigned i = 0; i < 300; ++i) {
        sense_learn(&sn_key[5], 0);
        sense_learn(&sn_key[6], 1023);
        const unsigned hard = sn_key[5].dfloor;
        const unsigned soft = sn_key[6].dceil;
        assert(hard >= SAMPLER_ADC_HARD_MIN && soft <= SAMPLER_ADC_SOFT_MAX);
        assert(sn_key[5].dceil >= hard + SAMPLER_ADC_SPAN_MIN);
        assert(soft >= sn_key[6].dfloor + SAMPLER_ADC_SPAN_MIN);
    }
    sampler_touch_capture(&profile);
    assert(sampler_touch_valid(&profile));
    assert(profile.landings[5] == 127 && profile.landings[6] == 127);

    sampler_velocity_reset();
    sampler_touch_capture(&profile);
    assert(profile.curve == SAMPLER_CURVE_NEUTRAL && profile.hard[3] == 530 && profile.soft[3] == 896);
    assert(profile.landings[3] == 0 && !sn_cfg.learn);
}

static void test_protocol(void)
{
    const uint8_t preview[] = {0, 8, 1};
    velocity_command(preview, sizeof preview);
    assert(sn_cfg.learn && sampler_curve == 8);

    const uint8_t query[] = {4};
    const uint8_t save[] = {1};
    velocity_command(query, sizeof query);
    velocity_command(save, sizeof save);
    assert(!sn_cfg.learn);

    usb_busy = 1;
    velocity_send_pending();
    assert(usb_replies == 0);
    usb_busy = 0;
    velocity_send_pending();
    assert(usb_replies == 2 && usb_reply_size == 102);
    assert(usb_reply[0] == 0x43 && usb_reply[1] == 8);
    assert(n14(usb_reply + 2) == 530 && n14(usb_reply + 4) == 896 && usb_reply[6] == 0);
    velocity_send_pending();
    assert(usb_replies == 2);

    const uint8_t reset[] = {2};
    velocity_command(reset, sizeof reset);
    assert(!sn_cfg.learn && sampler_curve == SAMPLER_CURVE_NEUTRAL);
    uint8_t load[1 + velocityProfileBytes] = {3};
    memcpy(load + 1, usb_reply + 1, velocityProfileBytes);
    velocity_command(load, sizeof load);
    assert(!sn_cfg.learn && sampler_curve == 8);
    load[1] = 0;
    load[2] = 0;
    load[3] = 0;
    velocity_command(load, sizeof load);
    assert(sampler_curve == 8 && sn_key[0].dfloor == 530);
}

int main(void)
{
    sampler_touch_init();
    assert(sense_is_analog(0));
    assert(sense_is_analog(19));
    assert(!sense_is_analog(20));

    land(0, 824);
    const uint8_t soft = sn_key[0].vel;
    land(19, 610);
    const uint8_t firm = sn_key[19].vel;
    assert(soft > 1 && firm > soft + 20u && firm < 127u);
    assert(sn_key[0].vel == soft && sn_key[0].st == SN_DOWN);
    release(0);
    release(19);
    assert(sn_key[0].vel == soft && sn_key[19].vel == firm);
    land(0, 610);
    assert(sn_key[0].vel == firm);
    release(0);

    sample(1, 580); /* Merely brushing a key must not generate a press. */
    sample(1, 1023);
    sample(1, 1023);
    assert(sn_key[1].st == SN_OPEN && sn_key[1].vel == 0);

    const unsigned top = SAMPLER_KEYS - 1;
    sense_probe(top, 120, 0);
    sense_probe(top, 120, 1);
    assert(sampler_top_velocity(&sn_probe[top]) == SAMPLER_DEFAULT_VELOCITY);
    sense_probe(top, 149, 1);
    const uint8_t top_soft = sampler_top_velocity(&sn_probe[top]);
    sense_probe(top, 120, 0);
    sense_probe(top, 160, 1);
    const uint8_t top_firm = sampler_top_velocity(&sn_probe[top]);
    assert(top_soft > 1 && top_firm > top_soft);
    sense_probe(top, 1023, 1);
    assert(sampler_top_velocity(&sn_probe[top]) == SAMPLER_DEFAULT_VELOCITY);

    test_calibration();
    test_protocol();

    printf("ADC velocity at the same key: soft %u, strong %u. Top-row probe: %u / %u. Calibration and SysEx tests passed.\n",
           soft, firm, top_soft, top_firm);
    return 0;
}
