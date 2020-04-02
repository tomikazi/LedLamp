#include <ESPGizmoDefault.h>
#include <WiFiUDP.h>
#include <FS.h>

#include <FastLED.h>

#include <WebSocketsServer.h>

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2020.04.01.001"

#define STATE      "/cfg/state"
#define FAVS       "/cfg/favs"

#define FRONT_PIN       4
#define BACK_PIN        5

#define MIC_PIN         A0

#define LED_COUNT               60
#define LED_TYPE                WS2812B
#define COLOR_ORDER             GRB
#define BRIGHTNESS              96
#define FRAMES_PER_SECOND       60

#define BUDDY_PORT  7001
WiFiUDP buddy;

#define PEER_PORT  7003
WiFiUDP peer;

#define GROUP_PORT  7002
WiFiUDP group;
IPAddress groupIp(224, 0, 42, 69);

CRGB frontLeds[LED_COUNT];
CRGB backLeds[LED_COUNT];

typedef struct StripRec Strip;

// Pattern renderer function type.
typedef void (*Renderer)(Strip *);

// Pattern record
typedef struct Pattern {
    const char *name;
    Renderer renderer;
    uint32_t huePause;
    uint32_t renderPause;
    boolean soundReactive;
    boolean favorite;
} Pattern;

typedef enum {
    NOT_RANDOM,
    ALL,
    FAVORITES,
    SOUND_REACTIVE,
    NOT_SOUND_REACTIVE
} RandomMode;

uint8_t favCount = 0;

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
    RandomMode randomMode;
    uint32_t th, tb, tp, t0, t1, t2, t3, t4;
    byte *data;
};

byte frontData[LED_COUNT];
byte backData[LED_COUNT];

Strip front = {
        .name = "front", .on = true, .color = CRGB::Red, .brightness = BRIGHTNESS,
        .leds = &frontLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL,
        .currentPalette = CRGBPalette16(PartyColors_p), .targetPalette = CRGBPalette16(PartyColors_p),
        .currentBlending = LINEARBLEND, .randomMode = NOT_RANDOM,
        .th = 0, .tb = 0, .tp = 0, .t0 = 0, .t1 = 0, .t2 = 0, .t3 = 0, .t4 = 0, .data = frontData
};
Strip back = {
        .name = "back", .on = true, .color = CRGB::Green, .brightness = BRIGHTNESS,
        .leds = &backLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL,
        .currentPalette = CRGBPalette16(PartyColors_p), .targetPalette = CRGBPalette16(PartyColors_p),
        .currentBlending = LINEARBLEND, .randomMode = NOT_RANDOM,
        .th = 0, .tb = 0, .tp = 0, .t0 = 0, .t1 = 0, .t2 = 0, .t3 = 0, .t4 = 0, .data = backData
};

static WebSocketsServer wsServer(81);

typedef struct {
    uint16_t sampleavg;
    uint16_t samplepeak;
    uint16_t oldsample;
} MicSample;

#define PEER_TIMEOUT   20000

#define MAX_PEERS   5
typedef struct {
    uint32_t ip;
    uint32_t lastHeard;
    char name[64];
} Peer;

Peer peers[MAX_PEERS];
uint8_t master = 0;

boolean syncWithMaster = true;
boolean buddyAvailable = false;
boolean buddySilent = false;
uint32_t buddyTimestamp = 0;
uint32_t buddyIp = 0;

boolean hadBuddyAndPeer = false;

#define FRONT_CTX       0x01
#define BACK_CTX        0x10
#define ALL_CTX         FRONT_CTX | BACK_CTX

#define HELLO           001

#define SAMPLE_REQ      100
#define SAMPLE_ADV      101

#define SYNC_REQ        200
#define PATTERN         201
#define COLORS          202

#define MAX_CMD_DATA    64
typedef struct {
    uint32_t src;
    uint16_t ctx;
    uint16_t op;
    uint8_t data[MAX_CMD_DATA];
} Command;

// Sample average, max and peak detection
uint16_t sampleavg = 0;
uint16_t samplepeak = 0;
uint16_t oldsample = 0;

#define SAMPLE_TIMEOUT  1000
uint32_t lastSample = 0;

#define SLEEP_TIMEOUT   30*60000
uint32_t sleepTime = 0;
uint32_t sleepDimmer = 100;

