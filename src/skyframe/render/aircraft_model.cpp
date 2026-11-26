#include "skyframe/render/aircraft_model.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace skyframe {
namespace {

struct V {
    float x{0}, y{0}, z{0};
    operator Vector3() const { return {x, y, z}; }
};
V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V operator*(V a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vector3 rv(V a) { return {a.x, a.y, a.z}; }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }

constexpr float kPif = 3.14159265f;
const V kLight = {0.30f, 0.88f, 0.36f};

Color shade(Color c, float k) {
    const auto ch = [k](unsigned char v) {
        return static_cast<unsigned char>(std::clamp(static_cast<float>(v) * k, 0.0f, 255.0f));
    };
    return {ch(c.r), ch(c.g), ch(c.b), c.a};
}

Color rgb(const std::array<std::uint8_t, 3>& c) { return {c[0], c[1], c[2], 255}; }
Color mix(Color a, Color b, float t) {
    return {static_cast<unsigned char>(lerpf(a.r, b.r, t)), static_cast<unsigned char>(lerpf(a.g, b.g, t)),
        static_cast<unsigned char>(lerpf(a.b, b.b, t)), 255};
}

// Two-sided flat-shaded triangle; the normal is flipped to face away from `inside`.
void tri(V a, V b, V c, V inside, Color col) {
    V n = cross(b - a, c - a);
    const float len = std::sqrt(dot(n, n));
    if (len < 1.0e-7f) return;
    n = n * (1.0f / len);
    if (dot(n, (a + b + c) * (1.0f / 3.0f) - inside) < 0.0f) n = n * -1.0f;
    const float k = std::clamp(0.60f + 0.45f * dot(n, kLight), 0.40f, 1.12f);
    DrawTriangle3D(rv(a), rv(b), rv(c), shade(col, k));
}

void quad(V a, V b, V c, V d, V inside, Color col) {
    tri(a, b, c, inside, col);
    tri(a, c, d, inside, col);
}

// Closed slab from a top quad and a bottom quad (matching vertex order).
void slab(const std::array<V, 4>& top, const std::array<V, 4>& bot, Color col) {
    V centre{};
    for (int i = 0; i < 4; ++i) centre = centre + top[static_cast<std::size_t>(i)] + bot[static_cast<std::size_t>(i)];
    centre = centre * (1.0f / 8.0f);
    quad(top[0], top[1], top[2], top[3], centre, col);
    quad(bot[0], bot[1], bot[2], bot[3], centre, col);
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t j = (i + 1) % 4;
        quad(top[i], top[j], bot[j], bot[i], centre, col);
    }
}

struct Ctx {
    const AircraftDesign& d;
    const AircraftSnapshot& s;
    float rz;   // fuselage half width
    float ry;   // fuselage half height
    float halfSpan;
    float rootZ;
    float mountY;
    float tanSweep;
    float tanDih;
    Color body, belly, accent, wingCol, metal;
};

struct WingPt {
    float leX, teX, y, z, thick;
};

WingPt wingAt(const Ctx& c, float u) {
    const float z = lerpf(c.rootZ, c.halfSpan, u);
    const float chord = lerpf(c.d.rootChord, c.d.tipChord, u);
    const float le = c.d.wingX + c.d.rootChord * 0.5f - c.tanSweep * (z - c.rootZ);
    const float y = c.mountY + c.tanDih * (z - c.rootZ);
    const float thick = std::max(0.12f, chord * 0.13f);
    return {le, le - chord, y, z, thick};
}

