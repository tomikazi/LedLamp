
// A 'Matrix' like display using sampleavg for brightness. Also add glitter based on peaks (and not sampleavg).
void matrix(Strip *s) {

    static uint8_t thishue = 0;

    s->leds[0] = ColorFromPalette(s->currentPalette, thishue++, sampleavg * 2, s->currentBlending);

    for (int i = LED_COUNT - 1; i > 0; i--)
        s->leds[i] = s->leds[i - 1];

    addGlitter(s, sampleavg / 2);
}