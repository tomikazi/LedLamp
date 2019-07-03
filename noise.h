
#define SCALE 30

void noise(Strip *s) {
    EVERY_N_MILLISECONDS(10) {
        // Blend towards the target palette over 48 iterations
        nblendPaletteTowardPalette(s->currentPalette, s->targetPalette, 48);
        fillnoise8(s);
    }

    // Change the target palette to a random one every 5 seconds.
    EVERY_N_SECONDS(5) {
        s->targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)),
                                         CHSV(random8(), 255, random8(128,255)),
                                         CHSV(random8(), 192, random8(128,255)),
                                         CHSV(random8(), 255, random8(128,255)));
    }
} // loop()

void fillnoise8(Strip *s) {
    // A random number for our noise generator.
    static uint16_t dist;

    // Just ONE loop to fill up the LED array as all of the pixels change.
    for(int i = 0; i < LED_COUNT; i++) {
        // Get a value from the noise function. I'm using both x and y axis.
        uint8_t index = inoise8(i*SCALE, dist+i*SCALE);
        // With that value, look up the 8 bit colour palette value and assign it to the current LED.
        s->leds[i] = ColorFromPalette(s->currentPalette, index, 255, s->currentBlending);
    }
    // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
    dist += beatsin8(10,1, 4);
}