void setup() {
    gizmo.beginSetup(LED_LIGHTS, SW_VERSION, "gizmo123");
    gizmo.setUpdateURL(SW_UPDATE_URL, onUpdate);

//    gizmo.debugEnabled = true;

    gizmo.httpServer()->on("/on", handleOn);
    gizmo.httpServer()->on("/off", handleOff);
    gizmo.setupWebRoot();
    setupWebSocket();

    gizmo.setCallback(mqttCallback);
    gizmo.addTopic("%s/sync");
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

    loadState();
    loadFavorites();
}

void setupWebSocket() {
    wsServer.begin();
    wsServer.onEvent(webSocketEvent);
    Serial.println("WebSocket server setup.");
}

void handleOn() {
    ESP8266WebServer *server = gizmo.httpServer();
    processCallback("/power", "on", &front);
    processCallback("/power", "on", &back);
    server->send(200, "text/plain", "on\n");
}

void handleOff() {
    ESP8266WebServer *server = gizmo.httpServer();
    processCallback("/power", "off", &front);
    processCallback("/power", "off", &back);
    server->send(200, "text/plain", "off\n");
}

// Command processors
void processOnOff(const char *value, Strip *strip) {
    strip->on = !strcmp(value, "on");
}


CRGB colorFromCSS(const char *css) {
    char tmp[32];
    strncpy(tmp, css, 32);
    uint8_t r, g, b;
    b = strtol(tmp + 5, NULL, 16);
    tmp[5] = '\0';
    g = strtol(tmp + 3, NULL, 16);
    tmp[3] = '\0';
    r = strtol(tmp + 1, NULL, 16);
    return CRGB(r, g, b);
}

