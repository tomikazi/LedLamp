#include <ESPGizmoDefault.h>
#include <FS.h>

#include <FastLED.h>

#include <WebSocketsServer.h>

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2019.06.29.001"

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

static WebSocketsServer wsServer(81);

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

    setupWebSocket();

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

void setupWebSocket() {
    wsServer.begin();
    wsServer.onEvent(webSocketEvent);
    Serial.println("WebSocket server setup.");
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

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected.\n", num);
            break;
        case WStype_CONNECTED:
            Serial.printf("[%u] Connected.\n", num);
            break;
        case WStype_TEXT:
            char cmd[128];
            cmd[0] = '\0';
            strncat(cmd, (char *) payload, length);
            handleWsCommand(cmd);
            break;

        default:
            Serial.printf("Unhandled message type\n");
            break;
    }
}

void handleWsCommand(char *cmd) {
    char *t = strtok(cmd, "&");
    char *m = strtok(NULL, "&");

    if (strcmp(t, "get")) {
        mqttCallback(t, (uint8_t *) m, strlen(m));
    }
    broadcastState();
}

void broadcastState() {
    char state[512], f[128], b[128];
    state[0] = '\0';
    snprintf(state, 511, "{%s,%s,\"version\":\"" SW_VERSION "\"}", stripStatus(f, &front), stripStatus(b, &back));
    wsServer.broadcastTXT(state);
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
    if (gizmo.isNetworkAvailable(finishWiFiConnect)) {
        wsServer.loop();
    }
    handleLEDs(&front);
    handleLEDs(&back);
}

void handleRoot() {
    File f = SPIFFS.open("/index.html", "r");
    gizmo.httpServer()->streamFile(f, "text/html");
    f.close();
}

#define STRIP_STATUS "\"%s\": {\"on\": %s,\"rgb\": \"%d,%d,%d\",\"brightness\": %d,\"effect\": \"%s\"}"

char *stripStatus(char *html, Strip *s) {
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

    static char f[128];
    static char b[128];

    gizmo.httpServer()->setContentLength(CONTENT_LENGTH_UNKNOWN);
    gizmo.httpServer()->send(200, "application/json", "{");
    gizmo.httpServer()->sendContent(stripStatus(f, &front));
    gizmo.httpServer()->sendContent(",");
    gizmo.httpServer()->sendContent(stripStatus(b, &back));
    gizmo.httpServer()->sendContent(",\"version\":\"" SW_VERSION "\"");
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


CRGBPalette16 currentPalette(CRGB::Black);
CRGBPalette16 targetPalette(OceanColors_p);

void noise(Strip *s) {
    EVERY_N_MILLIS(10) {
        // Blend towards the target palette over 48 iterations
        nblendPaletteTowardPalette(currentPalette, targetPalette, 48);
        fillnoise8(s);
    }

    // Change the target palette to a random one every 5 seconds.
    EVERY_N_SECONDS(5) {
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)),
                CHSV(random8(), 255, random8(128,255)),
                CHSV(random8(), 192, random8(128,255)),
                CHSV(random8(), 255, random8(128,255)));
    }
} // loop()

#define SCALE 30

void fillnoise8(Strip *s) {
    // A random number for our noise generator.
    static uint16_t dist;

    // Just ONE loop to fill up the LED array as all of the pixels change.
    for(int i = 0; i < LED_COUNT; i++) {
        // Get a value from the noise function. I'm using both x and y axis.
        uint8_t index = inoise8(i*SCALE, dist+i*SCALE);
        // With that value, look up the 8 bit colour palette value and assign it to the current LED.
        s->leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
    }
    // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
    dist += beatsin8(10,1, 4);
}

