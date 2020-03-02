
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
