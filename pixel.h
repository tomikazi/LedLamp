
void pixel(Strip *s) {
    // Persistent local variable
    static uint16_t currLED;

    currLED = (currLED + 1) % LED_COUNT;

    // Colour of the LED will be based on oldsample, while brightness is based on sampleavg.
    CRGB newcolour = ColorFromPalette(s->currentPalette, oldsample, oldsample, s->currentBlending);
    nblend(s->leds[currLED], newcolour, 192);
}
