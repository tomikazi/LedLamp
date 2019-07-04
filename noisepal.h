
// Create fire based on noise and sampleavg.
void noisepal(Strip *s) {

// Local definitions
#define xscale 20                                                             // How far apart they are
#define yscale 3                                                              // How fast they move

    // Current colour lookup value.
    uint16_t index = 0;

    // Fire palette definition. Lower value = darker.
    s->currentPalette = CRGBPalette16(
            CHSV(0, 255, 2), CHSV(0, 255, 4), CHSV(0, 255, 8), CHSV(0, 255, 8),
            CHSV(0, 255, 16), CRGB::Red, CRGB::Red, CRGB::Red,
            CRGB::DarkOrange, CRGB::DarkOrange, CRGB::Orange, CRGB::Orange,
            CRGB::Yellow, CRGB::Orange, CRGB::Yellow, CRGB::Yellow);

    for (int i = 0; i < LED_COUNT; i++) {
        // X location is constant, but we move along the Y at the rate of millis(). By Andrew Tuline.
        index = inoise8(i * xscale, millis() * yscale * LED_COUNT / 255);

        // Now we need to scale index so that it gets blacker as we get close to one of the ends
//        index = (255 - *i * 128 / LED_COUNT);

        // Now we need to scale index so that it gets blacker as we get close to one of the ends
        index = (255 - i * 256 / LED_COUNT) * index / 128;

        // With that value, look up the 8 bit colour palette value and assign it to the current LED.
        s->leds[LED_COUNT / 2 - i / 2 + 1] = ColorFromPalette(s->currentPalette, index, sampleavg, NOBLEND);
        s->leds[LED_COUNT / 2 + i / 2 - 1] = ColorFromPalette(s->currentPalette, index, sampleavg, NOBLEND);
    }
}