CRGB colorFromCSV(const char *csv) {
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

void processColor(const char *value, Strip *strip, boolean turnOn) {
    strip->on = turnOn;
    if (value[0] == '#') {
        strip->color = colorFromCSS(value);
    } else {
        strip->color = colorFromCSV(value);
    }
    strip->pattern = findPattern("solid");
    strip->randomMode = NOT_RANDOM;
}

void processBrightness(const char *value, Strip *strip) {
    strip->brightness = atoi(value);
    strip->on = strip->brightness != 0;
}

RandomMode randomMode(const char *name) {
    return !strcmp(name, "random") ? ALL :
           !strcmp(name, "fav_random") ? FAVORITES :
           !strcmp(name, "sr_random") ? SOUND_REACTIVE :
           !strcmp(name, "nsr_random") ? NOT_SOUND_REACTIVE : NOT_RANDOM;
}

void processEffect(const char *value, Strip *strip, boolean turnOn) {
    strip->on = turnOn;
    strip->randomMode = randomMode(value);
    if (strip->randomMode == NOT_RANDOM) {
        strip->pattern = findPattern(value);
    } else {
        strip->pattern = randomPattern(strip);
    }
}

void processCallback(const char *topic, const char *value, Strip *strip) {
    if (strstr(topic, "/rgb")) {
        processColor(value, strip, true);
    } else if (strstr(topic, "/brightness")) {
        processBrightness(value, strip);
    } else if (strstr(topic, "/effect")) {
        processEffect(value, strip, true);
    } else if (strstr(topic, "/fav")) {
        strip->pattern->favorite = !strip->pattern->favorite;
        saveFavorites();
        if (!strip->pattern->favorite) {
            strip->pattern = randomPattern(strip);
        }

    } else {
        processOnOff(value, strip);
    }
    saveState();
    syncPattern(strip);
    syncColorSettings(strip);
    requestSamples();
    broadcastState(false);
}

void processSync(const char *value) {
    syncWithMaster = !strcmp(value, "on");
    saveState();
    determineMaster();
    if (syncWithMaster) {
        requestSync();
    }
}

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    char value[64];
    value[0] = '\0';
    strncat(value, (char *) payload, length);

    Serial.printf("%s: %s\n", topic, value);

    if (strstr(topic, "/all")) {
        processCallback(topic, value, &front);
        processCallback(topic, value, &back);
        gizmo.schedulePublish("%s/all/state", front.on || back.on ? "on" : "off");
    } else if (strstr(topic, "/front")) {
        processCallback(topic, value, &front);
    } else if (strstr(topic, "/back")) {
        processCallback(topic, value, &back);

    } else if (strstr(topic, "/sleep")) {
        sleepTime = !strcmp(value, "on") ? (millis() + SLEEP_TIMEOUT) : 0;

    } else if (strstr(topic, "/sync")) {
        processSync(value);

    } else {
        gizmo.handleMQTTMessage(topic, value);
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
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
    broadcastState(!strcmp(m, "all") || strstr(t, "/fav"));
}

#define STATUS \
    "{%s,%s,\"master\": \"%s\",\"masterIp\": \"%s\",\"isMaster\": %s,\"hasPotentialMaster\": %s," \
    "\"syncWithMaster\": %s,\"buddyAvailable\": %s,\"buddySilent\": %s,\"name\": \"%s\",%s" \
    "\"sleep\": %lu,\"version\":\"" SW_VERSION "\"}"

void broadcastState(boolean all) {
    char state[1024], f[128], b[128], favs[512];
    state[0] = '\0';
    favs[0] = '\0';

    if (all) {
        favorites(favs);
    }

    snprintf(state, 1023, STATUS, stripStatus(f, &front), stripStatus(b, &back),
             peers[master].name, IPAddress(peers[master].ip).toString().c_str(),
             isMaster(WiFi.localIP()) ? "true" : "false",
             hasPotentialMaster() ? "true" : "false",
             syncWithMaster ? "true" : "false",
             buddyAvailable ? "true" : "false",
             buddySilent ? "true" : "false",
             peers[0].name, favs, sleepTime ? (sleepTime - millis()) / 1000 : 0);
    wsServer.broadcastTXT(state);
}

void onUpdate() {
    // Suppress samples and switch to glitter
    broadcast({.src = (uint32_t) WiFi.localIP(), .ctx = ALL_CTX, .op = SAMPLE_REQ, .data = {[0] = 0}});
    processEffect((char *) "plasma", &front, true);
    processEffect((char *) "cycle", &back, true);
}

void unicast(uint32_t ip, uint16_t port, Command command) {
    peer.beginPacket(ip, port);
    peer.write((char *) &command, sizeof(command));
    peer.endPacket();
}

void broadcast(Command command) {
    group.beginPacketMulticast(groupIp, GROUP_PORT, WiFi.localIP());
    group.write((char *) &command, sizeof(command));
    group.endPacket();

    if (buddyAvailable && buddyIp) {
        unicast(buddyIp, PEER_PORT, command);
    }

    for (int i = 1; i < MAX_PEERS; i++) {
        if (peers[i].ip && peers[i].lastHeard) {
            unicast(peers[i].ip, PEER_PORT, command);
        }
    }
}

void requestSync() {
    broadcast({.src = peers[0].ip, .ctx = ALL_CTX, .op = SYNC_REQ, .data = {[0] = 0}});
};

void requestSamples() {
    boolean needSamples = (back.on && back.brightness && back.pattern && back.pattern->soundReactive) ||
                          (front.on && front.brightness && front.pattern && front.pattern->soundReactive);
    broadcast({.src = (uint32_t) WiFi.localIP(), .ctx = ALL_CTX, .op = SAMPLE_REQ, .data = {[0] = needSamples}});
}

void sayHello() {
    Command cmd = {.src = peers[0].ip, .ctx = ALL_CTX, .op = HELLO, .data = {[0] = 0}};
    strncat((char *) cmd.data, peers[0].name, MAX_CMD_DATA);
    broadcast(cmd);
}

void syncPattern(Strip *s) {
    uint16_t ctx = s == &front ? FRONT_CTX : BACK_CTX;
    Command cmd = {.src = (uint32_t) WiFi.localIP(), .ctx = ctx, .op = PATTERN, .data = {[0] = 0}};
    strncat((char *) cmd.data, s->pattern->name, MAX_CMD_DATA);
    broadcast(cmd);
}

void syncColorSettings(Strip *s) {
    uint16_t ctx = s == &front ? FRONT_CTX : BACK_CTX;
    Command cmd = {.src = (uint32_t) WiFi.localIP(), .ctx = ctx, .op = COLORS, .data = {[0] = 0}};
    cmd.data[0] = s->on;
    cmd.data[1] = s->hue;
    cmd.data[2] = s->brightness;
    cmd.data[3] = s->color.raw[0];
    cmd.data[4] = s->color.raw[1];
    cmd.data[5] = s->color.raw[2];

    for (int i = 0, di = 6; i < 16; i++, di += 3) {
        cmd.data[di + 0] = s->targetPalette.entries[i].raw[0];
        cmd.data[di + 1] = s->targetPalette.entries[i].raw[1];
        cmd.data[di + 2] = s->targetPalette.entries[i].raw[2];
    }

    cmd.data[32] = (uint8_t) sleepDimmer;

    broadcast(cmd);
}


void addPeer(uint32_t ip, char *name) {
    int ai = 0;
    for (int i = 1; i < MAX_PEERS; i++) {
        if (ip == peers[i].ip) {
            peers[i].lastHeard = millis() + PEER_TIMEOUT;
            return;
        } else if (!ai && !peers[i].ip) {
            ai = i;
        }
    }
    if (ai) {
        peers[ai].ip = ip;
        peers[ai].lastHeard = millis() + PEER_TIMEOUT;
        strcpy(peers[ai].name, name);
        gizmo.debug("Peer %s discovered", IPAddress(ip).toString().c_str());
    }
}

boolean hasPotentialMaster() {
    for (int i = 1; i < MAX_PEERS; i++) {
        if (peers[i].ip && peers[0].ip > peers[i].ip) {
            return true;
        }
    }
    return false;
}

void determineMaster() {
    master = 0;
    for (int i = 1; syncWithMaster && i < MAX_PEERS; i++) {
        if (peers[i].ip && peers[master].ip > peers[i].ip) {
            master = i;
        }
    }
}

boolean isMaster(IPAddress ip) {
    return peers[master].ip == (uint32_t) ip;
}

void copyPattern(Command command) {
    if (isMaster(command.src)) {
        if (command.ctx == FRONT_CTX) {
            front.pattern = findPattern((char *) command.data);
        } else if (command.ctx == BACK_CTX) {
            back.pattern = findPattern((char *) command.data);
        }
    }
}

void copyStripColorSettings(Strip *s, Command cmd) {
    s->on = cmd.data[0] && cmd.data[2];
    s->hue = cmd.data[1];
    s->brightness = cmd.data[2];
    s->color.raw[0] = cmd.data[3];
    s->color.raw[1] = cmd.data[4];
    s->color.raw[2] = cmd.data[5];
    for (int i = 0, di = 6; i < 16; i++, di += 3) {
        s->targetPalette.entries[i].raw[0] = cmd.data[di + 0];
        s->targetPalette.entries[i].raw[1] = cmd.data[di + 1];
        s->targetPalette.entries[i].raw[2] = cmd.data[di + 2];
    }
    sleepDimmer = (uint32_t) cmd.data[32];
}

void copyColorSettings(Command command) {
    if (isMaster(command.src)) {
        if (command.ctx == FRONT_CTX) {
            copyStripColorSettings(&front, command);
        } else if (command.ctx == BACK_CTX) {
            copyStripColorSettings(&back, command);
        }
    }
}

void prunePeers() {
    uint32_t now = millis();
    uint8_t peerCount = 0;
    for (int i = 1; i < MAX_PEERS; i++) {
        if (peers[i].ip && peers[i].lastHeard < now) {
            gizmo.debug("Deleted peer %s", IPAddress(peers[i].ip).toString().c_str());
            peers[i].ip = 0;
            peers[i].lastHeard = 0;
        } else if (peers[i].ip) {
            peerCount++;
        }
    }
    determineMaster();

    if (buddyTimestamp && buddyTimestamp < millis()) {
        buddyAvailable = false;
        buddyTimestamp = 0;
        gizmo.debug("Deleted buddy");
    }

    gizmo.debug("peerCount=%d, buddyAvailable=%d", peerCount, buddyAvailable);

    if (peerCount && buddyAvailable) {
        hadBuddyAndPeer = true;

    } else if (!peerCount && !buddyAvailable && hadBuddyAndPeer) {
        gizmo.debug("Restarting group");
        group.flush();
    }
}

void handlePeers() {
    EVERY_N_SECONDS(3)
    {
        prunePeers();
        sayHello();
        requestSamples();
    }

    Command command;
    while (group.parsePacket()) {
        int len = group.read((char *) &command, sizeof(command));
        if (len < 0) {
            gizmo.debug("Unable to read command!!!!");
        } else {
            handlePeer(command);
        }
    }

    while (peer.parsePacket()) {
        int len = peer.read((char *) &command, sizeof(command));
        if (len < 0) {
            gizmo.debug("Unable to read command!!!!");
        } else {
            handlePeer(command);
        }
    }
}

void handlePeer(Command command) {
    if (command.src != (uint32_t) WiFi.localIP()) {
        switch (command.op) {
            case HELLO:
                addPeer(command.src, (char *) command.data);
                determineMaster();
                break;
            case SYNC_REQ:
                syncColorSettings(&front);
                syncPattern(&front);
                syncColorSettings(&back);
                syncPattern(&back);
                break;
            case PATTERN:
                copyPattern(command);
                break;
            case COLORS:
                copyColorSettings(command);
                break;
            case SAMPLE_ADV:
                if (!buddyAvailable) {
                    gizmo.debug("Discovered buddy");
                }
                buddyIp = command.src;
                buddyAvailable = true;
                buddyTimestamp = millis() + PEER_TIMEOUT;
                buddySilent = command.data[0];
                break;
            default:
                break;
        }
    }
}


#define EVERY_X_MILLIS(T, N)  if (T < millis()) { T = millis() + N;

void handleLEDs(Strip *strip) {
    if (strip->on && strip->pattern) {
        EVERY_X_MILLIS(strip->tb, 20)
            uint8_t maxChanges = 24;
            nblendPaletteTowardPalette(strip->currentPalette, strip->targetPalette, maxChanges);
        }

        EVERY_X_MILLIS(strip->th, strip->pattern->huePause)
            strip->hue++; // slowly cycle the "base color" through the rainbow
        }

        EVERY_X_MILLIS(strip->t1, strip->pattern->renderPause)
            strip->pattern->renderer(strip);
            strip->leds[0] = WiFi.status() != WL_CONNECTED ? CRGB::Red : strip->leds[0];
            strip->ctl->showLeds(
                    sleepDimmer < 100 ? (uint8_t)((sleepDimmer * strip->brightness) / 100) : strip->brightness);
        }
    } else {
        fill_solid(strip->leds, LED_COUNT, strip->on ? strip->color : CRGB::Black);
        strip->leds[0] = WiFi.status() != WL_CONNECTED ? CRGB::Red : strip->leds[0];
        strip->ctl->showLeds(strip->brightness);
    }

    // Change the target palette to a 'related colours' palette every 5 seconds.
    EVERY_X_MILLIS(strip->tp, 5000)
        if (isMaster(WiFi.localIP())) {
            // This is the base colour. Other colours are within 16 hues of this. One color is 128 + baseclr.
            uint8_t baseclr = random8();
            strip->targetPalette = CRGBPalette16(
                    CHSV(baseclr + random8(64), 255, random8(128, 255)),
                    CHSV(baseclr + random8(64), 255, random8(128, 255)),
                    CHSV(baseclr + random8(64), 192, random8(128, 255)),
                    CHSV(baseclr + random8(64), 255, random8(128, 255)));
            syncColorSettings(strip);
            syncPattern(strip);
        }
    }

    EVERY_X_MILLIS(strip->t0, 30000)
        if (isMaster(WiFi.localIP())) {
            if (strip->randomMode != NOT_RANDOM) {
                strip->pattern = randomPattern(strip);
                syncPattern(strip);
                requestSamples();
            }
        }
        broadcastState(false);
    }
}

