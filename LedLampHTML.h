#define INDEX_HTML \
"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>%s</title>\
    <style>\
      body { padding: 10px; background-color: #ffffff; font-family: Arial, Helvetica, Sans-Serif; font-size: 1.5em; Color: #000000; }\
      h1 { Color: #AA0000; }\
    </style>\
  </head>\
  <body>\
    <h1>%s Upper</h1>\
    <p>On: %s<p>\
    <p>RGB: %06x<p>\
    <p>Brightness: %u<p>\
    <p>LED Pin: %u<p>\
    <p>LED Offset: %u<p>\
    <p>LED Count: %u<p>\
    <h1>%s Lower</h1>\
    <p>On: %s<p>\
    <p>RGB: %06x<p>\
    <p>Brightness: %u<p>\
    <p>LED Pin: %u<p>\
    <p>LED Offset: %u<p>\
    <p>LED Count: %u<p>\
    <p>Topic Prefix: %s<p>\
    <p>Version: %s<p>\
  </body>\
</html>"

#define LED_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <title>LED Setup</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
        h3 {color:#080808;font-size:1.1em;margin-bottom:4px;}\
        input {border:1px solid #2f2f2f;border-radius:4px;font-size:0.9em;padding-left:8px;}\
        input[type=submit] {border-radius:4px;background-color:#f0f0f0;font-size:1.3em;}\
        select {border:1px solid #2f2f2f;border-radius:4px;font-size:0.9em;}\
    </style>\
</head>\
<body><h1>LED Setup</h1>\
<form action=\"/ledcfg\">\
    <h3>Upper LED Pin</h3><input type=\"text\" name=\"upin\" value=\"%d\" size=\"30\"><p>\
    <h3>Upper LED Offset</h3><input type=\"text\" name=\"uoffset\" value=\"%d\" size=\"30\"><p>\
    <h3>Upper LED Count</h3><input type=\"text\" name=\"ucount\" value=\"%d\" size=\"30\"><p>\
    <h3>Lower LED Pin</h3><input type=\"text\" name=\"lpin\" value=\"%d\" size=\"30\"><p>\
    <h3>Lower LED Offset</h3><input type=\"text\" name=\"loffset\" value=\"%d\" size=\"30\"><p>\
    <h3>Lower LED Count</h3><input type=\"text\" name=\"lcount\" value=\"%d\" size=\"30\"><p>\
    <input type=\"submit\" value=\"Apply Changes\">\
</form>\
</body>\
</html>"

#define LEDCFG_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <meta http-equiv=\"refresh\" content=\"5;url=/led\">\
    <title>LED Reconfigured</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
    </style>\
</head>\
<body><h1>LED Reconfigured</h1>\
    Reconfigured LEDS. Restarting...\
</body>\
</html>"

#define MAX_HTML    2048
static char html[MAX_HTML];