void blendwave(Strip *s) {
    uint8_t speed = beatsin8(6,0,255);

    CRGB clr1 = blend(CHSV(beatsin8(3,0,255),255,255), CHSV(beatsin8(4,0,255),255,255), speed);
    CRGB clr2 = blend(CHSV(beatsin8(4,0,255),255,255), CHSV(beatsin8(3,0,255),255,255), speed);

    uint8_t loc1 = beatsin8(10,0,LED_COUNT-1);

    fill_gradient_RGB(s->leds, 0, clr2, loc1, clr1);
    fill_gradient_RGB(s->leds, loc1, clr2, LED_COUNT-1, clr1);
}

void dotBeat(Strip *s) {
    uint8_t bpm = 30;
    uint8_t fadeval = 224;                                        // Trail behind the LED's. Lower => faster fade.

    uint8_t inner = beatsin8(bpm, LED_COUNT / 4, LED_COUNT / 4 * 3);    // Move 1/4 to 3/4
    uint8_t outer = beatsin8(bpm, 0, LED_COUNT - 1);               // Move entire length
    uint8_t middle = beatsin8(bpm, LED_COUNT / 3, LED_COUNT / 3 * 2);   // Move 1/3 to 2/3

    s->leds[middle] = CRGB::Purple;
    s->leds[inner] = CRGB::Blue;
    s->leds[outer] = CRGB::Aqua;

    // Fade the entire array. Or for just a few LED's, use  nscale8(&leds[2], 5, fadeval);
    nscale8(s->leds, LED_COUNT, fadeval);
}



void plasma(Strip *s) {
    EVERY_N_MILLISECONDS(50) {
        doPlasma(s);
    }

    EVERY_N_MILLISECONDS(100) {
        uint8_t maxChanges = 24;
        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);   // AWESOME palette blending capability.
    }

    // Change the target palette to a random one every 5 seconds.
    EVERY_N_SECONDS(5) {
        // You can use this as a baseline colour if you want similar hues in the next line.
        uint8_t baseC = random8();
        targetPalette = CRGBPalette16(CHSV(baseC+random8(32), 192, random8(128,255)),
                CHSV(baseC+random8(32), 255, random8(128,255)),
                CHSV(baseC+random8(32), 192, random8(128,255)),
                CHSV(baseC+random8(32), 255, random8(128,255)));
    }
}

// Use qsuba for smooth pixel colouring and qsubd for non-smooth pixel colouring
#define qsubd(x, b)  ((x>b)?b:0)                              // Digital unsigned subtraction macro. if result <0, then => 0. Otherwise, take on fixed value.
#define qsuba(x, b)  ((x>b)?x-b:0)                            // Analog Unsigned subtraction macro. if result <0, then => 0

void doPlasma(Strip *s) {
    // Setting phase change for a couple of waves.
    int thisPhase = beatsin8(6, -64, 64);
    int thatPhase = beatsin8(7, -64, 64);

    // For each of the LED's in the strand, set a brightness based on a wave as follows:
    for (int k = 0; k < LED_COUNT; k++) {
        // Create a wave and add a phase change and add another wave with its own phase change.. Hey, you can even change the frequencies if you wish.
        int colorIndex = cubicwave8((k * 23) + thisPhase) / 2 + cos8((k * 15) + thatPhase) / 2;
        // qsub gives it a bit of 'black' dead space by setting sets a minimum value. If colorIndex < current value of beatsin8(), then bright = 0. Otherwise, bright = colorIndex..
        int thisBright = qsuba(colorIndex, beatsin8(7, 0, 96));

        // Let's now add the foreground colour.
        s->leds[k] = ColorFromPalette(currentPalette, colorIndex, thisBright, LINEARBLEND);
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
        Pattern{.name = "noise", .renderer = noise, .pause = 20, .next = NULL },
        Pattern{.name = "blendwave", .renderer = blendwave, .pause = 20, .next = NULL },
        Pattern{.name = "dotbeat", .renderer = dotBeat, .pause = 20, .next = NULL },
        Pattern{.name = "plasma", .renderer = plasma, .pause = 20, .next = NULL },

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
