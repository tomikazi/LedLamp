// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING      75

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING    100

void fire(Strip *s) {
    EVERY_N_MILLISECONDS(10)
    {
        // Step 1.  Cool down every cell a little
        for (int i = 0; i < LED_COUNT; i++) {
            s->data[i] = qsub8(s->data[i], random8(0, ((COOLING * 10) / LED_COUNT) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int k = LED_COUNT - 1; k >= 2; k--) {
            s->data[k] = (s->data[k - 1] + s->data[k - 2] + s->data[k - 2]) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        if (random8() < SPARKING) {
            int y = random8(7);
            s->data[y] = qadd8(s->data[y], random8(160, 255));
        }

        // Step 4.  Map from heat cells to LED colors
        for (int j = 0; j < LED_COUNT; j++) {
            s->leds[j] = HeatColor(s->data[j]);
        }
    }
}
