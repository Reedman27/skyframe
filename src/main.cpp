#include "skyframe/render/world.hpp"
#include "skyframe/ui/hud.hpp"

#include <raylib.h>

#include <algorithm>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 900, "Skyframe - Flight Sandbox");
    // The HUD uses fixed-pixel panels; below ~725 px of height the right-hand and bottom
    // panels collide, so the supported minimum matches the 1280x720 target.
    SetWindowMinSize(1024, 720);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    skyframe::Simulation simulation;
    skyframe::WorldRenderer world;
    skyframe::Hud hud;
    bool cockpitView = false;

    while (!WindowShouldClose()) {
        const double dt = static_cast<double>(GetFrameTime());

        if (!hud.capturesKeyboard()) {
            if (IsKeyPressed(KEY_ESCAPE)) break;
            if (IsKeyPressed(KEY_SPACE) && !simulation.replay().isPlaying()) simulation.setPaused(!simulation.paused());
            if (IsKeyPressed(KEY_R)) {
                simulation.stopReplay();
                simulation.reset();
            }
            if (IsKeyPressed(KEY_C)) cockpitView = !cockpitView;
            if (IsKeyPressed(KEY_P)) simulation.startReplay();
            if (IsKeyPressed(KEY_O)) simulation.stopReplay();
            if (IsKeyPressed(KEY_X)) simulation.dropBomb();
            world.setLookBack(IsKeyDown(KEY_L));
            if (IsKeyPressed(KEY_Z)) simulation.cycleBomb(IsKeyDown(KEY_LEFT_SHIFT) ? -1 : 1);
        }

        if (!hud.capturesKeyboard() && !simulation.aircraft().flightEnded() && !simulation.replay().isPlaying()) {
            skyframe::FlightControls& controls = simulation.aircraft().controls();
            controls.pitch = (IsKeyDown(KEY_W) ? 1.0 : 0.0) - (IsKeyDown(KEY_S) ? 1.0 : 0.0);
            controls.roll = (IsKeyDown(KEY_D) ? 1.0 : 0.0) - (IsKeyDown(KEY_A) ? 1.0 : 0.0);
            controls.yaw = (IsKeyDown(KEY_E) ? 1.0 : 0.0) - (IsKeyDown(KEY_Q) ? 1.0 : 0.0);
            if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) controls.throttle += 0.32 * dt;
            if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) controls.throttle -= 0.32 * dt;
            controls.throttle = skyframe::clamp(controls.throttle, 0.0, 1.0);
            if (IsKeyPressed(KEY_G)) controls.gearDown = !controls.gearDown;
            if (IsKeyPressed(KEY_F)) controls.flaps = skyframe::nextFlapSetting(controls.flaps);
            if (IsKeyPressed(KEY_V)) controls.spoilers = !controls.spoilers;
            if (IsKeyPressed(KEY_B)) controls.parkingBrake = !controls.parkingBrake;
        }

        simulation.update(dt);
        const skyframe::ReplayFrame* replayFrame = simulation.replay().isPlaying()
            ? simulation.replay().currentFrame() : nullptr;

        BeginDrawing();
        world.draw(simulation, replayFrame, cockpitView);
        hud.draw(simulation, cockpitView, replayFrame);
        DrawFPS(GetScreenWidth() - 430, 14);  // free space between the title and aircraft panels
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