void drawWingSegment(const Ctx& c, float u0, float u1, float side, Color col) {
    const WingPt a = wingAt(c, u0);
    const WingPt b = wingAt(c, u1);
    const std::array<V, 4> top{V{a.leX, a.y + a.thick * 0.5f, a.z * side}, V{b.leX, b.y + b.thick * 0.5f, b.z * side},
        V{b.teX, b.y + b.thick * 0.5f, b.z * side}, V{a.teX, a.y + a.thick * 0.5f, a.z * side}};
    const std::array<V, 4> bot{V{a.leX, a.y - a.thick * 0.5f, a.z * side}, V{b.leX, b.y - b.thick * 0.5f, b.z * side},
        V{b.teX, b.y - b.thick * 0.5f, b.z * side}, V{a.teX, a.y - a.thick * 0.5f, a.z * side}};
    slab(top, bot, col);
}

// Tail-end vertical offset of the fuselage centreline (slight upsweep).
float tailLift(const Ctx& c, float x) {
    const float start = -c.d.length * 0.5f + c.d.tailLength;
    if (x >= start) return 0.0f;
    const float t = std::clamp((start - x) / std::max(0.1f, c.d.tailLength), 0.0f, 1.0f);
    return c.ry * 0.9f * t * t;
}

float tailRadius(float t) { return 1.0f - 0.80f * std::pow(t, 1.15f); }

void drawFuselage(const Ctx& c) {
    const AircraftDesign& d = c.d;
    const float halfL = d.length * 0.5f;
    struct Station { float x, rScale, yOff, hump; };
    std::array<Station, 24> st{};
    int n = 0;
    const int noseSegs = 6, bodySegs = 9, tailSegs = 5;
    for (int i = 0; i <= noseSegs; ++i) {      // tip -> start of cylinder
        const float t = 1.0f - static_cast<float>(i) / noseSegs;   // 1 at tip
        const float x = halfL - d.noseLength + d.noseLength * t;
        st[static_cast<std::size_t>(n++)] = {x, std::max(0.07f, std::sqrt(std::max(0.0f, 1.0f - t * t))), 0.0f, 0.0f};
    }
    // reorder: we generated tip first (t=1 at i=0 gives x=halfL); stations run tip -> tail
    for (int i = 1; i <= bodySegs; ++i) {
        const float x = lerpf(halfL - d.noseLength, -halfL + d.tailLength, static_cast<float>(i) / bodySegs);
        st[static_cast<std::size_t>(n++)] = {x, 1.0f, 0.0f, 0.0f};
    }
    for (int i = 1; i <= tailSegs; ++i) {
        const float t = static_cast<float>(i) / tailSegs;
        const float x = -halfL + d.tailLength - d.tailLength * t;
        st[static_cast<std::size_t>(n++)] = {x, tailRadius(t), c.ry * 0.9f * t * t, 0.0f};
    }
    if (d.humpDeck) {
        const float front = halfL - d.noseLength * 0.35f;
        const float back = halfL - d.noseLength - 0.22f * d.length;
        for (int i = 0; i < n; ++i) {
            Station& s = st[static_cast<std::size_t>(i)];
            if (s.x < front && s.x > back) s.hump = std::sin(kPif * (front - s.x) / (front - back));
        }
    }

    constexpr int kSides = 16;
    for (int i = 0; i + 1 < n; ++i) {
        const Station a = st[static_cast<std::size_t>(i)];
        const Station b = st[static_cast<std::size_t>(i + 1)];
        const V ca{a.x, a.yOff, 0.0f}, cb{b.x, b.yOff, 0.0f};
        const V mid = (ca + cb) * 0.5f;
        const bool stripe = std::abs(a.rScale - 1.0f) < 1.0e-3f && std::abs(b.rScale - 1.0f) < 1.0e-3f;
        for (int k = 0; k < kSides; ++k) {
            const float t0 = 2.0f * kPif * static_cast<float>(k) / kSides;
            const float t1 = 2.0f * kPif * static_cast<float>(k + 1) / kSides;
            const auto ring = [&](const Station& s, float th) {
                const float sn = std::sin(th);
                const float hump = sn > 0.0f ? 1.0f + 0.40f * s.hump : 1.0f;
                return V{s.x, s.yOff + c.ry * s.rScale * sn * hump, c.rz * s.rScale * std::cos(th)};
            };
            const float sm = std::sin((t0 + t1) * 0.5f);
            Color col = c.body;
            if (sm < -0.55f) col = c.belly;
            else if (stripe && sm < -0.05f && sm > -0.40f) col = c.accent;
            quad(ring(a, t0), ring(a, t1), ring(b, t1), ring(b, t0), mid, col);
        }
    }

    // Cockpit glazing and cabin windows.
    const Color glass{28, 44, 66, 255};
    const bool fighter = d.engineLayout == EngineLayout::Internal;
    if (fighter) {
        rlPushMatrix();
        rlTranslatef(halfL - d.noseLength * 0.58f, c.ry * 0.55f, 0.0f);
        rlScalef(d.noseLength * 0.34f, c.ry * 0.55f, c.rz * 0.42f);
        DrawSphere({0, 0, 0}, 1.0f, {80, 120, 160, 255});
        rlPopMatrix();
    } else {
        DrawCubeV({halfL - d.noseLength * 0.58f, c.ry * 0.52f, 0.0f},
            {d.noseLength * 0.26f, c.ry * 0.30f, c.rz * 1.52f}, glass);
        if (d.bodyWidth > 2.4f) {
            const float x0 = halfL - d.noseLength * 0.95f;
            const float x1 = -halfL + d.tailLength * 0.55f;
            const float y = c.ry * 0.30f;
            const float zz = c.rz * 0.955f + 0.03f;
            DrawCubeV({(x0 + x1) * 0.5f, y, zz}, {x0 - x1, c.ry * 0.10f, 0.05f}, glass);
            DrawCubeV({(x0 + x1) * 0.5f, y, -zz}, {x0 - x1, c.ry * 0.10f, 0.05f}, glass);
        }
    }
}

