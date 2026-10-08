# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Response style

Follow Zinsser's four principles of quality writing:
1. Simplicity
2. Brevity
3. Clarity
4. Humanity

Additionally, communicate in ASD-STE100, or Simplified Technical English. Cut clutter, give each word one
meaning, and don't always reach for highly abstract verbiage. As a reminder some of those core rules are:

- Make instructions as clear and specific as possible.
- Do not write multi-word nouns that have more than three words.
- Use the approved forms of verbs to make only:
  - The infinitive form
  - The imperative form
  - The simple present tense
  - The simple past tense
  - The simple future tense
  - The past participle (only as an adjective)
- Do not use auxiliary verbs to make complex verb constructions.
- Use the "-ing" form of a verb only as a technical noun or as a modifier in a technical noun.
- Use the active voice. In descriptive writing, one should use the passive voice only when the agent is unknown.
- Write short sentences: no more than 20 words in instructions (procedures) and 25 words in descriptive texts.
- Do not omit parts of the sentence (e.g. verb, subject, article) to make the text shorter.
- Use vertical lists for complex text.
- Write one instruction per sentence.
- Write only one topic per paragraph.
- Do not write more than six sentences in each paragraph.
- Start safety or performance instructions with a clear command or condition.

## Project overview

VeloxRhi is a C++23 WebGPU convenience wrapper for packaging small graphical demos (deployed to GitHub
Pages via Emscripten). It is explicitly **not** a game engine — no input-driven workflow tooling is a
goal, just enough scaffolding (context bootstrap, app lifecycle, math, shader cooking) to stand up demos
that share code between native and web builds. Uses Dawn (WebGPU), GLFW (native windowing), Slang
(shaders), Dear ImGui, and DirectXMath/WASM-SIMD128 for math.

## Build

Build is CMake + Ninja Multi-Config, driven entirely through `CMakePresets.json` (no vcpkg/conan;
dependencies are git submodules under `third_party/`: glfw, slang, imgui, dawn, magic_enum).

Configure once per preset, then build a specific configuration:

```bash
# Native, MSVC
cmake --preset ninja-msvc
cmake --build --preset windows-ninja-msvc-debug
cmake --build --preset windows-ninja-msvc-relwithdebinfo

# Native, Clang-CL
cmake --preset ninja-clang-cl
cmake --build --preset windows-ninja-clang-debug
cmake --build --preset windows-ninja-clang-relwithdebinfo

# Emscripten/web (requires EMSDK env var set)
cmake --preset emscripten
cmake --build --preset emscripten-build-debug
cmake --build --preset emscripten-build-relwithdebinfo
cmake --build --preset emscripten-build-release
cmake --build --preset emscripten-build-minsizerel
```

### Building outside VS Code (agents: read this first)

The presets above assume the MSVC environment is already present. VS Code's CMake Tools injects it from
the selected kit, so bare `cmake` works in its integrated terminal but **fails from a plain shell** with
`CMAKE_CXX_COMPILER: cl is not a full path and was not found in the PATH`. Locate the toolchain with
`vswhere` and wrap the command:

```bash
cmd /c "call \"<VS_INSTALL>\VC\Auxiliary\Build\vcvars64.bat\" >nul 2>&1 && <cmake command>"
```

The configure and build commands used in practice, which bypass the presets:

```bash
cmake -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
      -S D:/VeloxRhi -B D:/VeloxRhi/build/ninja-msvc -G "Ninja Multi-Config"
cmake --build D:/VeloxRhi/build/ninja-msvc --config Debug --target <target> --
```

**A failed configure is expensive.** It clobbers the generated `build-<config>.ninja` includes and forces
a full Slang rebuild (~10 minutes). The Slang core-module bootstrap step can also report failure once and
succeed on a plain retry — retry before investigating. Confirm the environment is right *before*
configuring, never by trying it.

Serving a built web demo locally: use the VS Code task "Serve VeloxRhi demo with EmRun"
(`.vscode/tasks.json`), which runs `emrun --no_browser --port 8080 <target>.html` from the demo's
build output directory.

Requires Python3 at configure time — it generates Ocean FFT LUTs and a `.slang` shader
(`tools/scripts/ocean_fft_support.py` + `OceanFftConfig.yaml`) into `<build>/generated`.
Non-Emscripten builds also require the Vulkan SDK (`find_package(Vulkan REQUIRED)`, resolved via the
custom `cmake/FindVulkanHeaders.cmake`, which must load before any other `find_package` call).