#define SLEEP_FADE_DURATION 60000

void handleSleep() {
    if (sleepTime && sleepTime < millis()) {
        front.on = false;
        syncColorSettings(&front);
        back.on = false;
        syncColorSettings(&back);
        sleepTime = 0;
        sleepDimmer = 100;
        broadcastState(false);

    } else if (sleepTime && (sleepTime - millis()) < SLEEP_FADE_DURATION) {
        sleepDimmer = (sleepTime - millis()) / 600;
    }
}

void handleSamples() {
    if (lastSample && lastSample < millis()) {
        lastSample = 0;
        samplepeak = 0;
        sampleavg = 0;
        oldsample = 0;
    }

    MicSample sample;
    while (buddy.parsePacket()) {
        buddy.read((char *) &sample, sizeof(sample));
        samplepeak = sample.samplepeak;
        sampleavg = sample.sampleavg;
        oldsample = sample.oldsample;
        lastSample = millis() + SAMPLE_TIMEOUT;
    }
}

void loop() {
    if (gizmo.isNetworkAvailable(finishWiFiConnect)) {
        wsServer.loop();
        handlePeers();
        handleSamples();
    }

    EVERY_N_SECONDS(1)
    {
        handleSleep();
    }

    handleLEDs(&front);
    handleLEDs(&back);
}