void drawStabiliser(const Ctx& c, float x, float y, float span, float chord, Color col) {
    const float half = span * 0.5f;
    const float tip = chord * 0.42f;
    const float sweep = 0.55f;
    const float th = std::max(0.08f, chord * 0.08f);
    for (int side = -1; side <= 1; side += 2) {
        const float sd = static_cast<float>(side);
        const float rootZ = std::min(c.rz * 0.5f, half * 0.4f);
        const std::array<V, 4> top{V{x + chord * 0.5f, y + th * 0.5f, rootZ * sd},
            V{x + chord * 0.5f - sweep * (half - rootZ), y + th * 0.5f, half * sd},
            V{x + chord * 0.5f - sweep * (half - rootZ) - tip, y + th * 0.5f, half * sd},
            V{x - chord * 0.5f, y + th * 0.5f, rootZ * sd}};
        std::array<V, 4> bot = top;
        for (V& p : bot) p.y -= th;
        slab(top, bot, col);
    }
}

void drawFin(const Ctx& c, float x, float yBase, float z, Color col) {
    const float h = c.d.finHeight;
    const float ch = c.d.finChord;
    const float tip = ch * 0.45f;
    const float sweepX = h * 0.62f;
    const float th = std::max(0.10f, ch * 0.07f);
    const std::array<V, 4> top{V{x + ch * 0.5f, yBase, z + th * 0.5f}, V{x + ch * 0.5f - sweepX, yBase + h, z + th * 0.5f},
        V{x + ch * 0.5f - sweepX - tip, yBase + h, z + th * 0.5f}, V{x - ch * 0.5f, yBase, z + th * 0.5f}};
    std::array<V, 4> bot = top;
    for (V& p : bot) p.z -= th;
    slab(top, bot, col);
}

