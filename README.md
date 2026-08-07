# Summer Client

Legit-focused cheat client for Minecraft Java **1.21.x**, injected as a JVMTI agent.
The DLL drives the game through JNI/JVMTI only — no Fabric/Forge mod, no memory
patching. The overlay is drawn from a `glfwSwapBuffers` detour with Dear ImGui.

> Educational / testing use only. Using this on online servers may get you banned.

## Features

- **Menu** — CS:GO-style window, toggle with `INSERT`, keybind any module.
- **Hitbox** — aims at the real entity hitbox:
  - *Normal*: smooth stick aim (FOV, range, random hit points).
  - *Legit*: snap to the hitbox, fire, then return the camera to where it was
    with randomized error and a post-hit delay. Head/chest/feet bias.
- **TriggerBot** — raycasts the crosshair against real hitboxes and attacks
  with random scan/fire delays.
- **AutoSwap** — scores hotbar weapons (sword > mace > axe > trident, then
  material) and swaps while fighting, with optional switch-back.
- **ESP** — 2D box, 3D hitbox, health bar, name, distance, tracers. Player / mob /
  invisible filters, color-coded teams.
- **Removals** — fullbright, no hurt-cam, no fire overlay, no view bobbing.
- **AutoSprint** — sprints while moving forward (stops while sneaking, food check,
  randomized engage delay).
- **ScreenWalk** — keeps movement keys applied while a GUI screen (inventory, chat)
  is open.

## Building

Requires CMake, a C++17 toolchain (MSVC or MinGW) and a JDK. Or just run the
GitHub Actions workflow and download `summer_client` from the artifacts.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

JNI headers are taken from `JAVA_HOME`. Dependencies (Dear ImGui, MinHook) are
fetched by CMake.

## Loading

Start Minecraft with the agent attached:

```
java -agentpath:path\to\summer_client.dll -jar minecraft.jar
```

## Configuration

- Config + log live in `%APPDATA%\SummerClient\` (`SummerClient.cfg`, `summer.log`).
- If Minecraft's Yarn names differ from the ones baked in, dump the class
  mappings from the menu (SETTINGS → *Dump class mappings*), then override any
  field/method in the config under `map.<key>`, e.g.:

  ```
  map.Minecraft.player=playerFieldName
  map.Entity.getX=getXField
  ```

## Layout

```
src/main.cpp            JVMTI agent entry + glfwSwapBuffers detour
src/summer/             client core, config, JVM/JVMTI helpers, module registry
src/mc/                 Minecraft bindings (JNI reflection layer)
src/gui/                Dear ImGui menu
src/overlay/            swap-hook render pass
src/modules/            combat / visual / movement modules
src/math/ src/util/     math + helpers
```
