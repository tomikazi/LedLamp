#include <ESPGizmoDefault.h>
#include <NeoPixelFader.h>
#include <FS.h>

#include <FastLED.h>

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2019.06.15.001"

#define LED_DATA   "cfg/led"

#define FRONT_PIN       4
#define BACK_PIN        5

#define LED_COUNT               60
#define LED_TYPE                WS2812B
#define COLOR_ORDER             GRB
#define BRIGHTNESS              96
#define FRAMES_PER_SECOND       60

CRGB frontLeds[LED_COUNT];
CRGB backLeds[LED_COUNT];

typedef struct StripRec Strip;

// Pattern renderer function type.
typedef void (*Renderer)(Strip *);

// Pattern record
typedef struct Pattern {
    char        *name;
    Renderer    renderer;
    Pattern     *next;
} Pattern;

struct StripRec {
    char    *name;
    bool    on;
    CRGB    color;
    uint8_t brightness;
    CRGB    *leds;
    Pattern *pattern;
    uint8_t hue;
    CLEDController *ctl;
};

Strip front = { .name = "front", .on = true, .color = CRGB::White, .brightness = BRIGHTNESS, .leds = &frontLeds[0] };
Strip back = { .name = "back", .on = true, .color = CRGB::White, .brightness = BRIGHTNESS, .leds = &backLeds[0] };


void setup() {
    gizmo.beginSetup(LED_LIGHTS, SW_VERSION, PASSKEY);
    gizmo.setUpdateURL(SW_UPDATE_URL);

    gizmo.setCallback(mqttCallback);
    gizmo.addTopic("%s");
    gizmo.addTopic("%s/all/rgb");
    gizmo.addTopic("%s/all/brightness");
    gizmo.addTopic("%s/all/effect");

    gizmo.addTopic("%s/front");
    gizmo.addTopic("%s/front/rgb");
    gizmo.addTopic("%s/front/brightness");
    gizmo.addTopic("%s/front/effect");

    gizmo.addTopic("%s/back");
    gizmo.addTopic("%s/back/rgb");
    gizmo.addTopic("%s/back/brightness");
    gizmo.addTopic("%s/back/effect");

    gizmo.httpServer()->on("/", handleRoot);

    setupLED();
    gizmo.endSetup();
}

void setupLED() {
    front.ctl = &FastLED.addLeds<LED_TYPE, FRONT_PIN, COLOR_ORDER>(frontLeds, LED_COUNT).setCorrection(TypicalLEDStrip);
    fadeToBlackBy(frontLeds, LED_COUNT, 255);
    front.ctl->showLeds(front.brightness);
    front.pattern = findPattern("confetti");

    back.ctl = &FastLED.addLeds<LED_TYPE, BACK_PIN, COLOR_ORDER>(backLeds, LED_COUNT).setCorrection(TypicalLEDStrip);
    fadeToBlackBy(backLeds, LED_COUNT, 255);
    back.ctl->showLeds(back.brightness);
    back.pattern = findPattern("glitter");
}

// TODO: use ESPGizmo to do this
void publishState(const char *topic, const char *value, Strip *strip) {
    char stateTopic[64];
    snprintf(stateTopic, 64, "%s/%s%s", gizmo.getTopicPrefix(), strip->name, topic);
    gizmo.publish(stateTopic, (char *) value);
}

void processOnOff(char *value, Strip *strip) {
    strip->on = !strcmp(value, "on");
    if (strip->on && strip->brightness == 0) {
        strip->brightness = 64;
    }
}

void processColor(char *value, Strip *strip) {
    strip->on = true;
    strip->color = colorFromCSV(value);
    strip->pattern = NULL;
}

void processBrightness(char *value, Strip *strip) {
    strip->brightness = atoi(value);
    strip->on = strip->brightness != 0;
}