`VELOX_RHI_BUILD_DEMOS` (CMake option, default `ON`) controls whether `demos/` is built.
`VELOX_RHI_BUILD_TESTS` (default `ON`) controls whether `tests/unit_tests/` is built.
`VELOX_RHI_MATH_RELAXED_SIMD` (default `ON`) opts the WASM math backend into relaxed-simd; it both
passes `-mrelaxed-simd` and defines `VX_MATH_RELAXED_SIMD`, which must stay coupled. Note relaxed-simd
does *not* degrade gracefully — a module containing those opcodes fails validation outright on an
engine without support, so turn this OFF to widen browser coverage.

## Testing

`tests/unit_tests/` holds standalone test executables — deliberately **not** GTest or Catch2. Each is
a plain program built on `TestHarness.hpp` (a check counter and a relative-tolerance comparison) that
prints only failures plus a one-line summary, and returns nonzero on failure. Run one directly, or use
`ctest` from the build directory. Under the Emscripten preset they build to `.js`/`.wasm` and are
registered to run through `node`.

`MathTests` is the one that matters most: broken math is otherwise only discoverable by staring at
wrong pixels. It leans on identities and known-answer cases rather than golden values, because the
failures worth catching look plausible — a transposed shuffle index or a dropped cofactor sign still
returns numbers of roughly the right size. **Run it on both backends after touching `include/math/`**;
they are independent implementations and only the tests keep them honest about semantics. The WASM
side needs both the relaxed and non-relaxed configurations, since those select different intrinsics
inside the same functions.

Also `tools/scripts/test_ifft.py`, a standalone Python sanity check for the Ocean FFT LUT generator —
run it directly with `python tools/scripts/test_ifft.py` if touching that pipeline. There are no CI
workflows. Verify rendering changes by building and running a demo.

Host tools have no demo to run, so verify them by running the tool and reading its output. For
`shader_cooker`, a cook that exits `0` means every variant compiled *and* its extracted reflection
matched the `@group`/`@binding` attributes in the emitted WGSL — that cross-check is the tool's own
regression test, and it hard-fails the cook on mismatch. Run it against `assets/shaders/compute/OceanFft.slang`
after any change to the cooker.

**PowerShell gotcha**: the cooker (and most native tools here) log to stderr. Do not use `2>&1` on a
native executable in PowerShell 5.1 — it wraps each stderr line in a `NativeCommandError` and reports a
bogus non-zero exit code even on success. Pipe to `Out-Null` and read `$LASTEXITCODE` instead.

## Code style

- Formatting: `.clang-format` — LLVM base, 4-space indent, 110-column limit, Allman braces, left-aligned
  pointers, always-break template declarations, no bin-packing of args/params.
- Static analysis: `.clang-tidy` is present and expected to be respected.

### Code Style, elaborated from the Repo Author


#### Code Formatting Rules
- **Single-line if statements**: NEVER allowed. All if statements must include brackets placed on a newline
- **Function implementations**: Eagerly define in source. No lazy implementations in headers — not getters,
  not setters, not one-line functions. Templates and `constexpr` functions intended for compile-time
  evaluation sometimes force our hand (`Future.hpp`, `SlotMap.hpp`, generated permutation-key code); that's
  just how it goes, and is not license to inline anything else
- **Indentation**: 4 spaces always (no tabs) for cross-platform consistency
- **Brackets**: Always go on new lines
- **Control Flow**: Always use braces for if statements, even single-line ones
- **Naming**: PascalCase for public APIs, camelCase for private members, snake_case for parameters
- **Single-word parameters**: snake_case and camelCase converge for a single token, so a single-word
  parameter can silently collide with a member of the same name. Prefix those with an underscore
  (`_instance`, `_createInfo`). Most compilers still resolve the member initialization correctly, but
  it is a silent killer when they don't, and the prefix is free. Multi-word parameters stay snake_case
  and need no prefix (`create_info`, `module_path`)
