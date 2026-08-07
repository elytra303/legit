# Summer Client

Cheat client for Minecraft Java 1.21.11, injected as a JVMTI agent.

## Features

- INSERT — open/close menu (CS:GO style)
- Hitboxes — Off / Normal / Legit (auto-aim on click, FOV-limited, delay, restores your view)
- TriggerBot — auto-attack when crosshair hits a player
- AutoSwap — auto-switches to best sword/axe when attacking (back-swap optional)
- ESP — box, tracers, health, distance
- Removals — remove fire overlay, fog, nausea
- Sprint — auto-sprint (W + no sneak)
- ScreenWalk — move while a screen (inventory, chat) is open
- HUD — FPS, coordinates, direction, active modules

## Building

Requires CMake and MSVC (or run GitHub Actions):

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

JNI headers are taken from `JAVA_HOME`.

## Loading

Start Minecraft (1.21.11) with:

```
java -agentpath:path\to\summer_client.dll -jar minecraft.jar
```
