# VeloxRhi RenderGraph — Design Brief

## Context & scope

VeloxRhi is a standalone WebGPU (webgpu_cpp) demo kit — not a port of the author's
larger DiamondDogs engine. The rendergraph described here is a deliberately
small-scale exploration of an idea the author has long wanted in DiamondDogs
(inspired by Our Machinery's frame-graph writing), tried here first because the
scope is tight and the API is simpler. It is explicitly **not** wired into
DiamondDogs' RhiResources, material system, or content compiler.

WebGPU-specific constraints that shape every decision below:
- No manual barriers/layout transitions — the API tracks resource hazards
  automatically. A rendergraph here is a cognitive/organizational aid, not a
  correctness mechanism.
- No bindless / descriptor indexing in portable WebGPU.
- Exactly one queue; no transfer-queue-family concept.
- WGSL shaders declare bindings inline (`@group(N) @binding(M)`), and the
  project's Slang→WGSL compile step can emit reflection data alongside them.

## Design goals, in priority order

1. **Declarative and expressive**, with minimal structures — reading a subpass
   declaration should tell you what it touches and depends on without tracing
   control flow.
2. **Reduce cognitive burden for maintainers.** Primary real-world motivation:
   engineers repeatedly misunderstanding or mishandling passes they didn't
   write. Explicit > implicit whenever the two trade off directly.
3. **Minimize manual resource-binding footguns.** Specifically: desync between
   what a shader actually declares (WGSL bindings) and what CPU-side code
   declares about it — a silent-drift bug class the author wants engineered
   out via build-time/startup validation against Slang's reflection output,
   not just discipline.
4. **Cheap to write, not just cheap to read** — DRY matters for repeated
   patterns (mesh draws, feature-gated variants), but never at the cost of (1).

## Original API sketch (author-provided, main.cpp)

Purely a feel/ergonomics exploration — not working code. Note the explicit
caveat in the source: `PipelineObject::ShaderModules` uses `std::span` "as a
stub... would totally explode without backing storage somewhere."

```cpp
struct RenderGraphSubpass
{
    RenderGraphSubpass& AddBuffer(BufferUsage buffer);
    RenderGraphSubpass& AddImage(ImageUsage image);
    RenderGraphSubpass& SetPipeline(PipelineObject pipeline);
    RenderGraphSubpass& AddDependency(RenderGraphSubpass& subpass);
    RenderGraphSubpass& AddDependency(std::string_view dependency_name);
    RenderGraphSubpass& SetAsRunOnce();
    RenderGraphSubpass& OverridePipelineIfFeatureSupported(
        PipelineObject pipelineToOverride, PipelineObject pipeline, std::string_view feature_name);
    ~RenderGraphSubpass(); // intent: dtor auto-registers into the graph
};

struct RenderGraphBuilder
{
    RenderGraphBuilder(std::string_view name);
    RenderGraphSubpass& StartSubpass(std::string_view name);
};
```

Usage shape (trimmed from the sketch's ocean-FFT example):

```cpp
auto SpectrumUpdateSubpass = graphBuilder.StartSubpass("SpectrumUpdate");
SpectrumUpdateSubpass.AddBuffer(spectrumBufferUsage)      // ReadWrite
                      .AddBuffer(waveDataBufferUsage)      // ReadWrite
                      .AddBuffer(waveParamsBufferUsage)    // ReadOnly, uniform
                      .SetPipeline(SpectrumUpdatePipeline)
                      .AddDependency("SpectrumInit");
```

## Resolved design decisions

### 1. Subpass builder lifetime and registration

**Problem:** `StartSubpass` returning `RenderGraphSubpass&` is unsafe two ways
at once. `auto x = builder.StartSubpass(...)` silently deduces a *copy*, not a
reference — mutating chain calls act on the copy, not graph state. Fixing that
to `auto&` instead creates a dangling-reference bug: if subpasses live in a
growable container (e.g. `std::vector`), a later `StartSubpass` call can
reallocate it, invalidating every reference handed out earlier — including the
`RenderGraphSubpass&` overload of `AddDependency`, which is therefore the more
fragile of the two dependency-declaration paths, not the string one.

**Resolution:** `StartSubpass` returns a small **builder-scope proxy by
value** (pointer/index back into the builder, accumulates the fluent calls).
The proxy's destructor commits the finished description into address-stable
storage (`std::deque<Subpass>` or `std::vector<std::unique_ptr<Subpass>>`).
This keeps the "auto-register via destructor" ergonomics the author wanted,
applied to a short-lived proxy rather than the long-lived graph node — no
reference ever outlives the container operation that could invalidate it.

```cpp
class SubpassBuilder // returned by value from StartSubpass
{
public:
    SubpassBuilder& AddBuffer(BufferUsage);
    SubpassBuilder& AddImage(ImageUsage);
    SubpassBuilder& SetPipeline(PipelineObject);
    SubpassBuilder& AddDependency(SubpassHandle);   // see §2
    ~SubpassBuilder(); // commits accumulated desc into graph's stable storage
private:
    RenderGraphBuilder* graph;
    SubpassDesc pending;
};
```

### 2. Dependency expression — two channels, not one

Kept deliberately separate rather than unified into a single mechanism:

- **Resource-declared dependencies** (the common case): if a resource is
  already declared via `AddBuffer`/`AddImage` for binding purposes, shared
  reads/writes of it imply ordering automatically — no separate dependency
  declaration needed. Validated against the ocean-FFT chain and generalizes to
  a post-process chain (HDR tonemap writing a color target, ODT reading it) —
  same mechanism, no special-casing.
- **Sentinel dependencies** (structural/non-data only): reserved strictly for
  ordering anchors with no resource behind them — `FrameStart`/`FrameEnd`.
  Represented as static sentinel objects of the *same handle type* as real
  subpasses (not magic strings), so `AddDependency` needs one signature, not
  two, and a typo can't silently produce a disconnected node.

**Rule to carry into implementation:** if the dependency has data behind it,
it's a resource declaration; if it's pure ordering with nothing behind it,
it's a sentinel. Don't let sentinels leak into the data-dependency case.

### 3. Feature-variant pipelines — `OverrideSubpass`

**Problem:** `OverridePipelineIfFeatureSupported` swapping only the pipeline
handle can't safely express variants that also need a different resource
schema (e.g. a subgroup-accelerated IFFT pipeline needing different scratch
buffers than the fallback).

**Resolution:** `OverrideSubpass` is a **fully independent, fully-declared
subpass** — its own bindings, its own dependency list — sharing
dependency-graph identity/position with its base variant, rather than a
partial diff applied on top of one. Chosen deliberately over lighter
per-resource feature-gating (e.g. tagging individual `AddBuffer` calls with a
feature name) for maintainability and because it gives a distinct, visualizable
node if/when a graph diagram tool is built.

### 4. Re-execution — generation-counter dirty tracking (generalizing `SetAsRunOnce`)

**Problem:** a boolean "run once" flag is too narrow — compute-heavy passes
need "only re-run when the inputs I actually care about changed" (e.g.
re-running spectrum init after a user commits a wave-parameter edit in
ImGui), and a push-based signal/callback system was considered but rejected
as architecturally "ugly" (implicit control flow, subscription lifetime,
action-at-a-distance).

**Resolution:** pull-based generation counters, checked at traversal time —
no callbacks fired anywhere.

```cpp
struct ResourceState { uint64_t generation = 0; };
// bumping is a side effect of the write path everyone already goes through —
// not a separate "notify" call that call sites can forget:
void WriteBuffer(ResourceHandle h, const void* data, size_t size)
{
    queue.WriteBuffer(GetBuffer(h), 0, data, size);
    resources[h].generation++;
}

struct SubpassState { uint64_t lastRunGeneration = 0; };
bool ShouldRun(const SubpassDesc& subpass, const SubpassState& state)
{
    for (auto& read : subpass.declaredReads)
        if (resources[read].generation > state.lastRunGeneration)
            return true;
    return state.lastRunGeneration == 0; // boot-time run-once falls out for free
}
```

- **Default policy:** a subpass's already-declared reads (`AddBuffer`/
  `AddImage`) *are* its watch list. No new declaration mechanism or name
  needed for the common case — this also resolves the "how do all parties
  know to communicate a write" worry, since the bump lives in the one write
  path everyone already calls through.
- **Residual naming need**, two edge cases only:
  - Opt a specific read **out** of gating (read every frame regardless of
    change) — candidate: `.ExcludeFromDirtyCheck()` chained modifier.
  - Opt **in** an extra trigger that isn't itself a binding — candidates:
    `RerunOn(...)` / `TriggerOn(...)`. Avoid "watch/signal/observer"
    vocabulary; keep naming consistent with existing `ReadOnly`/`WriteOnly`
    style.

### 5. Template subpasses — DRY + a future coalescing hook

**Motivation:** avoid hand-redeclaring near-identical subpasses (repeated
`OverrideSubpass` variants, common ops like mesh draws).

**Direction:** a `Template` is a base subpass prototype; instantiation clones
it and applies a small diff (pipeline swap / extra buffer / changed uniform)
to produce a fully materialized subpass. This is **complementary to**
`OverrideSubpass`, not competing with it — Template is an authoring-time
convenience; `OverrideSubpass` is what the resulting node looks like once it
exists in the graph (still fully explicit, still diagrammable).

Two implementation notes for planning:
- **Tag each instantiated subpass with its originating `TemplateId` at
  creation time.** Cheap now; if omitted, recovering "these N subpasses are
  actually the same shape" later requires re-deriving equivalence from
  pipeline/binding comparisons, which is fragile. This tag is what would let
  a future executor coalesce same-template draws/dispatches — real payoff,
  but not something to build yet.
- **Diff mechanism should stay scoped to genuinely small deltas.** It will hit
  the same problem `OverrideSubpass` solved (a pipeline swap sometimes implies
  a resource-list change too) — keep full manual subpass declaration as the
  honest escape valve for compound changes rather than growing the diff API
  to cover arbitrary combinations.

## Explicitly out of scope for this iteration

- **Resource versioning** (ping-pong blur, refraction sampling a
  pre-overwrite target) — real, known problem in frame-graph design
  generally, deliberately deferred. Judged more relevant to a full-engine
  implementation later than to this demo's current complexity.
- **Bindless** — not available in portable/core WebGPU.
- **Manual barrier/hazard management** — not exposed by WebGPU; handled
  automatically under the hood, so a rendergraph here has no correctness
  role, only an organizational one.

## Minor open item

`BufferObject`/`ImageObject`/`PipelineObject`/`ShaderModule` currently use
bare `uint32_t` handles with no type tag — nothing stops passing one where
another is expected. Low priority while shape is still settling; precedent
for the eventual fix exists in DiamondDogs' `RhiHandle<Tag>`.

## Suggested implementation order

1. Stable subpass storage + builder-proxy pattern (§1) — unblocks a safe
   fluent API; everything else builds on this.
2. Resource declaration model (`BufferUsage`/`ImageUsage`) doing double duty
   as both binding info and the dependency/dirty-watch source of truth (§2, §4).
3. Sentinel node support (`FrameStart`/`FrameEnd`) unifying `AddDependency`
   to a single handle type (§2).
4. `OverrideSubpass` as a first-class node type (§3).
5. Generation-counter dirty tracking wired to the existing CPU-write path
   (§4).
6. Template/prototype cloning, with `TemplateId` tagging from day one (§5).
