
void solid(Strip *s) {
    fill_solid(s->leds, LED_COUNT, s->color);
}

void test(Strip *s) {
    fill_solid(s->leds, LED_COUNT, CRGB::White);
    s->leds[0] = CRGB::Green;
    s->leds[LED_COUNT - 1] = CRGB::Red;
}

void glitter(Strip *s) {
    fadeToBlackBy(s->leds, LED_COUNT, 20);
    addGlitter(s, 80);
}

void rainbow(Strip *s) {
    fill_rainbow(s->leds, LED_COUNT, s->hue, 7);
}

void rainbowWithGlitter(Strip *s) {
    rainbow(s);
    addGlitter(s, 80);
}

void addGlitter(Strip *s, fract8 chanceOfGlitter) {
    if (random8() < chanceOfGlitter) {
        s->leds[random16(LED_COUNT)] += CRGB::White;
    }
}

// Send the pixels one or the other direction down the line.
void lineit(Strip *s, int thisdir) {
    if (thisdir == 0) {
        for (int i = LED_COUNT - 1; i > 0; i--) {
            s->leds[i] = s->leds[i - 1];
        }
    } else {
        for (int i = 0; i < LED_COUNT - 1; i++) {
            s->leds[i] = s->leds[i + 1];
        }
    }
}


// Shifting pixels from the center to the left and right.
void waveit(Strip *s) {
    // Move to the right.
    for (int i = LED_COUNT - 1; i > LED_COUNT / 2; i--) {
        s->leds[i] = s->leds[i - 1];
    }

    // Move to the left.
    for (int i = 0; i < LED_COUNT / 2; i++) {
        s->leds[i] = s->leds[i + 1];
    }
}


void cycle(Strip *s) {
    fill_solid(s->leds, LED_COUNT, CHSV(s->hue, 200, 255));
}

void confetti(Strip *s) {
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(s->leds, LED_COUNT, 10);
    int pos = random16(LED_COUNT);
    s->leds[pos] += CHSV(s->hue + random8(64), 200, 255);
}

void sinelon(Strip *s) {
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(s->leds, LED_COUNT, 20);
    int pos = beatsin16(13, 0, LED_COUNT - 1);
    s->leds[pos] += CHSV(s->hue, 255, 192);
}

void bpm(Strip *s) {
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < LED_COUNT; i++) { //9948
        s->leds[i] = ColorFromPalette(s->currentPalette, s->hue + (i * 2), beat - s->hue + (i * 10));
    }
}

void juggle(Strip *s) {
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(s->leds, LED_COUNT, 20);
    byte dothue = 0;
    for (int i = 0; i < 8; i++) {
        s->leds[beatsin16(i + 7, 0, LED_COUNT - 1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

void gradient(Strip *s) {
    CRGB entries[16] = s->currentPalette.entries;
    fill_gradient_RGB(s->leds, LED_COUNT, entries[0], entries[5], entries[10], entries[15]);
}


int shift(int v, int min, int max, int d) {
    if (d > 0) {
        return v + 1 >= max ? v : v + 1;
    } else if (d < 0) {
        return v - 1 <= min ? v : v - 1;
    }
    return v;
}

#define C1 CRGB::Yellow
#define C2 CRGB::Cyan

void blend(Strip *s, CRGB c, int f, int t) {
    for (int i = f; i < t; i++) {
        s->leds[i] = blend(s->leds[i], c, 8);
    }
}

void vibrancy(Strip *s) {
    static int b1 = LED_COUNT * 0.3;
    static int b2 = LED_COUNT * 0.7;

    EVERY_X_MILLIS(s->t2, 2000)
        b1 = shift(b1, 6, b2, random(3) - 1);
        b2 = shift(b2, b1, LED_COUNT - 6, random(3) - 1);
    }

    blend(s, C1, 0, b1);
    blend(s, C2, b2, LED_COUNT);
    blend(s, s->color, b1, b2);
}


// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride(Strip *s) {
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < LED_COUNT; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t) b16 * (uint32_t) b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t) bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (LED_COUNT - 1) - pixelnumber;

        nblend(s->leds[pixelnumber], newcolor, 64);
    }
}