void drawTail(const Ctx& c) {
    const AircraftDesign& d = c.d;
    const float finX = -d.length * 0.5f + d.finChord * 0.55f + d.length * 0.015f;
    const float baseY = tailLift(c, finX) + c.ry * 0.45f;
    const float stabChord = d.finChord * 0.62f;
    if (d.tail == TailKind::Twin) {
        const float z = std::max(c.rz * 0.8f, d.stabSpan * 0.18f);
        drawFin(c, finX, baseY, z, c.accent);
        drawFin(c, finX, baseY, -z, c.accent);
        drawStabiliser(c, finX - d.finChord * 0.15f, tailLift(c, finX) + c.ry * 0.1f, d.stabSpan, stabChord, c.wingCol);
        return;
    }
    drawFin(c, finX, baseY, 0.0f, c.accent);
    if (d.tail == TailKind::TTail) {
        const float tipX = finX + d.finChord * 0.5f - d.finHeight * 0.62f - d.finChord * 0.2f;
        drawStabiliser(c, tipX, baseY + d.finHeight, d.stabSpan, stabChord, c.wingCol);
    } else {
        drawStabiliser(c, finX - d.finChord * 0.1f, tailLift(c, finX) + c.ry * 0.15f, d.stabSpan, stabChord, c.wingCol);
    }
}

struct EngineSlot {
    V pos;
    int side;          // 0 left, 1 right, 2 centre/single
    bool pylon;
};

int collectEngines(const Ctx& c, std::array<EngineSlot, 8>& out) {
    const AircraftDesign& d = c.d;
    int n = 0;
    const float half = d.engineLength * 0.5f;
    switch (d.engineLayout) {
        case EngineLayout::UnderWing: {
            const int perSide = std::max(1, d.engineCount / 2);
            for (int side = 0; side < 2; ++side) {
                const float sd = side == 0 ? -1.0f : 1.0f;
                for (int i = 0; i < perSide && n < 8; ++i) {
                    const float lo = perSide > 2 ? 0.16f : 0.26f;
                    const float hi = perSide > 2 ? 0.72f : 0.55f;
                    const float u = perSide == 1 ? 0.34f
                        : lo + (hi - lo) * static_cast<float>(i) / static_cast<float>(perSide - 1);
                    const WingPt w = wingAt(c, u);
                    const bool above = d.wingMount == WingMount::High && d.engineKind != EngineKind::Jet;
                    const float y = above ? w.y - d.engineRadius * 0.35f : w.y - w.thick * 0.5f - d.engineRadius * 0.95f;
                    out[static_cast<std::size_t>(n++)] = {{w.leX - half * 0.30f, y, w.z * sd}, side, !above};
                }
            }
            break;
        }
        case EngineLayout::RearFuselage: {
            const float x = -d.length * 0.5f + d.tailLength * 0.72f;
            const float y = tailLift(c, x) + c.ry * 0.42f;
            const float z = c.rz + d.engineRadius * 1.15f;
            out[static_cast<std::size_t>(n++)] = {{x, y, -z}, 0, true};
            out[static_cast<std::size_t>(n++)] = {{x, y, z}, 1, true};
            if (d.engineCount >= 3) {
                out[static_cast<std::size_t>(n++)] = {{-d.length * 0.5f + d.tailLength * 0.42f,
                    c.ry + d.engineRadius * 1.0f + tailLift(c, -d.length * 0.5f + d.tailLength * 0.42f), 0.0f}, 2, false};
            }
            break;
        }
        case EngineLayout::Nose:
            out[static_cast<std::size_t>(n++)] = {{d.length * 0.5f - half * 0.95f, 0.0f, 0.0f}, 2, false};
            break;
        case EngineLayout::Internal: {
            const float x = -d.length * 0.5f + half * 0.9f;
            out[static_cast<std::size_t>(n++)] = {{x, c.ry * -0.05f, -c.rz * 0.42f}, 0, false};
            out[static_cast<std::size_t>(n++)] = {{x, c.ry * -0.05f, c.rz * 0.42f}, 1, false};
            break;
        }
    }
    return n;
}

