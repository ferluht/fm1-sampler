// One draw operation of the phone UI; android DawView replays a list of these.
#pragma once
#include <cstdint>
#include <string>
struct UiOp {
    enum Type { RRECT, STROKE, TEXT, LINE, CIRCLE, TRI, CLIP, ENDCLIP };
    Type type;
    float f[6] = {0, 0, 0, 0, 0, 0};   // RRECT/STROKE x y w h r [stroke]; TEXT x baseline size align(0 l,1 c,2 r) bold; LINE x1 y1 x2 y2 w; CIRCLE cx cy r; TRI x1 y1 x2 y2 x3 y3; CLIP x y w h
    uint32_t color = 0;
    std::string text;
};