- **Member prefixes**: `m_` must NEVER be used as prefix for member variables
- **Constructor initializers**: Colon on same line as declaration, each initializer on new line with trailing comma:
```cpp
Struct::Struct(int _val0, int _val1, int _val2) :
  val0{ _val0 },
  val1{ _val1 },
  val2{ _val2 }
{}
```
- **Switch Statements**: if a case is going to do more than return a value or call a function, pull that logic out into a separate function with a descriptive name. If brackets would need to be inserted to initialize variables in the case: pull it out into a separate function. Treat switch statements in this usage like a table of functions to be called
- **Comment usage**: Avoid as much as absolutely possible. Comments are no subsititude for descriptive code: I would rather have function names that are 80 characters long than comments that will rapidly drift from the source. Absolutely no comments depicting categories of code: that should be inferred from how functions are grouped (in the same order as they are declared, and in declaration order in the definition file)
- **Local variables**: If doing repeated operations, prefer longer variable names. use `deltaX` instead of `dX`, assign variables to const during long chains of mathematical operations (almost like writing scalarized SSA code), and favor being readable over being clever or taking shortcuts. We will save shortcuts and esoteric performant code for profiling results
- **Eagerly factor out common logic**: If some bit of code is greater than 4-5 lines and being duplicated, factor it out into a common function.
- **`todo` comments**: Spare these for things that are actually worth having greppable as distinct work items. For things that will need to be fixed before shipping this to customers or clients who are not devs or friends: use `todo-ship`. use `todo-perf` for things that could grant sizeable performance benefits. Minimize the usage of `todo` as much as possible: we need to get to MVP, but we also don't need to fill our backlog on that road.

Examples of well formatted code in this codebase: `Future.hpp`, `InputManager.hpp` + `InputManager.cpp`, `Context.hpp` + `Context.cpp`. 

#### C++ Language Preferences
- **Functions**: No implementations in headers; mark `constexpr` and `noexcept` when possible
- **Constructors**: Should be `noexcept` when possible
- **Move/copy operators**: Define `noexcept` versions when beneficial
- **Auto usage**: Minimize except for iterators/complex nested types (e.g., `auto iter = map.find(key)` OK, `auto value = vector.front()` not OK)
- **Virtual classes**: Use `final` when possible to collapse vtables and improve performance
- **Error handling**: Use `Result` types for function return status within RHI code; avoid exceptions. A result type is just `std::expected` with an error code enum as the unexpected value. When working outside the RHI, declare an error code for that subsystem and use that as appropriate. Don't leak error codes
- **Subsystem error pattern**: a subsystem outside the core RHI declares its own enum plus its own alias,
  and never borrows `RhiError`. `tools/shader_cooker` is the reference: `CookError` +
  `template<typename T> using CookResult = std::expected<T, CookError>;` in `CookerErrors.hpp`, with a
  `ToString(CookError)` declared there and defined in the matching source file
- **`Result<void>`**: `std::expected<void, E>` is the return type for "can fail, yields nothing." Return a
  braced `{}` for success and `std::unexpected(Error::Whatever)` for failure. Propagate by returning the
  `Result` itself rather than re-wrapping it
- **Error logging**: For debug code or code that will be executed only on native, log with `std::println` frequently and often if it will help debugging. Same philosophy as comments though: do not fill it up for the sake of saying something.
- **Dynamic allocation**: avoid as much as possible, whenever possible. If required, allocate carefully, reserve upfront, and do not let memory persist

#### Enum Formatting
- Use smallest bitwidth type possible, always prefer `enum class` for scoping
- **Every enum reserves `0` for `Invalid`**, so a zero-initialized or memset value is never mistaken for
  a meaningful one. This mirrors how booleans behave: zero is the absence of a valid answer
- For result/error enums: `Invalid = 0`, then `Success` (or the equivalent "it worked" value) at `1`,
  then every error value beyond that
- For taxonomy enums (no success concept — `BindingKind`, `ShaderStageKind`, input event types): just
  `Invalid = 0`, then the values
- For bitmask enums: add operators for at least `|` and `&` operations
- Boolean conversion operators are preferable

#### Memory & Performance
- **Threading**: This project is not designed for massive threading but it is considered an important design goal
  that it is *thread-hardened*. Atomics are used where threads could compete over resources, and mutexes should
  be a reluctant object of absolute last resort. This app should be designed to scale to multiple threads, but 
  individual objects and functions should be viewed as single-threaded internally. Don't communicate by sharing
  memory, share memory by communicating.
  - **Message Passing**: as such, interfaces between modules of code that may need to talk to each  other should
    use message-passing paradigms. This allows for better thread isolation of work, and encourages a validate-
    try-commit model that is more recoverable.