void drawPropeller(const Ctx& c, V front, float radius, int blades, bool running) {
    const float throttle = static_cast<float>(c.s.controls.throttle);
    const float angle = running ? static_cast<float>(GetTime()) * (260.0f + 900.0f * throttle) : 0.0f;
    rlPushMatrix();
    rlTranslatef(front.x, front.y, front.z);
    rlRotatef(angle, 1.0f, 0.0f, 0.0f);
    for (int i = 0; i < blades; ++i) {
        rlPushMatrix();
        rlRotatef(360.0f * static_cast<float>(i) / static_cast<float>(blades), 1.0f, 0.0f, 0.0f);
        DrawCubeV({0.0f, radius * 0.5f, 0.0f}, {0.07f, radius, std::max(0.12f, radius * 0.11f)}, {30, 32, 36, 255});
        rlPopMatrix();
    }
    rlPopMatrix();
    if (running) {
        DrawCylinderEx({front.x - 0.02f, front.y, front.z}, {front.x + 0.02f, front.y, front.z}, radius, radius, 20, {200, 205, 210, 55});
    }
}

void drawEngines(const Ctx& c) {
    const AircraftDesign& d = c.d;
    std::array<EngineSlot, 8> slots{};
    const int n = collectEngines(c, slots);
    const Color healthy = mix(c.metal, c.body, 0.35f);
    const Color hot{200, 108, 45, 255};
    const Color dead{86, 52, 48, 255};
    const float pulse = 0.65f + 0.18f * static_cast<float>(std::sin(GetTime() * 3.0));

    for (int i = 0; i < n; ++i) {
        const EngineSlot& e = slots[static_cast<std::size_t>(i)];
        const EngineState* st = &c.s.systems.engines[e.side == 1 ? 1 : 0];
        bool running = st->running;
        bool overheated = st->overheated;
        if (e.side == 2 && d.engineLayout != EngineLayout::Nose) {   // centre engine follows either side
            running = c.s.systems.engines[0].running || c.s.systems.engines[1].running;
            overheated = c.s.systems.engines[0].overheated || c.s.systems.engines[1].overheated;
        }
        const Color col = overheated ? hot : running ? healthy : dead;
        const float len = d.engineLength;
        const float r = d.engineRadius;
        const V front = e.pos + V{len * 0.5f, 0.0f, 0.0f};
        const V rear = e.pos - V{len * 0.5f, 0.0f, 0.0f};

        if (e.pylon) {
            const float topY = e.pos.y + r + 0.05f;
            const float reach = (d.engineLayout == EngineLayout::RearFuselage) ? 0.0f : 0.5f;
            if (d.engineLayout == EngineLayout::RearFuselage) {
                const float sideSign = e.pos.z < 0.0f ? -1.0f : 1.0f;
                DrawCubeV({e.pos.x, e.pos.y, sideSign * (c.rz + r * 0.5f)}, {len * 0.55f, r * 0.7f, r * 1.1f}, c.body);
            } else {
                DrawCubeV({e.pos.x + len * 0.1f, topY + reach * 0.2f, e.pos.z}, {len * 0.5f, 0.55f * r, 0.22f}, c.body);
            }
        }

        if (d.engineKind == EngineKind::Jet) {
            DrawCylinderEx(front, rear, r, r * 0.82f, 14, col);
            DrawCylinderEx(front + V{0.02f, 0, 0}, front - V{0.35f * r, 0, 0}, r * 0.80f, r * 0.80f, 14, {26, 28, 32, 255}); // intake
            DrawCylinderEx(rear + V{0.05f, 0, 0}, rear - V{r * 0.9f, 0, 0}, r * 0.66f, r * 0.34f, 12, {70, 72, 76, 255});    // exhaust
            if (!running) DrawCylinderEx(front + V{0.03f, 0, 0}, front - V{0.1f, 0, 0}, r * 0.55f, r * 0.55f, 10, {48, 28, 26, 255});
        } else {
            const float cowl = d.engineKind == EngineKind::Piston ? r : r * 0.85f;
            DrawCylinderEx(front - V{r * 0.6f, 0, 0}, rear, cowl * 0.9f, cowl * 0.75f, 12, col);
            DrawCylinderEx(front, front - V{r * 0.6f, 0, 0}, cowl * 0.35f, cowl * 0.9f, 12, shade(col, 1.15f));   // spinner
            const float propR = d.engineKind == EngineKind::Piston ? r * 2.0f : r * 2.4f;
            drawPropeller(c, front + V{0.12f, 0, 0}, propR, d.engineKind == EngineKind::Piston ? 2 : 4, running);
        }

        if (overheated) {
            DrawSphere(rv(rear - V{r * 0.8f, -r * 0.2f, 0.0f}), pulse * r * 1.8f, {88, 92, 96, 140});
            DrawSphere(rv(rear - V{r * 2.2f, -r * 0.7f, 0.0f}), pulse * r * 1.4f, {70, 72, 76, 100});
        }
    }
}

