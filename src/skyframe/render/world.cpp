#include "skyframe/render/world.hpp"

#include "skyframe/render/aircraft_model.hpp"
#include "skyframe/sim/fleet.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace skyframe {
namespace {

Vector3 toRay(const Vec3& v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

Color terrainColor(MapPreset terrain) {
    switch (terrain) {
        case MapPreset::TrainingField: return {76, 103, 73, 255};
        case MapPreset::Coast: return {93, 129, 103, 255};
        case MapPreset::Highlands: return {91, 105, 91, 255};
        case MapPreset::Desert: return {159, 139, 95, 255};
        case MapPreset::Hills: return {91, 123, 78, 255};
        case MapPreset::Plains: return {119, 145, 91, 255};
        case MapPreset::City: return {76, 83, 87, 255};
        case MapPreset::Ocean: return {39, 92, 119, 255};
        case MapPreset::Tundra: return {156, 173, 180, 255};
        case MapPreset::Mountains: return {89, 99, 103, 255};
        case MapPreset::Random: return {76, 103, 73, 255};
    }
    return {76, 103, 73, 255};
}

Color terrainElevationColor(MapPreset terrain, double height) {
    Color base = terrainColor(terrain);
    if ((terrain == MapPreset::Mountains || terrain == MapPreset::Highlands)
        && height > 250.0) return {190, 197, 194, 255};
    if (terrain == MapPreset::Tundra && height > 24.0) return {193, 210, 215, 255};
    const float factor = static_cast<float>(clamp(0.9 + height * 0.00035, 0.72, 1.12));
    base.r = static_cast<unsigned char>(clamp(static_cast<double>(base.r) * factor, 0.0, 255.0));
    base.g = static_cast<unsigned char>(clamp(static_cast<double>(base.g) * factor, 0.0, 255.0));
    base.b = static_cast<unsigned char>(clamp(static_cast<double>(base.b) * factor, 0.0, 255.0));
    return base;
}

Color featureColor(const TerrainFeature& feature, MapPreset terrain) {
    const unsigned char variation = static_cast<unsigned char>(feature.palette * 5U);
    switch (feature.kind) {
        case TerrainFeatureKind::Tree: return {static_cast<unsigned char>(48U + variation), static_cast<unsigned char>(92U + variation), 54, 255};
        case TerrainFeatureKind::Rock: return {105, 108, 103, 255};
        case TerrainFeatureKind::Hill: return terrain == MapPreset::Desert ? Color{174, 147, 94, 255} : Color{92, 117, 82, 255};
        case TerrainFeatureKind::Mountain: return terrain == MapPreset::Tundra ? Color{194, 207, 210, 255} : Color{105, 112, 112, 255};
        case TerrainFeatureKind::Building: return {static_cast<unsigned char>(92U + variation), static_cast<unsigned char>(103U + variation), static_cast<unsigned char>(112U + variation), 255};
        case TerrainFeatureKind::Dune: return {190, 158, 103, 255};
        case TerrainFeatureKind::Iceberg: return {207, 224, 229, 255};
        case TerrainFeatureKind::Water: return {46, 111, 143, 220};
        case TerrainFeatureKind::Tower: return {101, 109, 111, 255};
        case TerrainFeatureKind::PowerPole: return {94, 79, 57, 255};
    }
    return terrainColor(terrain);
}

void drawAirport(const GeneratedMap& map) {
    if (!map.hasAirport) return;
    const float x = static_cast<float>(map.airportPosition.x);
    const float runwayLength = static_cast<float>(map.runwayLength);
    const float runwayWidth = static_cast<float>(map.runwayWidth);
    DrawCubeV({x, 0.10f, 0.0f}, {runwayLength, 0.26f, runwayWidth}, {48, 51, 56, 255});
    DrawCubeV({x, 0.23f, runwayWidth * 0.5f + 1.2f}, {runwayLength, 0.10f, 1.8f}, LIGHTGRAY);
    DrawCubeV({x, 0.23f, -runwayWidth * 0.5f - 1.2f}, {runwayLength, 0.10f, 1.8f}, LIGHTGRAY);
    const int markings = std::max(8, static_cast<int>(map.runwayLength / 95.0));
    for (int i = 0; i < markings; ++i) {
        const float markX = x - runwayLength * 0.46f + static_cast<float>(i) * runwayLength * 0.92f / static_cast<float>(markings - 1);
        DrawCubeV({markX, 0.25f, 0.0f}, {22.0f, 0.08f, 1.5f}, {230, 232, 226, 255});
    }
    const float apronX = x + runwayLength * 0.18f;
    DrawCubeV({apronX, 0.10f, runwayWidth * 1.2f}, {runwayLength * 0.34f, 0.20f, runwayWidth * 1.9f}, {76, 79, 79, 255});
    DrawCubeV({apronX + runwayLength * 0.1f, 12.0f, runwayWidth * 1.55f},
        {runwayLength * 0.12f, 24.0f, runwayWidth * 0.55f}, {132, 145, 151, 255});
    const int hangars = map.resolvedAirportClass == AirportClass::Airfield ? 1
        : map.resolvedAirportClass == AirportClass::Regional ? 2 : 3;
    for (int i = 0; i < hangars; ++i) {
        const float hz = runwayWidth * (1.9f + static_cast<float>(i) * 0.9f);
        DrawCubeV({apronX - runwayLength * 0.09f, 13.0f, hz}, {72.0f, 26.0f, 52.0f}, {98, 112, 123, 255});
        DrawCubeV({apronX - runwayLength * 0.09f, 5.0f, hz + 26.3f}, {32.0f, 12.0f, 1.0f}, {48, 62, 73, 255});
    }
    DrawCubeV({x - runwayLength * 0.32f, 3.5f, -runwayWidth * 1.7f}, {88.0f, 7.0f, 38.0f}, {121, 132, 139, 255});
}

void drawFeature(const TerrainFeature& feature, MapPreset terrain) {
    const Vector3 p = toRay(feature.position);
    const Vector3 s = toRay(feature.scale);
    const Color color = featureColor(feature, terrain);
    switch (feature.kind) {
        case TerrainFeatureKind::Tree:
            DrawCylinder({p.x, p.y + s.y * 0.28f, p.z}, std::max(0.8f, s.x * 0.10f),
                std::max(0.8f, s.x * 0.07f), s.y * 0.56f, 5, {93, 72, 49, 255});
            DrawSphere({p.x, p.y + s.y * 0.73f, p.z}, std::max(s.x, s.z) * 0.66f, color);
            break;
        case TerrainFeatureKind::Rock:
            DrawSphere({p.x, p.y + s.y * 0.38f, p.z}, std::max({s.x, s.y, s.z}) * 0.58f, color);
            break;
        case TerrainFeatureKind::Hill:
            DrawSphere({p.x, p.y + s.y * 0.20f, p.z}, std::max(s.x, s.z) * 0.75f, color);
            break;
        case TerrainFeatureKind::Mountain:
            DrawCylinder({p.x, p.y + s.y * 0.5f, p.z}, 0.0f, std::max(s.x, s.z) * 0.62f,
                s.y, 7, color);
            DrawCylinder({p.x, p.y + s.y * 0.83f, p.z}, 0.0f, std::max(s.x, s.z) * 0.19f,
                s.y * 0.34f, 6, {177, 185, 183, 255});
            break;
        case TerrainFeatureKind::Building:
            DrawCubeV({p.x, p.y + s.y * 0.5f, p.z}, s, color);
            DrawCubeV({p.x, p.y + s.y + 1.3f, p.z}, {s.x * 0.25f, 2.6f, s.z * 0.25f}, {80, 87, 94, 255});
            break;
        case TerrainFeatureKind::Dune:
            DrawSphere({p.x, p.y + s.y * 0.35f, p.z}, std::max(s.x, s.z) * 0.72f, color);
            break;
        case TerrainFeatureKind::Iceberg:
            DrawCubeV({p.x, p.y + s.y * 0.4f, p.z}, s, color);
            break;
        case TerrainFeatureKind::Water:
            DrawCubeV({p.x, p.y - 0.3f, p.z}, {s.x, 0.35f, s.z}, color);
            break;
        case TerrainFeatureKind::Tower:
            DrawCylinder({p.x, p.y + s.y * 0.5f, p.z}, s.x * 0.18f, s.x * 0.32f, s.y, 5, color);
            DrawCylinder({p.x, p.y + s.y * 0.78f, p.z}, s.x * 0.8f, s.x * 0.8f, 2.0f, 4, color);
            break;
        case TerrainFeatureKind::PowerPole:
            DrawCylinder({p.x, p.y + s.y * 0.5f, p.z}, 0.8f, 0.55f, s.y, 5, color);
            DrawCubeV({p.x, p.y + s.y * 0.83f, p.z}, {s.x * 5.0f, 1.2f, 1.2f}, color);
            break;
    }
}

void drawTerrain(const GeneratedMap& map, const Vec3& cameraFocus) {
    (void)cameraFocus;
    const float extent = static_cast<float>(map.worldExtent);
    const int gridCells = map.heightGridCells;
    const int gridSamples = gridCells + 1;
    const float cellSize = (extent * 2.0f) / static_cast<float>(gridCells);
    const auto heightAt = [&map, gridSamples](int x, int z) {
        const std::size_t index = static_cast<std::size_t>(z * gridSamples + x);
        return map.heightField[index];
    };

    // The core precomputes this deterministic height field once per map. Rendering only
    // reads the cached samples and submits a compact low-poly triangle grid each frame.
    for (int zIndex = 0; zIndex < gridCells; ++zIndex) {
        for (int xIndex = 0; xIndex < gridCells; ++xIndex) {
            const double x0 = -map.worldExtent + static_cast<double>(xIndex) * cellSize;
            const double z0 = -map.worldExtent + static_cast<double>(zIndex) * cellSize;
            const double x1 = x0 + cellSize;
            const double z1 = z0 + cellSize;
            const double h00 = heightAt(xIndex, zIndex);
            const double h10 = heightAt(xIndex + 1, zIndex);
            const double h01 = heightAt(xIndex, zIndex + 1);
            const double h11 = heightAt(xIndex + 1, zIndex + 1);
            const Vector3 v00{static_cast<float>(x0), static_cast<float>(h00), static_cast<float>(z0)};
            const Vector3 v10{static_cast<float>(x1), static_cast<float>(h10), static_cast<float>(z0)};
            const Vector3 v01{static_cast<float>(x0), static_cast<float>(h01), static_cast<float>(z1)};
            const Vector3 v11{static_cast<float>(x1), static_cast<float>(h11), static_cast<float>(z1)};
            const double meanHeight = (h00 + h10 + h01 + h11) * 0.25;
            const Color color = terrainElevationColor(map.resolvedTerrain, meanHeight);
            DrawTriangle3D(v00, v01, v10, color);
            DrawTriangle3D(v10, v01, v11, color);
        }
    }

    if (map.resolvedTerrain == MapPreset::Ocean) {
        DrawPlane({0.0f, -0.65f, 0.0f}, {extent * 2.05f, extent * 2.05f}, {43, 105, 137, 255});
        DrawPlane({0.0f, -0.35f, 0.0f}, {extent * 0.62f, extent * 0.62f}, {105, 133, 111, 255});
    } else if (map.resolvedTerrain == MapPreset::Coast) {
        DrawCubeV({0.0f, -1.1f, extent * 0.66f}, {extent * 2.0f, 0.24f, extent * 0.70f}, {44, 106, 139, 255});
    }

    drawAirport(map);
    const double maxDistance = map.worldExtent * 1.32;
    const double maxDistanceSquared = maxDistance * maxDistance;
    const TerrainFeature* previousPowerPole = nullptr;
    for (const TerrainFeature& feature : map.features) {
        const double dx = feature.position.x - cameraFocus.x;
        const double dz = feature.position.z - cameraFocus.z;
        const bool visible = dx * dx + dz * dz <= maxDistanceSquared;
        if (visible) drawFeature(feature, map.resolvedTerrain);

        if (feature.kind == TerrainFeatureKind::PowerPole) {
            if (previousPowerPole != nullptr) {
                const double midpointX = (feature.position.x + previousPowerPole->position.x) * 0.5;
                const double midpointZ = (feature.position.z + previousPowerPole->position.z) * 0.5;
                const double midpointDistance = (midpointX - cameraFocus.x) * (midpointX - cameraFocus.x)
                    + (midpointZ - cameraFocus.z) * (midpointZ - cameraFocus.z);
                if (midpointDistance <= maxDistanceSquared) {
                    for (int cable = -1; cable <= 1; ++cable) {
                        const double lateral = static_cast<double>(cable) * 2.3;
                        const Vector3 start{
                            static_cast<float>(previousPowerPole->position.x),
                            static_cast<float>(previousPowerPole->position.y + previousPowerPole->scale.y * 0.86 + 3.0),
                            static_cast<float>(previousPowerPole->position.z + lateral)};
                        const Vector3 middle{
                            static_cast<float>(midpointX),
                            static_cast<float>((previousPowerPole->position.y + feature.position.y) * 0.5
                                + std::min(previousPowerPole->scale.y, feature.scale.y) * 0.86 - 2.8),
                            static_cast<float>(midpointZ + lateral)};
                        const Vector3 end{
                            static_cast<float>(feature.position.x),
                            static_cast<float>(feature.position.y + feature.scale.y * 0.86 + 3.0),
                            static_cast<float>(feature.position.z + lateral)};
                        DrawLine3D(start, middle, {42, 43, 45, 255});
                        DrawLine3D(middle, end, {42, 43, 45, 255});
                    }
                }
            }
            previousPowerPole = &feature;
        }
    }
}


// ---- Ordnance sandbox visuals: static targets, falling bombs, explosions ---------------------

Color withAlpha(Color c, float alpha) {
    c.a = static_cast<unsigned char>(clamp(static_cast<double>(alpha), 0.0, 255.0));
    return c;
}

Vector3 vec(const Vec3& v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

void drawCrashDummy(Vector3 base, bool lying, float yawDeg) {
    const Color yellow{240, 196, 24, 255};
    const Color black{24, 24, 24, 255};
    rlPushMatrix();
    rlTranslatef(base.x, base.y, base.z);
    rlRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
    if (lying) rlRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    DrawCube({0.0f, 0.85f, -0.18f}, 0.28f, 1.7f, 0.3f, yellow);   // legs
    DrawCube({0.0f, 0.85f, 0.18f}, 0.28f, 1.7f, 0.3f, yellow);
    DrawCube({0.0f, 1.95f, 0.0f}, 0.55f, 1.25f, 0.95f, yellow);   // torso
    DrawCube({0.0f, 2.25f, 0.0f}, 0.57f, 0.18f, 0.97f, black);    // chest stripe
    DrawSphere({0.0f, 2.95f, 0.0f}, 0.34f, yellow);                // head
    DrawCube({0.34f, 2.95f, 0.0f}, 0.05f, 0.34f, 0.34f, black);    // face marking
    rlPopMatrix();
}

void drawTarget(const Target& t) {
    const Vector3 p = vec(t.position);
    const float yaw = static_cast<float>(t.yawDegrees);
    const int v = t.variant;
    if (t.kind == TargetKind::CrashDummy) {
        if (!t.destroyed) { drawCrashDummy(p, false, yaw); return; }
        const float age = static_cast<float>(t.destroyedAge);
        const float f = std::min(1.0f, age / 1.2f);
        const float lift = age < 1.2f ? 9.0f * std::sin(3.14159f * age / 1.2f) : 0.0f;
        drawCrashDummy({p.x + static_cast<float>(t.fling.x) * f, p.y + lift + (age < 1.2f ? 0.0f : 0.0f),
            p.z + static_cast<float>(t.fling.z) * f}, true, yaw + age * 40.0f * (1.0f - f));
        return;
    }

    const Color concrete{148, 150, 156, 255};
    const Color olive{86, 98, 72, 255};
    const Color burnt{44, 40, 38, 255};
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(yaw, 0.0f, 1.0f, 0.0f);
    switch (t.kind) {
        case TargetKind::Building: {
            const float w = 18.0f + static_cast<float>(v % 3) * 6.0f;
            const float d = 16.0f + static_cast<float>(v % 2) * 6.0f;
            const float h = 10.0f + static_cast<float>(v % 3) * 5.0f;
            if (!t.destroyed) {
                DrawCube({0.0f, h * 0.5f, 0.0f}, w, h, d, concrete);
                DrawCube({0.0f, h + 0.3f, 0.0f}, w + 0.6f, 0.6f, d + 0.6f, {96, 98, 104, 255});
            } else {
                DrawCube({0.0f, 0.9f, 0.0f}, w * 0.9f, 1.8f, d * 0.9f, burnt);
                DrawCube({w * 0.25f, 2.4f, -d * 0.2f}, 4.0f, 3.0f, 3.5f, {70, 66, 64, 255});
                DrawCube({-w * 0.3f, 1.8f, d * 0.25f}, 3.0f, 2.4f, 4.0f, {82, 78, 74, 255});
            }
            break;
        }
        case TargetKind::Tower:
            if (!t.destroyed) {
                DrawCylinder({0.0f, 0.0f, 0.0f}, 2.2f, 3.0f, 36.0f, 10, {170, 172, 178, 255});
                DrawCube({0.0f, 37.5f, 0.0f}, 9.0f, 4.5f, 9.0f, {120, 150, 170, 255});
            } else {
                DrawCylinder({0.0f, 0.0f, 0.0f}, 3.0f, 3.0f, 3.5f, 10, burnt);
                DrawCube({6.0f, 1.2f, 2.0f}, 8.0f, 2.4f, 5.0f, {70, 66, 64, 255});
            }
            break;
        case TargetKind::Truck:
            DrawCube({0.0f, 1.9f, 0.0f}, 8.0f, 3.0f, 3.2f, t.destroyed ? burnt : olive);
            DrawCube({4.9f, 1.7f, 0.0f}, 2.4f, 2.6f, 3.0f, t.destroyed ? burnt : Color{70, 80, 60, 255});
            DrawCube({-0.5f, 0.6f, 0.0f}, 9.0f, 1.0f, 2.6f, {30, 30, 32, 255});
            break;
        case TargetKind::AntiAirVehicle: {
            // Inert, static gun-truck prop. It never moves, tracks or fires.
            const Color hull = t.destroyed ? burnt : Color{74, 84, 62, 255};
            DrawCube({0.0f, 1.6f, 0.0f}, 9.0f, 2.2f, 4.2f, hull);
            DrawCube({0.0f, 3.4f, 0.0f}, 3.4f, 1.6f, 3.2f, t.destroyed ? burnt : Color{64, 72, 54, 255});
            if (!t.destroyed) {
                DrawCylinderEx({1.2f, 3.6f, 0.7f}, {5.6f, 5.6f, 0.7f}, 0.22f, 0.22f, 8, {40, 40, 44, 255});
                DrawCylinderEx({1.2f, 3.6f, -0.7f}, {5.6f, 5.6f, -0.7f}, 0.22f, 0.22f, 8, {40, 40, 44, 255});
            }
            DrawCube({0.0f, 0.5f, 0.0f}, 9.5f, 1.0f, 3.8f, {28, 28, 30, 255});
            break;
        }
        case TargetKind::CrashDummy: break;
    }
    rlPopMatrix();

    if (t.destroyed && t.destroyedAge < 14.0 && t.kind != TargetKind::CrashDummy) {
        const float a = 1.0f - static_cast<float>(t.destroyedAge) / 14.0f;
        for (int i = 0; i < 3; ++i) {
            const float rise = static_cast<float>(t.destroyedAge) * (3.0f + static_cast<float>(i));
            DrawSphere({p.x + static_cast<float>(i - 1) * 1.5f, p.y + 4.0f + rise, p.z},
                2.2f + static_cast<float>(i) * 1.2f + static_cast<float>(t.destroyedAge) * 0.5f,
                withAlpha({62, 60, 60, 255}, 150.0f * a));
        }
    }
}

void drawBlast(const Blast& b) {
    const float t = static_cast<float>(b.age / b.duration);
    const float fade = std::pow(std::max(0.0f, 1.0f - t), 1.2f);
    const float R = static_cast<float>(b.radius);
    const Vector3 c = vec(b.position);
    if (b.kind == BombKind::Nuclear) {
        // Arcade mushroom cloud: ground flash, fireball, stem and cap. Visual only.
        const float age = static_cast<float>(b.age);
        const auto ease = [](float x) { x = std::clamp(x, 0.0f, 1.0f); return 1.0f - (1.0f - x) * (1.0f - x); };
        const float fire = R * 0.55f * ease(age / 3.0f);
        const float rise = R * 0.45f * ease(age / 9.0f);
        const float stemH = R * 1.5f * ease(age / 6.0f);
        const float capR = R * 0.62f * ease(age / 8.0f);
        DrawCircle3D({c.x, c.y + 0.5f, c.z}, R * ease(age / 1.2f), {1.0f, 0.0f, 0.0f}, 90.0f, withAlpha({255, 238, 200, 255}, 230.0f * fade));
        DrawSphere({c.x, c.y + fire * 0.8f + rise * 0.3f, c.z}, fire, withAlpha({255, 170, 50, 255}, 235.0f * fade));
        DrawSphere({c.x, c.y + fire * 0.8f + rise * 0.3f, c.z}, fire * 0.6f, withAlpha({255, 245, 190, 255}, 255.0f * std::max(0.0f, 1.0f - age / 4.0f)));
        if (age > 0.8f) {
            DrawCylinderEx(c, {c.x, c.y + stemH, c.z}, R * 0.16f, R * 0.09f, 14, withAlpha({140, 108, 84, 255}, 215.0f * fade));
            DrawSphere({c.x, c.y + stemH + capR * 0.35f, c.z}, capR, withAlpha({176, 112, 68, 255}, 220.0f * fade));
            DrawSphere({c.x, c.y + stemH + capR * 0.55f, c.z}, capR * 0.68f, withAlpha({214, 148, 84, 255}, 200.0f * fade));
            DrawCylinderEx({c.x, c.y + stemH, c.z}, {c.x, c.y + stemH + capR * 0.08f, c.z}, capR * 1.18f, capR * 1.0f, 18, withAlpha({96, 80, 70, 255}, 190.0f * fade));
        }
        return;
    }
    const float grow = std::min(1.0f, static_cast<float>(b.age) / 0.45f);
    const float fire = R * (0.25f + 0.45f * std::sqrt(grow));
    const bool incendiary = b.kind == BombKind::Incendiary;
    const Color outer = incendiary ? Color{235, 70, 20, 255} : Color{255, 150, 40, 255};
    const Color inner = incendiary ? Color{255, 190, 60, 255} : Color{255, 236, 150, 255};
    DrawCircle3D({c.x, c.y + 0.4f, c.z}, R * std::min(1.0f, static_cast<float>(b.age) / 0.5f), {1.0f, 0.0f, 0.0f}, 90.0f,
        withAlpha({255, 255, 255, 255}, 150.0f * fade));
    DrawSphere({c.x, c.y + fire * 0.7f, c.z}, fire, withAlpha(outer, 225.0f * fade));
    DrawSphere({c.x, c.y + fire * 0.7f, c.z}, fire * 0.55f, withAlpha(inner, 240.0f * std::max(0.0f, 1.0f - t * 1.6f)));
    DrawSphere({c.x, c.y + R * 0.25f + R * 0.5f * t, c.z}, R * (0.25f + 0.35f * t), withAlpha({70, 66, 64, 255}, 150.0f * fade));
}

void drawOrdnance(const OrdnanceField& field, const Vec3& cameraFocus) {
    (void)cameraFocus;
    const Vec3 rc = field.rangeCenter();
    const Vector3 c{static_cast<float>(rc.x), static_cast<float>(rc.y) + 0.45f, static_cast<float>(rc.z)};
    DrawCircle3D(c, 130.0f, {1.0f, 0.0f, 0.0f}, 90.0f, {235, 235, 235, 120});
    DrawCircle3D(c, 126.0f, {1.0f, 0.0f, 0.0f}, 90.0f, {200, 40, 40, 120});
    for (const Target& t : field.targets()) drawTarget(t);
    for (const Bomb& b : field.bombs()) {
        const float scale = b.kind == BombKind::Nuclear ? 1.7f : (b.bomblet ? 0.45f : 1.0f);
        const Vector3 p = vec(b.position);
        const Vec3 dir = b.velocity.normalized();
        DrawLine3D(p, {p.x - static_cast<float>(dir.x) * 9.0f, p.y - static_cast<float>(dir.y) * 9.0f,
            p.z - static_cast<float>(dir.z) * 9.0f}, {230, 230, 230, 150});
        const Color body = b.kind == BombKind::Nuclear ? Color{210, 210, 214, 255}
            : b.kind == BombKind::Incendiary ? Color{190, 60, 30, 255} : Color{58, 62, 56, 255};
        DrawSphere(p, 0.9f * scale, body);
        DrawCube({p.x - static_cast<float>(dir.x) * 1.4f * scale, p.y - static_cast<float>(dir.y) * 1.4f * scale,
            p.z - static_cast<float>(dir.z) * 1.4f * scale}, 0.3f * scale, 1.6f * scale, 1.6f * scale, body);
    }
    for (const Blast& b : field.blasts()) drawBlast(b);
}

Color skyColor(const WeatherState& weather, double hours) {
    Color sky = {102, 153, 202, 255};
    const double sun = std::max(0.0, std::sin((hours - 6.0) * kPi / 12.0));
    const unsigned char daylight = static_cast<unsigned char>(70.0 + sun * 185.0);
    sky = {static_cast<unsigned char>(daylight * 0.55), static_cast<unsigned char>(daylight * 0.73), daylight, 255};
    if (weather.activePreset == WeatherPreset::Cloudy || weather.activePreset == WeatherPreset::Overcast) sky = {103, 118, 139, 255};
    if (weather.activePreset == WeatherPreset::Rain) sky = {74, 88, 107, 255};
    if (weather.activePreset == WeatherPreset::Storm) sky = {42, 49, 68, 255};
    if (weather.activePreset == WeatherPreset::Fog) sky = {154, 165, 171, 255};
    if (weather.activePreset == WeatherPreset::Snow) sky = {143, 161, 176, 255};
    if (hours < 5.0 || hours > 21.0) sky = {10, 17, 35, 255};
    else if (hours < 7.0 || hours > 19.0) sky = {164, 111, 91, 255};
    return sky;
}

} // namespace

void WorldRenderer::draw(const Simulation& simulation, const ReplayFrame* replayFrame, bool cockpitView) {
    const AircraftSnapshot state = replayFrame ? replayFrame->aircraft : simulation.aircraft().snapshot();
    const WeatherState& activeWeather = replayFrame ? replayFrame->weather : simulation.weather();
    const MapConfig& activeMapConfig = replayFrame ? replayFrame->mapConfig : simulation.mapConfig();
    const double hours = replayFrame ? replayFrame->timeOfDay.hours : simulation.timeOfDay().hours;
    const double worldTime = replayFrame ? replayFrame->timeSeconds : simulation.timeSeconds();
    if (!hasGeneratedMap_ || generatedMap_.config != activeMapConfig) {
        generatedMap_ = generateMap(activeMapConfig);
        hasGeneratedMap_ = true;
    }

    const Vec3 forward = state.orientation.rotate({1.0, 0.0, 0.0}).normalized();
    const Vec3 up = state.orientation.rotate({0.0, 1.0, 0.0}).normalized();
    const int presetIndex = replayFrame ? replayFrame->aircraftPresetIndex : simulation.aircraftPresetIndex();
    const AircraftDesign& design = fleetEntry(presetIndex).design;
    const double scaledLength = static_cast<double>(design.length * kAircraftRenderScale);
    const double dist = clamp(scaledLength * 2.6, 26.0, 120.0);
    if (cockpitView) {
        camera_.position = toRay(state.position + forward * (scaledLength * 0.3) + up * (design.bodyHeight * 0.45));
        camera_.target = toRay(state.position + forward * 100.0 + up * 1.0);
    } else if (lookBack_) {
        camera_.position = toRay(state.position + forward * dist + up * (dist * 0.4) + Vec3{0.0, dist * 0.14, dist * 0.58});
        camera_.target = toRay(state.position - forward * (scaledLength * 0.3 + 90.0));
    } else {
        camera_.position = toRay(state.position - forward * dist + up * (dist * 0.4) + Vec3{0.0, dist * 0.14, dist * 0.58});
        camera_.target = toRay(state.position + forward * (scaledLength * 0.3));
    }
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = cockpitView ? 78.0f : 58.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    ClearBackground(skyColor(activeWeather, hours));
    // raylib's default 0.05 near plane leaves too little depth precision at range, which
    // z-fights thin runway/apron slabs against the terrain height field. Keep the near
    // plane as far out as each camera allows and size the far plane to the generated world.
    rlSetClipPlanes(cockpitView ? 0.5 : 4.0,
        std::max(6000.0, static_cast<double>(generatedMap_.worldExtent) * 4.0));
    BeginMode3D(camera_);
    drawTerrain(generatedMap_, state.position);

    const int cloudCount = activeWeather.activePreset == WeatherPreset::Clear ? 8 : 22;
    for (int i = 0; i < cloudCount; ++i) {
        const float cloudDrift = static_cast<float>(std::fmod(worldTime * (3.5 + activeWeather.wind.length() * 0.12), 1900.0));
        const float x = static_cast<float>(state.position.x) - 950.0f
            + std::fmod(static_cast<float>(i) * 98.0f + cloudDrift, 1900.0f);
        const float z = static_cast<float>(state.position.z) + 380.0f + static_cast<float>((i * 73) % 420);
        const float y = static_cast<float>(activeWeather.cloudBaseMeters) * 0.45f + static_cast<float>(i % 3) * 12.0f;
        const Color cloud = activeWeather.activePreset == WeatherPreset::Storm
            ? Color{106, 112, 124, 235} : Color{210, 218, 226, 205};
        DrawSphere({x, y, z}, 18.0f + static_cast<float>(i % 4) * 5.0f, cloud);
    }

    if (activeWeather.rainIntensity > 0.1) {
        const int rainCount = static_cast<int>(45.0 + activeWeather.rainIntensity * 100.0);
        for (int i = 0; i < rainCount; ++i) {
            const float x = static_cast<float>(state.position.x) + static_cast<float>((i * 37) % 250 - 125);
            const float y = static_cast<float>(state.position.y) + 40.0f
                + std::fmod(static_cast<float>(i * 19) + static_cast<float>(worldTime * 92.0), 120.0f);
            const float z = static_cast<float>(state.position.z) + static_cast<float>((i * 61) % 250 - 125)
                + static_cast<float>(worldTime * activeWeather.wind.z * 0.7);
            DrawLine3D({x, y, z}, {x - 1.5f, y - 6.0f, z + 0.8f}, {196, 207, 221, 135});
        }
    }
    if (activeWeather.snowIntensity > 0.1) {
        for (int i = 0; i < 110; ++i) {
            const float x = static_cast<float>(state.position.x) + static_cast<float>((i * 47) % 270 - 135);
            const float y = static_cast<float>(state.position.y) + 22.0f
                + std::fmod(static_cast<float>(i * 31) + static_cast<float>(worldTime * 8.0), 105.0f);
            const float z = static_cast<float>(state.position.z) + static_cast<float>((i * 67) % 270 - 135)
                + static_cast<float>(worldTime * activeWeather.wind.z * 0.25);
            DrawSphere({x, y, z}, 0.8f + static_cast<float>(i % 3) * 0.4f, {236, 243, 248, 220});
        }
    }

    drawOrdnance(simulation.ordnance(), state.position);
    drawAircraftModel(design, state);
    EndMode3D();

    for (const Blast& blast : simulation.ordnance().blasts()) {
        if (blast.kind == BombKind::Nuclear && blast.age < 1.6) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                {255, 250, 235, static_cast<unsigned char>(235.0 * (1.0 - blast.age / 1.6))});
        }
    }

    if (activeWeather.fogDensity > 0.0) {
        const unsigned char alpha = static_cast<unsigned char>(clamp(activeWeather.fogDensity * 115.0, 0.0, 160.0));
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {190, 199, 204, alpha});
    }
    if (activeWeather.lightningIntensity > 0.0) {
        const double phase = std::fmod(worldTime, 13.0);
        if (phase < 0.12) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {213, 225, 247,
                static_cast<unsigned char>(activeWeather.lightningIntensity * 95.0)});
        }
    }
}

} // namespace skyframe
