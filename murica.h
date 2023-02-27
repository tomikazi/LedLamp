
void triWave(Strip *s, CRGB c1, CRGB c2, CRGB c3, uint8_t decay, uint8_t bpm) {
    uint8_t n = s->count - 1;
    uint8_t w1 = beatsin8(bpm, 0, n, 0, 0);
    uint8_t w2 = beatsin8(bpm, 0, n, 0, 85);
    uint8_t w3 = beatsin8(bpm, 0, n, 0, 170);

    s->leds[w1] = c1;
    s->leds[w2] = c2;
    s->leds[w3] = c3;

    fadeToBlackBy(s->leds, s->count, decay);
}

void murica(Strip *s) {
    triWave(s, CRGB::Blue, CRGB::Red, CRGB::White, 4, 8);
}