void drawWheel(V centre, float radius, float width) {
    DrawCylinderEx(rv(centre - V{0, 0, width * 0.5f}), rv(centre + V{0, 0, width * 0.5f}), radius, radius, 10, {22, 22, 24, 255});
    DrawCylinderEx(rv(centre - V{0, 0, width * 0.52f}), rv(centre + V{0, 0, width * 0.52f}), radius * 0.45f, radius * 0.45f, 8, {150, 154, 160, 255});
}

void drawGear(const Ctx& c) {
    const AircraftDesign& d = c.d;
    const Color strut = c.s.systems.landingGearIntegrity < 0.5 ? Color{210, 120, 40, 255} : Color{120, 124, 130, 255};
    const float bottom = -(c.ry + d.gearHeight);
    const float wr = std::clamp(d.gearHeight * 0.26f, 0.22f, 0.95f);
    const float ww = wr * 0.7f;

    // Nose gear.
    const float noseX = d.length * 0.5f - d.noseLength * 0.85f;
    DrawCylinderEx({noseX, -c.ry * 0.6f, 0.0f}, {noseX, bottom + wr, 0.0f}, 0.07f + wr * 0.12f, 0.07f + wr * 0.10f, 6, strut);
    drawWheel({noseX, bottom + wr, wr * 0.55f}, wr * 0.85f, ww);
    drawWheel({noseX, bottom + wr, -wr * 0.55f}, wr * 0.85f, ww);

    // Main gear.
    const bool highWing = d.wingMount == WingMount::High;
    const bool fighter = d.engineLayout == EngineLayout::Internal;
    for (int side = -1; side <= 1; side += 2) {
        const float sd = static_cast<float>(side);
        float z, topY;
        float x = d.wingX - d.rootChord * 0.08f;
        if (highWing) {
            z = (c.rz + wr * 1.2f + 0.2f) * sd;
            topY = -c.ry * 0.3f;
            DrawCubeV({x, -c.ry * 0.55f, sd * (c.rz + wr * 0.55f)}, {d.rootChord * 0.9f, c.ry * 0.9f, wr * 1.4f}, c.body);   // sponson
        } else if (fighter) {
            z = c.rz * 0.8f * sd;
            topY = -c.ry * 0.2f;
            x = d.wingX - 0.3f;
        } else {
            const WingPt w = wingAt(c, 0.22f);
            z = w.z * sd;
            topY = w.y - w.thick * 0.5f;
            x = (w.leX + w.teX) * 0.5f - 0.2f;
        }
        const float lowest = bottom + wr;
        DrawCylinderEx({x, topY, z}, {x, lowest, z}, 0.08f + wr * 0.16f, 0.07f + wr * 0.13f, 6, strut);
        const int axles = d.bodyWidth > 5.0f ? 2 : 1;
        for (int a = 0; a < axles; ++a) {
            const float ox = axles == 2 ? (a == 0 ? 0.9f * wr * 1.6f : -0.9f * wr * 1.6f) : 0.0f;
            drawWheel({x + ox, lowest, z + wr * 0.55f * sd}, wr, ww);
            drawWheel({x + ox, lowest, z - wr * 0.55f * sd}, wr, ww);
        }
    }
}

} // namespace

