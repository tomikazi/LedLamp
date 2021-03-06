
void dotBeat(Strip *s) {
    uint8_t bpm = 30;
    uint8_t fadeval = 224;                                        // Trail behind the LED's. Lower => faster fade.

    uint8_t inner = beatsin8(bpm, s->count / 4, s->count / 4 * 3);    // Move 1/4 to 3/4
    uint8_t outer = beatsin8(bpm, 0, s->count - 1);               // Move entire length
    uint8_t middle = beatsin8(bpm, s->count / 3, s->count / 3 * 2);   // Move 1/3 to 2/3

    s->leds[middle] = CRGB::Purple;
    s->leds[inner] = CRGB::Blue;
    s->leds[outer] = CRGB::Aqua;

    // Fade the entire array. Or for just a few LED's, use  nscale8(&leds[2], 5, fadeval);
    nscale8(s->leds, s->count, fadeval);
}