void finishWiFiConnect() {
    peers[0].ip = (uint32_t) WiFi.localIP();
    strcpy(peers[0].name, gizmo.getHostname());

    buddy.begin(BUDDY_PORT);
    peer.begin(PEER_PORT);
    group.beginMulticast(WiFi.localIP(), groupIp, GROUP_PORT);

    determineMaster();
    sayHello();
    requestSamples();

    Serial.printf("%s is ready\n", LED_LIGHTS);
}

#define STRIP_STATUS "\"%s\": {\"on\": %s,\"rgb\": \"#%06X\",\"brightness\": %d,\"effect\": \"%s\"}"

char *stripStatus(char *html, Strip *s) {
    snprintf(html, 127, STRIP_STATUS, s->name, s->on ? "true" : "false",
             s->color.red << 16 | s->color.green << 8 | s->color.blue, s->brightness,
             s->pattern ? s->pattern->name : "solid");
    return html;
}

void loadState() {
    File f = SPIFFS.open(STATE, "r");
    if (f) {
        loadStripState(f, &front);
        loadStripState(f, &back);

        char field[32];
        f.readBytesUntil('\n', field, 32);
        syncWithMaster = strcmp(field, "off");
        f.close();

    } else {
        front.pattern = findPattern("confetti");
        back.pattern = findPattern("cycle");
        syncWithMaster = true;
    }
}