- **Memory**: Avoid dynamic allocations as often as possible. On web targets, we are running in a virtual env
  with a pre-allocated linear span of memory. We must be frugal with memory, and should favor using statically
  allocated arenas and being efficient with our choice of datatypes.
- **Span**: Use `std::span` for array parameters instead of raw pointers + size, and for passing ranges
  of values between systems. 
- **String conversion**: Use `charconv` instead of C conversion functions for string/char to integral types
- **Error Handling**: Use `Result` types for function return status within RHI code; avoid exceptions. For code outside core Rhi, create a new enum class and use it with `std::expected` for error handling. Bubble up errors to the caller instead of logging and returning a default value.

#### Error Handling
- **Result<T>**: Use a rust-like Result<T> bubbled up through functions to return a value or an error
  code to users
- **Error Values**: Use an enum class of the minimum width required to convey the systems range of errors.
  Use 0 as an invalid initial value, 1 as success, and pin all further error codes to be > 1
- **Error Messages**: For `enum class` error values, provide an enum-to-stringview conversion function in
  a header that uses magic_enum in the source to retrieve the enum name. Further information may be appended
  to the view, but careful consideration of lifetime of error strings should be considered
- **Exceptions**: Exceptions are a tangled mess on web, especially with how event-loop-driven our app is.
  Avoid them as much as possible, and attempt to provide a way for modules of code to shutdown and restart
  in a known-good state to recover from errors.

#### Header Conventions
- Headers carry **both** `#pragma once` and a traditional `#ifndef`/`#define`/`#endif` guard. This is
  intentional and consistent across the repo — do not "clean it up" to one or the other. Guard macros are
  screaming-snake-case derived from the path (`VELOX_ASYNC_FUTURE_HPP`,
  `VELOX_SHADER_COOKER_ERRORS_HPP`), and the closing `#endif` carries a `// !GUARD_NAME` comment
- Keep standard-library includes minimal and sorted within a group; a header should include what it
  names and nothing more. Prefer forward declarations across module boundaries (see how `Context.hpp`
  forward-declares `Scheduler`)
- Hide third-party types behind a pimpl when a header would otherwise force them on every consumer.
  `tools/shader_cooker/include/SlangCompiler.hpp` names no Slang type for exactly this reason

### C++ Standard Library Usage
- Use `std::upper_bound` and `std::lower_bound` from `<algorithm>` when possible
- Retrieve numerical constants from `<numbers>` header
- Minimize standard library includes across module boundaries

## Architecture

**Library + demo-app pattern.** `include/`/`src/` build the `VeloxRhi` static library; `demos/<name>/`
are executables linked against it via the `add_velox_rhi_demo(NAME ...)` CMake helper
(`demos/CMakeLists.txt`), which handles native vs. Emscripten link flags and (on native Windows)
post-build DLL copying.

Both `include/` and `src/` are subdivided by subsystem: `common/` (errors, logging), `core/` (context,
application, scheduler/coroutine layer, input), `math/`, `utility/` (mesh generation, `SlotMap`). This
split happened as part of the move toward the rendergraph and an upcoming audio module — `include/audio/`
and `include/graph/` already exist as staging points for that work but aren't wired into `CMakeLists.txt`
yet. When adding new subsystem files, add them to the matching `VeloxRhi_<Subsystem>Sources` list (and
`source_group`) in the root `CMakeLists.txt`, not just the filesystem.

- **`velox::Context`** (`include/core/Context.hpp`, `src/core/Context.cpp`) owns the WebGPU Instance /
  Adapter / Device / Queue / Surface and centralizes setup. Bootstrap is staged and asynchronous via a
  `BootstrapPhase` enum (`Invalid → InstanceCreated → RequestingAdapter → RequestingDevice → Complete`)
  driven through `Result<BootstrapPhase> RunBootstrap()`. Owns a `Scheduler` for coroutine-driven async
  WebGPU calls, and exposes `AcquireNextFrame()` / `Present()` / `Resize()`.