void processEffect(char *value, Strip *strip) {
    strip->pattern = findPattern(value);
}

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    char value[64];
    value[0] = NULL;
    strncat(value, (char *) payload, length);

    Serial.printf("%s: %s\n", topic, value);

    if (strstr(topic, "/all")) {
        processCallback(topic, value, &front);
        processCallback(topic, value, &back);
    } else if (strstr(topic, "/front")) {
        processCallback(topic, value, &front);
    } else if (strstr(topic, "/back")) {
        processCallback(topic, value, &back);
    } else {
        gizmo.handleMQTTMessage(topic, value);
    }
}

void processCallback(char *topic, char *value, Strip *strip) {
    if (strstr(topic, "/rgb")) {
        processColor(value, strip);
    } else if (strstr(topic, "/brightness")) {
        processBrightness(value, strip);
    } else if (strstr(topic, "/effect")) {
        processEffect(value, strip);
    } else {
        processOnOff(value, strip);
    }
}

CRGB colorFromCSV(char *csv) {
    uint8_t r, g, b;
    char *p = strtok(csv, ",");
    r = p ? atoi(p) : 0;
    p = strtok(NULL, ",");
    g = p ? atoi(p) : 0;
    p = strtok(NULL, ",");
    b = p ? atoi(p) : 0;
    return CRGB(r, g, b);
}

void handleLEDs(Strip *strip) {
    if (strip->pattern) {
        strip->pattern->renderer(strip);
        strip->ctl->showLeds(strip->brightness);

        EVERY_N_MILLISECONDS(20)
            { strip->hue++; } // slowly cycle the "base color" through the rainbow
        //    EVERY_N_SECONDS(15)
        //    { nextPattern(); } // change patterns periodically

    } else {
        fill_solid(strip->leds, LED_COUNT, strip->color);
        strip->ctl->showLeds(strip->on ? strip->brightness : 0);
    }
}

void finishWiFiConnect() {
    Serial.printf("%s is ready\n", LED_LIGHTS);
}

void loop() {
    gizmo.isNetworkAvailable(finishWiFiConnect);
    handleLEDs(&front);
    handleLEDs(&back);
}

void handleRoot() {
    gizmo.httpServer()->send(200, "text/html", "LedLamp!");
}

void switchOn(boolean o, Strip *strip) {
    strip->on = o;
}

void setBrightness(uint8_t b, Strip *strip) {
    strip->brightness = b;
}



// LED Patterns
void test(Strip *s) {
    fill_solid(s->leds, LED_COUNT, CRGB::White);
    s->leds[0] = CRGB::Green;
    s->leds[LED_COUNT-1] = CRGB::Red;
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
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < LED_COUNT; i++) { //9948
        s->leds[i] = ColorFromPalette(palette, s->hue + (i * 2), beat - s->hue + (i * 10));
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


// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING  75

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 100

void fire(Strip *s) {
    // Array of temperature readings at each simulation cell
    static byte heat[LED_COUNT];

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < LED_COUNT; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / LED_COUNT) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = LED_COUNT - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < SPARKING) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int j = 0; j < LED_COUNT; j++) {
        s->leds[j] = HeatColor(heat[j]);
    }
}




// Setup a catalog of the different patterns.
Pattern patterns[] = {
        Pattern{ .name = "glitter", .renderer = glitter },
        Pattern{ .name = "confetti", .renderer = confetti },
        Pattern{ .name = "rainbow", .renderer = rainbow },
        Pattern{ .name = "rainbowWithGlitter", .renderer = rainbowWithGlitter },
        Pattern{ .name = "sinelon", .renderer = sinelon },
        Pattern{ .name = "juggle", .renderer = juggle },
        Pattern{ .name = "bpm", .renderer = bpm },
        Pattern{ .name = "fire", .renderer = fire },

        Pattern{ .name = "test", .renderer = test }
};

Pattern *findPattern(char *name) {
    int i = 0;
    while (strcmp(patterns[i].name, name) && strcmp(patterns[i].name, "test")) {
        i++;
    }
    return &patterns[i];
}
