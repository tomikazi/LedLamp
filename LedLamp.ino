#include <ESPGizmoDefault.h>
#include <WiFiUDP.h>
#include <FS.h>

#include <FastLED.h>

#include <WebSocketsServer.h>

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2019.07.02.001"

#define STATE      "state"

#define FRONT_PIN       4
#define BACK_PIN        5

#define LED_COUNT               60
#define LED_TYPE                WS2812B
#define COLOR_ORDER             GRB
#define BRIGHTNESS              96
#define FRAMES_PER_SECOND       60

WiFiUDP UDP;

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
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;
    TBlendType currentBlending;
    bool random;
    byte *data;
};

byte frontData[LED_COUNT];
byte backData[LED_COUNT];

Strip front = {
        .name = "front", .on = true, .color = CRGB::Red, .brightness = BRIGHTNESS,
        .leds = &frontLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL,
        .currentPalette = CRGBPalette16(PartyColors_p), .targetPalette = CRGBPalette16(PartyColors_p),
        .currentBlending = LINEARBLEND,
        .random = false, .data = frontData
};
Strip back = {
        .name = "back", .on = true, .color = CRGB::Green, .brightness = BRIGHTNESS,
        .leds = &backLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL,
        .currentPalette = CRGBPalette16(PartyColors_p), .targetPalette = CRGBPalette16(PartyColors_p),
        .currentBlending = LINEARBLEND,
        .random = false, .data = backData
};

static WebSocketsServer wsServer(81);

#define MAX_SAMPLES     8
typedef struct {
    uint16_t count;
    uint16_t data[MAX_SAMPLES];
} Sample;

// For moving average for the N most recent (already normalized) samples
#define N   64
uint32_t mat = 0;

Sample sample;


void setup() {
    gizmo.beginSetup(LED_LIGHTS, SW_VERSION, "gizmo123");
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
    UDP.begin(7001);

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
    saveState();
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
    loadState();
}

void handleMicData() {
    if (UDP.parsePacket()) {
        UDP.read((char *) &sample, sizeof(sample));
        for (int i = 0; i < sample.count; i++) {
            mat = mat + sample.data[i] - (mat >> 6);
            Serial.printf("100, %d, %d\n", sample.data[i], mat >> 6);
        }
    }
}

void loop() {
    if (gizmo.isNetworkAvailable(finishWiFiConnect)) {
        wsServer.loop();
        handleMicData();
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

void loadState() {
    File f = SPIFFS.open(STATE, "r");
    if (f) {
        loadStripState(f, &front);
        loadStripState(f, &back);
        f.close();
    }
}

void loadStripState(File f, Strip *s) {
    char field[32];
    int l = f.readBytesUntil('|', field, 32);
    field[l] = '\0';
    s->on = !strcmp(field, "on");

    l = f.readBytesUntil('|', field, 32);
    field[l] = '\0';
    processColor(field, s);

    l = f.readBytesUntil('|', field, 7);
    field[l] = '\0';
    s->brightness = atoi(field);

    l = f.readBytesUntil('\n', field, 32);
    field[l] = '\0';
    s->pattern = !strcmp(field, "none") ? NULL : findPattern(field);
}

void saveState() {
    File f = SPIFFS.open(STATE, "w");
    if (f) {
        f.printf("%s|%d,%d,%d|%d|%s\n", front.on ? "on" : "off",
                 front.color.red, front.color.green, front.color.blue, front.brightness,
                 front.pattern ? front.pattern->name : "none");
        f.printf("%s|%d,%d,%d|%d|%s\n", back.on ? "on" : "off",
                 back.color.red, back.color.green, back.color.blue, back.brightness,
                 back.pattern ? back.pattern->name : "none");
        f.close();
    }
}

// LED Patterns
#include "simple.h"
#include "fire.h"
#include "noise.h"
#include "plasma.h"
#include "blendwave.h"
#include "dotBeat.h"

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

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

Pattern *findPattern(const char *name) {
    int i = 0;
    while (strcmp(patterns[i].name, name) && strcmp(patterns[i].name, "test")) {
        i++;
    }
    return &patterns[i];
}

Pattern *randomPattern() {
    return &patterns[random8(ARRAY_SIZE(patterns) - 1)];
}
