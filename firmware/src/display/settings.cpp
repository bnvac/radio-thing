#include "settings.h"

#include <Preferences.h>

void Settings::load() {
    Preferences p;
    p.begin("rt-disp", true);
    channel = p.getUChar("chan", 1);
    brightness = p.getUChar("bright", 80);
    feet = p.getBool("feet", false);
    buzzer = p.getBool("buzz", true);
    nerf = p.getBool("nerf", false);
    material = p.getUChar("mat", 0);
    range = p.getUChar("range", 1);
    mode = p.getUChar("mode", 0);
    p.end();
    if (channel < 1 || channel > 13) channel = 1;
    if (brightness < 10 || brightness > 100) brightness = 80;
    if (range > 2) range = 1;
    if (mode > 2) mode = 0;
    if (material > 3) material = 0;
}

void Settings::save() const {
    Preferences p;
    p.begin("rt-disp", false);
    p.putUChar("chan", channel);
    p.putUChar("bright", brightness);
    p.putBool("feet", feet);
    p.putBool("buzz", buzzer);
    p.putBool("nerf", nerf);
    p.putUChar("mat", material);
    p.putUChar("range", range);
    p.putUChar("mode", mode);
    p.end();
}