void drawAircraftModel(const AircraftDesign& d, const AircraftSnapshot& state) {
    Ctx c{d, state, d.bodyWidth * 0.5f, d.bodyHeight * 0.5f, d.wingSpan * 0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
        rgb(d.body), {}, rgb(d.accent), {}, {}};
    c.rootZ = c.rz * 0.85f;
    c.mountY = d.wingMount == WingMount::High ? c.ry * 0.80f : d.wingMount == WingMount::Mid ? 0.0f : -c.ry * 0.55f;
    c.tanSweep = std::tan(d.sweepDegrees * kPif / 180.0f);
    c.tanDih = std::tan(d.dihedralDegrees * kPif / 180.0f);
    c.belly = mix(c.body, {245, 245, 245, 255}, 0.45f);
    c.wingCol = mix(c.body, {96, 110, 128, 255}, 0.42f);
    c.metal = {150, 158, 168, 255};

    Vec3 axis{};
    double angle = 0.0;
    state.orientation.toAxisAngle(axis, angle);

    const bool gearDown = state.controls.gearDown;
    const float clearance = gearDown && !state.systems.gearStuck ? 1.9f : 0.65f;
    const float bottomLocal = gearDown ? -(c.ry + d.gearHeight) : -c.ry;
    const float shiftY = -clearance - bottomLocal * kAircraftRenderScale;

    rlDrawRenderBatchActive();   // flush earlier geometry with culling still on
    rlDisableBackfaceCulling();
    rlPushMatrix();
    rlTranslatef(static_cast<float>(state.position.x), static_cast<float>(state.position.y), static_cast<float>(state.position.z));
    if (std::abs(angle) > 1.0e-5) {
        rlRotatef(static_cast<float>(degrees(angle)), static_cast<float>(axis.x), static_cast<float>(axis.y), static_cast<float>(axis.z));
    }
    rlTranslatef(0.0f, shiftY, 0.0f);
    rlScalef(kAircraftRenderScale, kAircraftRenderScale, kAircraftRenderScale);

    drawFuselage(c);

    // Wings: inboard section always; outboard panel disappears with a panel failure.
    for (int side = -1; side <= 1; side += 2) {
        const bool panelVisible = side < 0 ? state.systems.leftWingPanelVisible : state.systems.rightWingPanelVisible;
        const float sd = static_cast<float>(side);
        drawWingSegment(c, 0.0f, 0.78f, sd, c.wingCol);
        if (panelVisible) drawWingSegment(c, 0.78f, 1.0f, sd, mix(c.wingCol, c.body, 0.25f));
        if (d.winglets && panelVisible) {
            const WingPt w = wingAt(c, 1.0f);
            const float h = d.wingSpan * 0.055f;
            const std::array<V, 4> top{V{w.leX, w.y, w.z * sd + 0.05f}, V{w.leX - w.thick * 5.0f, w.y + h, w.z * sd + 0.05f},
                V{w.teX + 0.3f, w.y + h, w.z * sd + 0.05f}, V{w.teX, w.y, w.z * sd + 0.05f}};
            std::array<V, 4> bot = top;
            for (V& p : bot) p.z -= 0.10f;
            slab(top, bot, c.accent);
        }
    }

    drawTail(c);
    drawEngines(c);
    if (gearDown) drawGear(c);
    rlPopMatrix();
    rlDrawRenderBatchActive();   // raylib batches draws: flush while culling is still disabled
    rlEnableBackfaceCulling();
}

} // namespace skyframe