- **`velox::Application`** (`include/core/Application.hpp`, `src/core/Application.cpp`) is the abstract
  lifecycle base class demos derive from (see `demos/triangle/Triangle.cpp`'s `TriangleApplication`).
  Holds a `Context*`, a `LifecyclePhase` enum, and a `FrameClock`; virtual hooks are `OnSetup()`,
  `OnResize(width, height)`, `OnUpdate()`, `OnRender(wgpu::TextureView&)` (pure virtual), `OnShutdown()`.
  `TickClock(double now)` clamps large delta-time spikes (e.g. after a debugger pause) before advancing
  the frame clock.
- **`ApplicationMainLoop(Context&, Application&)`** (free function) drives the loop from `main()`. On
  Emscripten, control never returns from this call — the loop is handed off to the browser and anything
  after it in `main()` will not run.
- **Async/coroutine layer**: `include/core/{Scheduler,Future,AsyncTasks,CoroutineAllocator}.hpp` wrap
  WebGPU's async callback APIs (adapter/device request, pipeline creation, buffer mapping) in
  `std::coroutine`-based awaitables. Errors propagate via `std::expected`-based `Result<T>` and the
  `RhiError` enum (`include/common/VeloxErrors.hpp`), used throughout the public API rather than
  exceptions.
- **`velox::math`** (`include/math/Math.hpp` + the rest of `include/math/`) — see `docs/math_handoff.md` before
  `docs/math-implementation-notes.md` for why the SIMD forms are shaped as they are. Ported from the
  author's other project,
  "DiamondDogs"; see the header's doc comment for what was intentionally dropped in that port (e.g.
  `ReciprocalEst`/`SqrtEst`, `Refract<N>`) and a flagged pre-existing inconsistency (`Perspective`/
  `Orthographic` default left-handed, `LookAt`/`LookTo` default right-handed — preserved, not "fixed").
  Two type families:
  - Storage types (`Float2/3/4`, `Float3x3/4x3/4x4`) — plain, constexpr, backend-agnostic, for
    persistence, with swizzle accessors.
  - SIMD types (`Vector`, `Matrix`, both `alignas(16)`) — backed by DirectXMath on native
    (`include/math/MathBackendDX.inl`) or WASM SIMD128 on Emscripten (`include/math/MathBackendWASM.inl`),
    selected in the root `CMakeLists.txt` by platform (or forced via `VX_MATH_FORCE_BACKEND_WASM` /
    `VX_MATH_FORCE_BACKEND_DIRECTX`). Not meant for storage — convert via `ToVector`/`FromVector`/
    `ToMatrix`/`FromMatrix`.
- **Input**: `include/core/InputManager.hpp`/`InputEvent.hpp` implement an FSM-based per-frame
  event/gesture system (`GetEventsForFrame()`, `GetGesturesForFrame()`).
- **Shaders**: Slang source lives under `assets/shaders/compute/` (e.g. the Ocean FFT compute pipeline).
  `tools/shader_cooker` is a native-only offline Slang→WGSL compiler, laid out as `include/`+`src/`
  mirroring the top level. It extracts binding reflection via Slang's binding-range API, validates it
  against the emitted WGSL, and bakes variants into a generated header. See
  `docs/shader-cooker-plan.md` for the design and its progress log. `ShaderUtils.hpp` provides runtime
  loading/compilation helpers.
- **Permutation axis names are a silent-failure surface.** A `PermutationAxis::Name` in the cooker must
  match an `extern const static` declaration in the Slang source *exactly*. If it doesn't, Slang links a
  symbol nobody references, the shader keeps its default value, and nothing errors — every variant cooks
  identical output. This has already happened once (`FFT_SIZE` vs `IFFT_SIZE`).

### Two different `generated/` directories

Easy to confuse; they are unrelated:
- **`<build>/generated/`** — produced at *configure* time by `tools/scripts/ocean_fft_support.py`:
  the LUT `.hpp`/`.cpp` pairs (`SinCosLUT`, `DonelanBannerNormLUT`) and the generated Slang snippet
  `OceanFFT_Generated.slang`. Not checked in; on the include path for the `VeloxRhi` target.
- **`include/generated/`** — checked-in output of `tools/shader_cooker`: the baked WGSL variant headers.
  Currently written by running the cooker by hand (it is not yet wired to an `add_custom_command`).
- **Rendering backend**: Dawn (`webgpu_dawn`/`dawn_native` native, `emdawnwebgpu_cpp` on Emscripten) for
  WebGPU, GLFW for native windowing, Dear ImGui (compiled directly from `third_party/imgui/*.cpp` into
  the `VeloxRhi` target, `imgui_impl_glfw`/`imgui_impl_wgpu` backends,
  `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` defined).
