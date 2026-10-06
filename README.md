# Summer Client

Legit-focused cheat client for Minecraft Java **1.21.x**, injected as a JVMTI agent.
The DLL drives the game through JNI/JVMTI only — no Fabric/Forge mod, no memory
patching. The overlay is drawn from a `glfwSwapBuffers` detour with Dear ImGui.

> Educational / testing use only. Using this on online servers may get you banned.

## Features

- **Menu** — dark styled click GUI, toggle with `INSERT` (rebindable), search
  and keybind every entry.
- **Watermark** — client name + fps in the top-left corner, can be disabled.
- **Config** — save/load from the menu, class mapping dump for obfuscated
  names.

Modules (combat / visual / movement) are not in the build right now.

## Building

Requires CMake, a C++17 toolchain (MSVC or MinGW) and a JDK. Or just run the
GitHub Actions workflow and download `summer_client` from the artifacts.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

JNI headers are taken from `JAVA_HOME`. Dear ImGui is vendored in
`third_party/imgui` (v1.92.0), MinHook is fetched by CMake.

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
src/summer/             client core, config, JVM/JVMTI helpers, module base
src/mc/                 Minecraft bindings (JNI reflection layer)
src/gui/                Dear ImGui click GUI
src/overlay/            swap-hook render pass
src/math/ src/util/     math + helpers
third_party/imgui/      Dear ImGui v1.92.0 (vendored)
```