void loadStripState(File f, Strip *s) {
    char field[32];
    int l = f.readBytesUntil('|', field, 32);
    field[l] = '\0';
    s->on = !strcmp(field, "on");

    l = f.readBytesUntil('|', field, 32);
    field[l] = '\0';
    processColor(field, s, s->on);

    l = f.readBytesUntil('|', field, 7);
    field[l] = '\0';
    s->brightness = atoi(field);

    l = f.readBytesUntil('\n', field, 32);
    field[l] = '\0';
    s->pattern = NULL;
    if (strcmp(field, "none")) {
        processEffect(field, s, s->on);
    }
}

const char *effect(Strip *s) {
    return s->randomMode == ALL ? "random" :
           s->randomMode == FAVORITES ? "fav_random" :
           s->randomMode == SOUND_REACTIVE ? "sr_random" :
           s->randomMode == NOT_SOUND_REACTIVE ? "nsr_random" :
           s->pattern ? s->pattern->name : "solid";
}

void saveState() {
    File f = SPIFFS.open(STATE, "w");
    if (f) {
        f.printf("%s|%d,%d,%d|%d|%s\n", front.on ? "on" : "off",
                 front.color.red, front.color.green, front.color.blue, front.brightness, effect(&front));
        f.printf("%s|%d,%d,%d|%d|%s\n", back.on ? "on" : "off",
                 back.color.red, back.color.green, back.color.blue, back.brightness, effect(&back));
        f.printf("%s\n", syncWithMaster ? "on" : "off");
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
#include "pixels.h"
#include "ripple.h"
#include "matrix.h"
#include "pixel.h"
#include "onesine.h"
#include "noisefire.h"
#include "rainbowg.h"
#include "noisepal.h"
#include "besin.h"
#include "fillnoise.h"
#include "plasmasr.h"
#include "rainbowbit.h"

// Setup a catalog of the different patterns.
Pattern patterns[] = {
        Pattern{.name = "glitter", .renderer = glitter, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "confetti", .renderer = confetti, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "cycle", .renderer = cycle, .huePause = 200, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "rainbow", .renderer = rainbow, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "rainbowg", .renderer = rainbowWithGlitter, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "sinelon", .renderer = sinelon, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "juggle", .renderer = juggle, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "bpm", .renderer = bpm, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "fire", .renderer = fire, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "noise", .renderer = noise, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "blendwave", .renderer = blendwave, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "dotbeat", .renderer = dotBeat, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "plasma", .renderer = plasma, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false},

        Pattern{.name = "sr_pixel", .renderer = pixel, .huePause = 2000, .renderPause = 0, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_pixels", .renderer = pixels, .huePause = 2000, .renderPause = 30, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_ripple", .renderer = ripple, .huePause = 2000, .renderPause = 20, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_matrix", .renderer = matrixDown, .huePause = 2000, .renderPause = 40, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_matrixup", .renderer = matrixUp, .huePause = 2000, .renderPause = 40, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_onesine", .renderer = onesine, .huePause = 2000, .renderPause = 30, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_noisefire", .renderer = noisefire, .huePause = 2000, .renderPause = 0, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_noisepal", .renderer = noisepal, .huePause = 2000, .renderPause = 0, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_rainbowg", .renderer = rainbowg, .huePause = 2000, .renderPause = 10, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_rainbowbit", .renderer = rainbowbit, .huePause = 2000, .renderPause = 10, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_besin", .renderer = besin, .huePause = 2000, .renderPause = 20, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_fillnoise", .renderer = fillnoise, .huePause = 2000, .renderPause = 20, .soundReactive = true, .favorite = false},
        Pattern{.name = "sr_plasma", .renderer = plasmasr, .huePause = 2000, .renderPause = 10, .soundReactive = true, .favorite = false},

        Pattern{.name = "solid", .renderer = solid, .huePause = 2000, .renderPause = 20, .soundReactive = false, .favorite = false},
        Pattern{.name = "test", .renderer = test, .huePause = 20, .renderPause = 20, .soundReactive = false, .favorite = false}
};

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

Pattern *findPattern(const char *name) {
    int i = 0;
    while (strcmp(patterns[i].name, name) && strcmp(patterns[i].name, "test")) {
        i++;
    }
    return &patterns[i];
}

Pattern *randomPattern(Strip *s) {
    Pattern *p;
    boolean nsrFavs = 0;

    int n = ARRAY_SIZE(patterns);
    for (int i = 0; i < n; i++) {
        if (!patterns[i].soundReactive && patterns[i].favorite) {
            nsrFavs++;
        }
    }

    RandomMode mode = s->randomMode;
    if (mode == FAVORITES && favCount == 0) {
        mode = buddySilent ? NOT_SOUND_REACTIVE : SOUND_REACTIVE;
    } else if (buddySilent && mode == FAVORITES && nsrFavs == 0) {
        mode = NOT_SOUND_REACTIVE;
    } else if (buddySilent && mode == SOUND_REACTIVE) {
        mode = NOT_SOUND_REACTIVE;
    }

    do {
        p = &patterns[random8(ARRAY_SIZE(patterns) - 1)];
    } while ((mode == FAVORITES && (!p->favorite || (p->soundReactive && buddySilent))) ||
             (mode == SOUND_REACTIVE && !p->soundReactive) ||
             (mode == NOT_SOUND_REACTIVE && p->soundReactive));
    return p;
}

char *favorites(char *favs) {
    char fav[32];
    favs[0] = '\0';
    strncat(favs, "\"favs\": {", 16);
    int i = 0;
    boolean first = true;
    while (strcmp(patterns[i].name, "test")) {
        if (patterns[i].favorite) {
            fav[0] = '\0';
            snprintf(fav, 24, "%s\"%s\":1", !first ? "," : "", patterns[i].name);
            strncat(favs, fav, 31);
            first = false;
        }
        i++;
    }
    strncat(favs, "},", 16);
    return favs;
}

void loadFavorites() {
    Serial.printf("Loading favorites...\n");
    char name[32];
    File f = SPIFFS.open(FAVS, "r");
    favCount = 0;
    if (f) {
        while (f.available()) {
            int l = f.readBytesUntil('\n', name, 31);
            name[l] = 0;
            Pattern *p = findPattern(name);
            if (p) {
                p->favorite = true;
                favCount++;
            }
        }
        f.close();
    }
}

void saveFavorites() {
    Serial.printf("Saving favorites...\n");
    File f = SPIFFS.open(FAVS, "w");
    favCount = 0;
    if (f) {
        int i = 0;
        while (strcmp(patterns[i].name, "test")) {
            if (patterns[i].favorite) {
                f.printf("%s\n", patterns[i].name);
                favCount++;
            }
            i++;
        }
        f.close();
    }
}
