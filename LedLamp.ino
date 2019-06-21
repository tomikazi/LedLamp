#include <ESPGizmoDefault.h>
#include <FS.h>

#include <FastLED.h>

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2019.06.21.001"

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
    const char *name;
    Renderer renderer;
    uint32_t pause;
    Pattern *next;
} Pattern;

struct StripRec {
    const char *name;
    bool on;
    CRGB color;
    uint8_t brightness;
    CRGB *leds;
    Pattern *pattern;
    uint8_t hue;
    CLEDController *ctl;
    bool random;
    byte *data;
};

byte frontData[LED_COUNT];
byte backData[LED_COUNT];

Strip front = {
        .name = "front", .on = true, .color = CRGB::Red, .brightness = BRIGHTNESS,
        .leds = &frontLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL, .random = false,
        .data = frontData
};
Strip back = {
        .name = "back", .on = true, .color = CRGB::Green, .brightness = BRIGHTNESS,
        .leds = &backLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL, .random = false,
        .data = backData
};


void setup() {
    gizmo.beginSetup(LED_LIGHTS, SW_VERSION, "");
    gizmo.setUpdateURL(SW_UPDATE_URL);

    gizmo.setCallback(mqttCallback);
    gizmo.addTopic("%s/all");
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
    gizmo.httpServer()->on("/cmd", handleCommand);
    gizmo.httpServer()->serveStatic("/", SPIFFS, "/", "max-age=86400");

    setupLED();
    gizmo.endSetup();
}

void setupLED() {
    front.ctl = &FastLED.addLeds<LED_TYPE, FRONT_PIN, COLOR_ORDER>(frontLeds, LED_COUNT).setCorrection(TypicalLEDStrip);
    fadeToBlackBy(frontLeds, LED_COUNT, 255);
    front.ctl->showLeds(front.brightness);
    front.pattern = findPattern("glitter");

    back.ctl = &FastLED.addLeds<LED_TYPE, BACK_PIN, COLOR_ORDER>(backLeds, LED_COUNT).setCorrection(TypicalLEDStrip);
    fadeToBlackBy(backLeds, LED_COUNT, 255);
    back.ctl->showLeds(back.brightness);
    back.pattern = findPattern("glitter");
}

// Command processors
void processOnOff(char *value, Strip *strip) {
    strip->on = !strcmp(value, "on");
}

CRGB colorFromCSV(char *csv) {
    char tmp[32];
    strncpy(tmp, csv, 32);
    uint8_t r, g, b;
    char *p = strtok(tmp, ",");
    r = p ? atoi(p) : 0;
    p = strtok(NULL, ",");
    g = p ? atoi(p) : 0;
    p = strtok(NULL, ",");
    b = p ? atoi(p) : 0;
    return CRGB(r, g, b);
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
    strip->on = true;
    strip->random = !strcmp(value, "random");
    if (strip->random) {
        strip->pattern = randomPattern();
    } else {
        strip->pattern = findPattern(value);
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


// MQTT Callback
void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    char value[64];
    value[0] = '\0';
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

void handleLEDs(Strip *strip) {
    if (strip->on && strip->pattern) {
        strip->pattern->renderer(strip);
        strip->ctl->showLeds(strip->brightness);

        // Sets initial timing only. Changes here don't do anything
        EVERY_N_MILLIS_I(ticktimer, 20)
        {
            strip->hue++; // slowly cycle the "base color" through the rainbow
            ticktimer.setPeriod(strip->pattern->pause);
        }

        EVERY_N_SECONDS(60)
        {
            if (strip->random) {
                strip->pattern = randomPattern();
            }
        }

    } else {
        fill_solid(strip->leds, LED_COUNT, strip->color);
        strip->ctl->showLeds(strip->on ? strip->brightness : 0);
    }
}

void finishWiFiConnect() {
    Serial.printf("%s is ready\n", LED_LIGHTS);
    front.pattern = findPattern("confetti");
    back.pattern = findPattern("confetti");
}

void loop() {
    gizmo.isNetworkAvailable(finishWiFiConnect);
    handleLEDs(&front);
    handleLEDs(&back);
}

void handleRoot() {
    File f = SPIFFS.open("/index.html", "r");
    gizmo.httpServer()->streamFile(f, "text/html");
    f.close();
}

#define STRIP_STATUS "\"%s\": {\"on\": %s,\"rgb\": \"%d,%d,%d\",\"brightness\": %d,\"effect\": \"%s\"}"

char *stripStatus(Strip *s) {
    static char html[128];
    snprintf(html, 127, STRIP_STATUS, s->name, s->on ? "true" : "false",
             s->color.red, s->color.green, s->color.blue, s->brightness,
             s->pattern ? s->pattern->name : "none");
    return html;
}

void handleCommand() {
    char t[64], m[64];
    strncpy(t, gizmo.httpServer()->arg("t").c_str(), 63);
    strncpy(m, gizmo.httpServer()->arg("m").c_str(), 63);
    if (strcmp(t, "get")) {
        mqttCallback(t, (uint8_t *) m, strlen(m));
    }

    gizmo.httpServer()->setContentLength(CONTENT_LENGTH_UNKNOWN);
    gizmo.httpServer()->send(200, "application/json", "{");
    gizmo.httpServer()->sendContent(stripStatus(&front));
    gizmo.httpServer()->sendContent(",");
    gizmo.httpServer()->sendContent(stripStatus(&back));
    gizmo.httpServer()->sendContent("}");
}

// LED Patterns
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


// Setup a catalog of the different patterns.
Pattern patterns[] = {
        Pattern{.name = "glitter", .renderer = glitter, .pause = 20, .next = NULL },
        Pattern{.name = "confetti", .renderer = confetti, .pause = 20, .next = NULL },
        Pattern{.name = "cycle", .renderer = cycle, .pause = 200, .next = NULL },
        Pattern{.name = "rainbow", .renderer = rainbow, .pause = 20, .next = NULL },
        Pattern{.name = "rainbowWithGlitter", .renderer = rainbowWithGlitter, .pause = 20, .next = NULL },
        Pattern{.name = "sinelon", .renderer = sinelon, .pause = 20, .next = NULL },
        Pattern{.name = "juggle", .renderer = juggle, .pause = 20, .next = NULL },
        Pattern{.name = "bpm", .renderer = bpm, .pause = 20, .next = NULL },
        Pattern{.name = "fire", .renderer = fire, .pause = 20, .next = NULL },

        Pattern{.name = "test", .renderer = test, .pause = 20, .next = NULL }
};

Pattern *findPattern(const char *name) {
    int i = 0;
    while (strcmp(patterns[i].name, name) && strcmp(patterns[i].name, "test")) {
        i++;
    }
    return &patterns[i];
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

Pattern *randomPattern() {
    return &patterns[random8(ARRAY_SIZE(patterns) - 1)];
}
