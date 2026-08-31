# Multiplayer gameplay platform

The original actor system remains a useful renderer, animation library, collision world, and source of movement
mechanics. It is not the authority for multiplayer game state. New gameplay code must follow the boundaries below.

## Runtime boundaries

1. **Input** samples raw keys and mouse state once, producing numbered player commands. A movement command contains
   held controls, edge-triggered actions, view direction, and the client simulation tick. Equipment selection is a
   separate reliable, server-confirmed intent. Local input edges exist for one controller sample only; a rejecting
   animation state discards them instead of retaining them for a later action window.
2. **Simulation** runs at a fixed tick independent of rendering. The server owns entity transforms, health, action
   transitions, collision results, projectiles, objectives, teams, rewards, death, and respawn. Ocarina actor
   functions may implement mechanics, but they do not decide network authority.
3. **Replication** sends entity creation/destruction reliably, transient commands/events reliably when loss changes
   gameplay, and periodic snapshots unreliably. Snapshots contain component state rather than copied C actor memory.
4. **Presentation** maps authoritative entities to local Ocarina models and animations. The local player predicts
   accepted movement and actions immediately, then reconciles against server acknowledgements. Remote entities are
   interpolated from snapshots and never run local gameplay authority. Native Ocarina gameplay remains a fixed 20 Hz
   simulation; interpolation FPS controls only additional presents between native states. Interpolation sample counts
   are derived from the current native update rate rather than a second hard-coded clock. The renderer measures its
   real presentation cost and discards optional intermediate samples when they no longer fit inside the next fixed
   simulation interval, while always presenting the newest native state. A requested render rate therefore cannot
   make gameplay run slowly on hardware that cannot sustain that presentation rate.
   Native remote-player nameplates are a separate presentation boundary: they consume admitted player identity and
   renderer positions, then perform native camera projection and collision-world occlusion. Transport and simulation
   code do not draw labels or decide whether an obstructed name is visible.
   `NativeCombatPresentationController` converts admitted authoritative combat outcomes into native hit marks, sounds,
   and local reactions without computing damage or changing health. `NativeLocalRespawnController` accepts only the
   local player's server respawn event, atomically rebases client command/projectile state to its life epoch, discards
   sampled native input edges, restores the three-heart native projection, and starts the scene transition. The former
   bridge-owned respawn suppression flag was dead state and has been removed.
   `ClientSessionGenerationTracker` is the transport-to-client-state lifetime boundary. The first valid transport
   generation and every replacement generation require one complete reset of session-owned gameplay, replicas, and
   native bindings; repeated observations and invalid generation zero do not. Network telemetry setup and teardown
   belong to `MultiplayerUI`, so the bridge cannot leave HUD state alive across UI shutdown.
   `NativeVoiceController` independently owns Windows capture/playback devices, voice settings, push-to-talk sampling,
   and the admitted voice inbox. Chat contributes only whether text entry currently owns keyboard input. Disabling
   voice without ever opening an audio device still drains admitted audio, preventing stale packets from surviving
   until a later enable. `MultiplayerUI` no longer owns or directly updates the codec/device implementation.
   `MultiplayerHudRenderer` consumes a transport-neutral immutable view containing formatted chat lines, edit state,
   notices, and projected health. It owns overlay geometry, line colors, cursor-window scrolling, and life-bar drawing;
   it cannot connect, send commands, inspect network peers, or mutate gameplay. `MultiplayerUI` now supplies interaction
   state and no longer contains overlay layout or life rendering code.
   `MultiplayerInteractionPort` is the application-facing boundary for hosting, connecting, chat, peer identity,
   telemetry, and voice lifecycle. `NativeMultiplayerInteractionController` is the sole adapter from those semantic
   requests to `NetworkRuntime` and the native voice controller; UI code cannot include either implementation.
   `MultiplayerCommandProcessor` converts slash commands into port requests without depending on Win32, rendering,
   packets, crypto, or audio devices. A fake-port self-test verifies command aliases, endpoint defaults, administration
   forwarding, private-message identity resolution, history clearing, and text sanitization without launching a game.

## Entity model

Every replicated object has a generation-checked `EntityId` and an explicit component set. Initial components are:

- `Transform`: position, rotation, velocity, scene, and movement mode.
- `Player`: connection owner, team, selected weapon, health, life state, and last processed command.
- `Action`: idle/move/evade/attack/block/aim/fire/fish state with start tick and phase.
- `Collider`: shape, layer, and collision mask owned by server simulation.
- `Projectile`: owner, weapon, velocity, lifetime policy, and impact state.
- `Renderable`: model, equipment, animation, pose time, and presentation flags.
- `Objective`: team ownership, capture/build state, health, and replication priority for future WvW/tower defense.

Entities are created and removed through the registry. Corpses, arrows, fish, players, structures, and objectives do
not use negative player IDs or overloaded actor packets.

## Strategic world topology

The `wvw`, `wvw2`, `wvw3`, and `wvw4` map references supplied for this project define the intended large-map shape,
not a scoring system. The strategic world is a server-authored graph laid over physical scene geometry:

- Three territorial fronts meet through a contested center. Routes, bridges, water, cliffs, walls, and gates create
  real chokepoints; ownership is not inferred from a rectangular grid or client map overlay.
- A borderlands map contains thirteen strategic objectives: three keeps, four towers, and six supply camps. The
  reference arrangement places one keep toward each territorial front, pairs towers with the major approach lanes,
  and distributes camps around the outer logistics routes. This count is world-layout data, not a source of points.
- Supply camps are capturable source objectives. Directed supply routes connect each camp to specific nearby towers
  and keeps, so capturing a camp changes logistics along authored routes rather than awarding points.
- Towers are smaller fortified objectives controlling approach routes. Keeps are larger multi-entrance objectives
  composed of independently authoritative gates, walls, and future build/defense locations.
- Every strategic objective anchors an authored territory/influence region and an adjacency list. These regions are
  used for map presentation, spawn/logistics rules, and replication interest hints; combat and capture still use
  physical server collision and explicit radii/volumes.
- Objective identity, kind (`camp`, `tower`, or `keep`), team owner, authored influence region, supply-route topology,
  fortification state, and dependent structures are durable server world state. Players, projectiles, corpses, and
  transient combat remain session state.
- There is deliberately no score value, points-per-tick accumulator, match score, or 5/10/25 weighting in the model.
  The colored numbers visible in one reference image are ignored. Ownership and logistics exist to alter the playable
  battlefield itself, not to feed a scoreboard.
- The screenshots are design references, not runtime map data. Exact objective transforms, influence polygons,
  adjacency edges, and camp supply destinations must be authored against the final playable scene so visual regions,
  server collision, and actual routes agree.

`StrategicWorldTopology` implements the first stage as a server-owned aggregate beside capture simulation. It gives
objectives durable camp/tower/keep identity and unique influence-region keys. It accepts only unique directed
camp-to-tower or camp-to-keep supply routes and normalized undirected influence-region adjacency edges whose endpoints
exist in the same scene. Removing a strategic site retires all attached routes and region edges.
`ServerWorld::HasFriendlySupply` derives current supply from authoritative endpoint ownership rather than a client
report. Build and repair intents targeting strategic towers or keeps are rejected unless one of those authored routes
currently begins at a camp owned by the same team. The rejected sequence is consumed and never queued for later
execution; capturing a source camp enables subsequent new intents. Non-strategic structures do not acquire an implicit
logistics rule. The complete graph is schema-versioned with objectives and structures, validated before mutation, and
restored transactionally. Administrative topology changes cross `ServerWorldManagement` and trigger one replication
interest refresh; there is still no score representation anywhere in this aggregate.

`EntityRegistry` is the sole allocator for simulation entities. Its IDs contain a slot and generation, so a delayed
packet or presentation handle cannot accidentally address a different entity after a slot is reused. Simulation
systems expose immutable snapshots and explicit events; networking serializes those values but does not own their
lifecycle.

The current server systems divide ownership as follows:

- `ServerWorld` owns all authoritative simulation systems and advances them from one capped 60 Hz accumulator.
  Projectile and fishing systems step every world tick; player movement/combat and objective capture step in fixed
  order every second tick at 30 Hz. `NetworkRuntime` submits commands and replicates the resulting snapshots/events;
  it does not independently advance gameplay clocks or retain references to individual gameplay simulations. Player
  commands, scene/team changes, projectile/fishing mutations, snapshots, and event drains cross explicit world
  operations. Player admission is transactional, and player departure is one
  world operation that removes the player plus owned projectiles, hooked fish, lure, and intent-admission history.
  Objective removal similarly cascades to dependent structures. These aggregate lifecycle operations prevent stale
  ownership state from accumulating across reconnects and long-running matches. Sequenced arrow, lure, fish, scene,
  weapon, and structure commands enter through aggregate `ServerWorld::Execute*` operations; transport code cannot
  consume replay floors separately from the state mutation it guards.
  Mutable subsystem accessors are not part of the production API. Game, transport, and dedicated-server code can
  only submit aggregate commands, read immutable snapshots, and drain explicit events. A test-only friend fixture
  permits controlled low-level injection for primitive and load tests without reopening that production boundary;
  load-test disconnects still use the real aggregate departure operation and verify owned-entity cleanup.
  `PlayerSimulation`, `ProjectileSimulation`, and `FishingSimulation` expose fixed steps only; their former independent
  wall-clock `Update` accumulators have been deleted. Consequently there is one gameplay scheduler, one catch-up cap,
  and one ordering point for movement, combat, projectiles, fishing, and resulting lifecycle events.
- `PlayerSimulation` owns movement, action state, melee/block resolution, health, death, and respawn. Death schedules
  a simulation lifecycle event and respawn exactly 150 player ticks later; transport/render-loop timing cannot shorten
  or prolong the five-second interval. Living players collide through the original Link-sized radius-12, height-60
  cylinder. A deterministic player-ID pair order and uniform-grid broad phase keep separation independent of entity
  registry order without an all-player quadratic scan. The runtime only turns lifecycle events into reliable messages
  and a corpse presentation.
  Session-owner lookup is backed by a generation-checked owner-to-entity index, so command admission, damage, team,
  equipment, and lifecycle operations do not repeatedly scan the player registry. The same authoritative player set is
  synchronized into a scene/XZ spatial index after movement and every spawn, scene change, respawn, removal, and reset.
  Melee and arrow sweeps query only nearby indexed candidates before applying the exact articulated body and shield
  narrow phase; sustained projectile fire therefore no longer allocates and tests a full player snapshot array per
  arrow.
- Scene entry is an explicit reliable request/response lifecycle. `ServerWorld` composes the allowed scene/spawn
  registry and replay-protected `SceneTransitionAuthority`, creates a player only through configured server spawns,
  rejects unregistered scenes, and chooses the resulting transform and entity identity. A scene transition atomically
  retires the player's old-scene arrows, lure, hooked fish, and fishing presentation before replication
  reconciliation. Client pose packets cannot create players, select a scene/spawn coordinate, or leave owned entities
  attached to a different scene.
- `PlayerSimulation` owns team membership and rejects same-team combat for non-neutral teams. Neutral players remain
  free-for-all. The normal `/team red|blue|neutral` command changes server state; snapshots replicate the result.
- `ProjectileSimulation` owns arrow creation, fixed-tick motion, collision, damage, and retirement.
  Clients send only reliable arrow-fire intents. The server derives projectile origin, orientation, velocity,
  collision, impact, and lifetime. Projectile state packets
  are server-to-client snapshots and lifecycle events only. Network code reaches these mutations and event drains
  through explicit `ServerWorld` operations and does not retain a projectile-system reference.
  Active arrows are indexed independently by server replication ID and by owner. Exact lookup, replay correlation,
  disconnect cleanup, replication-ID allocation, and the 99-stuck-arrow retention policy operate on those bounded
  indexes; every destruction path removes all index entries before a registry slot can be reused.
- `FishingSimulation` owns generation-checked fish and lure entities, exclusive hook ownership, and server-validated
  hook and release transitions. Clients send compact deployed/reel controls rather than coordinates. The 60 Hz server
  simulation owns cast velocity, gravity, water entry, terrain collision, reeling, hook state, and removal; native
  fishing remains local prediction/presentation. Dedicated lifecycle states drive remote actors, with unreliable
  motion snapshots and reliable removal and interest re-entry baselines.
  Local hook actions are also explicit transitions: `LocalFishIntentStream::BeginHook` and `EndHook` own the semantic
  hook lifetime. Native fish actors commit those transitions only at bite, line-break, catch-release, equipment-cancel,
  destruction, and reset boundaries. Per-frame actor state observation cannot create network gameplay.
  `NativeRemoteFishPresentationController` resolves an authoritative fish identity, owner lifetime, interpolated pose,
  and fish state into one typed visual snapshot. Ocarina fish retain only an externally-presented flag; they no longer
  store remote player IDs or query player and fish network stores independently.
  `FishCatalog` supplies canonical pond definitions, while archive-derived water boxes bootstrap bounded wild-fish
  definitions. Catalog entries use a stable `(scene, spawnKey)` identity and explicit `FishSpecies`; Ocarina actor IDs,
  parameters, rooms, and home coordinates are not network-entity identity. The native fishing actor derives the same
  spawn key locally only to bind presentation to an authoritative entity.
  `ServerWorld::ExecuteFishAction` requires a live fishing-equipped player and an authoritative lure.
  The server accepts a hook only after its own lure has settled, then chooses the deterministic nearest unowned catalog
  fish within the bite radius whose scene/bounds contain that lure and derives species, weight, placement, and
  ownership; release always targets the player's currently owned fish. Transport code
  maps only the sequenced hook/release action and cannot select fish identity or gameplay attributes.
  After player and projectile combat each fixed world tick, `ServerWorld` enforces the ownership invariant that only
  a living player with the fishing weapon selected may retain a lure or hooked fish. One
  `FishingSimulation::RemoveIneligibleOwners` sweep builds an eligible-owner set and visits each fish/lure once, so
  cleanup scales with players plus entities rather than scanning all fishing entities once per player. Transport and
  life-event presentation no longer mutate this gameplay state.
  Active fish are indexed by stable fish identity and owner, and active lures are indexed by owner. Hook admission,
  release, owner queries, and lure controls therefore perform direct authoritative lookups rather than linear entity
  scans. Replication consumes one bulk lure snapshot, avoiding the former player-by-lure quadratic publication pass.
  Network code likewise reaches lure/fish commands, snapshots, and lifecycle drains through `ServerWorld`; disconnect
  cleanup cannot bypass the aggregate player-departure operation.
- `ServerReplicationCoordinator` is the aggregate owner of observer replication state. It coordinates player,
  owner-following, and spatial visibility; isolated per-observer bandwidth budgets; and coalesced disposable queues.
  Disconnect removes the observer from every visibility model, budget, and pending queue through one operation, while
  returning reliable player-leave transitions for the remaining observers.
- `ServerReplicationInterestPublisher` is the wire-facing lifecycle boundary for player, arrow, fish, lure, corpse,
  objective, and structure visibility transitions. It queries `ServerWorld`, consumes coordinator transitions, and
  emits exact reliable enter/leave packets plus authoritative baselines. A narrow delivery callback supplies connected
  observers and encrypted transport sending; it cannot alter visibility. The listen-server client inbox receives the
  same transitions directly. `NetworkRuntime` triggers refreshes but no longer contains entity-specific interest or
  baseline policy.
- `ServerReplicationEventPublisher` runs after those lifecycles and owns serialization/fan-out for periodic player,
  objective, and structure snapshots plus projectile, fishing, combat, death, and respawn events. It consumes only
  authoritative `ServerWorld` drains and coordinator observer sets. Owner fallback delivery is limited to authenticated
  connected players supplied by the transport callback. `NetworkRuntime` schedules publication and flushes encrypted
  queues; it does not interpret simulation events or duplicate their observer rules.
- `FishingPresentationRelay` keeps untrusted rod, line, and hooked-fish pose telemetry in the replication layer rather
  than authoritative `ServerWorld`. Each cosmetic sample is bound to the exact live player generation, scene, and
  fishing loadout; reconciliation drops it on death, scene transition, weapon change, replacement, or disconnect.
  It can influence only remote rendering and cannot mutate fish, lure, player, or combat simulation. Shared
  presentation bounds reject extreme rod transforms, rotations, line values, and hook offsets at packet ingress and
  again after the server replaces the client lure offset with its authoritative lure position. A client therefore
  cannot use disposable cosmetic telemetry to submit an unbounded matrix or detach hook geometry from the
  server-owned lure.
- `ReplicationCadence` derives the 20 Hz player and 10 Hz objective/structure publication deadlines from completed
  60 Hz authoritative world steps. Render-loop stalls, window movement, and transport polling frequency therefore
  cannot change snapshot cadence; catch-up publishes the newest coalesced state once while preserving tick phase.
- `ServerAuthorityScheduler` is the protocol-independent host loop boundary. It advances `ServerWorld`, consumes
  `ReplicationCadence`, and invokes typed publication callbacks in deterministic player/combat/world-event/life order.
  Combat queues drain exactly once per host update: before projectile publication on 30 Hz player ticks, or afterward
  on odd 60 Hz projectile-only ticks. Reset clears cadence without creating transport or presentation state.
- `PlayerReplicationSystem` owns observer-to-player visibility and the exact entity lifetime visible in each pair.
  It uses the shared `SpatialGridIndex` for coarse candidates, deterministically emits reliable enter/leave transitions,
  emits leave-before-enter when a player ID changes generation, and scopes high-rate player and cosmetic fishing-pose
  replication. There is no parallel player-visibility map outside the coordinator.
- `OwnedEntityReplicationSystem` extends the same observer model to player-owned entities. It owns arrow,
  hooked-fish, and lure visibility, emits deterministic reliable enter/leave transitions, and retains the exact
  server entity generation and immutable typed lifecycle payload needed to establish or retire a presentation when
  the entity crosses interest. It also owns independent per-observer, per-entity state sequences; enter, leave, and
  disposable lure updates therefore share one ordering domain that is removed with the observer and skips zero on
  wraparound. Transport serializes the allocated revision but keeps no fish/lure counters. Entities use their own
  authoritative scene and position in the shared spatial grid, so an arrow crossing into an observer's interest area
  remains visible even when its shooter is outside it. The predicting owner additionally receives only entities marked
  for owner reconciliation. Observers therefore evaluate nearby spatial candidates plus their small owner bucket,
  rather than scanning every arrow, fish, and lure in the world. Fish and lure baselines
  therefore do not depend on whether an observer happened to use a fishing pole earlier in its session. Client
  projectile numbers are only command correlation values and are never authoritative lifetime identity. The runtime
  no longer shadows prior fish/lure snapshots or searches current entity arrays once per visibility transition.
  Every leave explicitly distinguishes an ended/replaced authoritative lifetime from an entity that merely left one
  observer's interest, so transport code never infers destruction by searching current simulation arrays.
- `ReplicationBudgetSystem` isolates disposable bandwidth per observer. High-rate player state, normal projectile
  and fishing state, and low-rate objective/structure/world state use independent refillable pools (65/25/10 by
  default) under a 512 KiB/s per-client ceiling. Exhausting one class cannot consume another class's reservation,
  and one slow observer cannot reduce another observer's budget. Reliable command results, damage, chat, and entity
  lifecycle traffic are critical and bypass disposable budgets.
- `ReplicationQueueSystem` is the disposable-snapshot outbox. Its stable stream key includes the message stream,
  logical owner/key, server entity slot/generation, and optional sub-ID. Repeated state for one lifetime replaces
  the pending older state instead of growing a packet backlog. Each priority drains in deterministic round-robin
  key order with its own packet ceiling, so a low-ID entity or a large projectile population cannot permanently
  starve other entities or the low-priority objective/structure reservation. Reliable lifecycle removal purges
  queued state for that exact lifetime before the presentation is retired.
- `SpatialEntityReplicationSystem` owns relevance for entities whose visibility follows world position rather than
  a player owner. It compares generation-checked corpse, objective, and structure
  lifetimes against authoritative observer scene/position, emits deterministic leave-before-enter transitions, and
  retains the typed snapshot associated with every visible lifetime. It also owns a monotonic state revision for
  every observer and logical spatial entity. Reliable enter/leave baselines and disposable objective/structure
  snapshots consume that same revision stream, so a delayed periodic packet cannot resurrect a retired corpse,
  objective, or structure or overwrite a replacement generation. Client admission rejects stale revisions before
  they enter presentation queues; corpse queues coalesce by logical entity slot rather than retaining parallel old
  and replacement generations. The shared scene/XZ grid limits exact 3D tests
  to nearby cells instead of scanning every world entity for every observer; deterministic candidate metrics make
  that scaling property testable. Corpse retention churn and periodic state no longer broadcast to every connection
  or maintain separate late-join loops; reliable leaves use the replication system's retained payload.
- `InterestRelevance` defines one deterministic hysteresis rule shared by player, owned-entity, and spatial-world
  replication. New visibility uses the configured enter radius; an exact lifetime already visible remains admitted
  through a bounded outer leave margin. Position jitter at an interest boundary therefore cannot repeatedly emit
  reliable leave/enter pairs and full baselines. Replacement generations never inherit the margin from the previous
  lifetime, and entities outside the outer radius still retire immediately.
- `ObjectiveSimulation` owns stable objective entities, capture radii, team ownership, contested state, capture
  progress, and ownership-change events. Only living server-simulated players in the objective's scene contribute.
  Objectives resolve through a generation-checked world-key index. Each capture tick queries the authoritative player
  spatial index around that objective and then applies the exact 3D capture radius, health, scene, and team rules;
  camps, towers, and keeps no longer each scan a copied array of every connected player.
- `StructureSimulation` owns planned/building/active/destroyed structure phases, objective-owner build authorization,
  team ownership, health, hostile damage, friendly repair, and explicit lifecycle events. Structure keys resolve through
  a generation-checked index, while a synchronized scene/XZ index supplies projectile collision candidates. Exact
  vertical-cylinder intersection still chooses the first impact, but every arrow no longer scans every fortification
  or future tower-defense object. Create, remove, reset, and restore update both indexes atomically with registry
  lifetime changes.
- `ServerWorld` consumes objective ownership changes in the capture tick before exposing them to replication. Every
  dependent fortification is reset to a neutral planned state first, so the old owner's wall or gate cannot remain
  active for projectile collision and the new owner must rebuild it through normal supply-authorized commands.
- `CorpseSimulation` owns generation-checked corpse entities. A death event carries the exact source player entity,
  life epoch, scene, pose, and equipment; `ServerWorld` creates the retained corpse in that same authoritative
  transaction, and spatial-interest reconciliation emits reliable enter/remove lifecycles. Respawn never creates or
  mutates the old body. It retains at most 99 corpses per scene and removes the oldest through normal entity
  destruction; corpse identity is never encoded as a negative player ID or sent through a mutable player-state packet.
  On each client, `CorpsePresentationRegistry` indexes that lifetime both by corpse entity and by the exact source
  player entity/life epoch. The retained corpse exclusively owns that incarnation's body presentation: matching local
  and remote live-player meshes are suppressed without retiring their semantic player replicas, while a newer respawn
  epoch is visible immediately and the old corpse remains independently retained.
- `ServerWorldManagement` is the administrative mutation boundary for scene-spawn configuration, transition
  authorization, and objective/structure creation and removal. A successful spatial mutation publishes exactly one
  replication-interest refresh before returning; a rejected removal neither changes authoritative state nor emits a
  refresh. `NetworkRuntime` delegates these operations and no longer has to
  pair direct `ServerWorld` mutations with manually remembered replication work.
- The dedicated server currently keeps all world state in memory and performs no disk loading, saving, or snapshot
  capture. Persistence can be designed later when there is meaningful durable state and a change-driven save policy.

- `ServerIntentAdmission` owns incarnation-aware replay floors for every gameplay intent and cooldown state keyed by
  player and intent kind. Sane intents consume their sequence before world-state validation, so a rejected action
  cannot execute later after equipment, scene, life, or nearby-object state changes. Sequence comparison supports
  unsigned wraparound, disconnect/reset removes all player floors and cooldowns, and cooldowns use fixed server ticks
  rather than process wall time. It is private to `ServerWorld`; `NetworkRuntime` only validates
  packet shape, maps wire enums into semantic commands, and dispatches them. Admission and world-state evaluation are
  one authoritative operation.

`NetworkRuntime` adapts these snapshots and events to the compatibility wire protocol. Objectives and structures
use dedicated entity packets with spatially scoped reliable enter/leave baselines plus keyed, coalesced disposable
snapshots. Structure build/repair packets contain player intent only; the server derives team, objective ownership,
distance, health, accepted amount, rate, and resulting state. It must not keep a second gameplay-state map for an
entity already owned by a simulation system.
Machine identity and moderation-list persistence live in `LocalNetworkIdentity`, outside the wire schema. The packet
header contains no Windows username, volume-serial, executable-path, or filesystem implementation; runtime startup
and moderation explicitly depend on that platform module instead of acquiring file I/O through packet definitions.
`ServerGameplayCommandService` is the authenticated semantic-command boundary for player input, weapon selection,
scene entry, structures, fishing, and projectile fire. Protocol dispatch validates packet shape and binds the sender;
the service invokes `ServerGameplayIngress` once and publishes only accepted outcomes through the replication
publishers. Scene replies are queued before destination interest lifecycles, and packet-supplied player IDs cannot
redirect a command away from the authenticated sender. `NetworkRuntime` no longer owns command execution methods.
`ServerPlayerSessionService` owns the inverse admission/departure transactions. Admission creates the tentative world
entity before committing identity, rolls it back if identity admission fails, then emits assignment, initial lifecycle,
scene authority, chat keys, interest baselines, and the join message in order. Departure removes coordinator visibility,
session identity/crypto, world ownership, private-chat keys, voice state, dependent projectiles, and reliable player
lifecycles as one operation. `NetworkRuntime` supplies only send, broadcast, kick, and chat-key callbacks.
Scene packets follow the same rule: transport maps the request and presents the resulting state, while spawn policy,
replay floors, fallback admission, scene mutation, and dependent-entity cleanup remain inside `ServerWorld`.
The public client-submission boundary accepts only semantic scene, weapon, player, projectile, fishing-presentation,
lure-control, fish-action, and structure-action values. The local command service stamps the current life epoch,
maps semantic enums and booleans, validates the resulting packet, and chooses reliable or disposable transport.
`ClientRuntime` contains no packet structs or wire constants; native Ocarina code observes input and render state
without depending on protocol layout.
`NativeLocalProjectileController` is the sole pointer-bearing boundary for locally presented arrows. It correlates
native actors with semantic presentation IDs, consumes accepted/rejected authority results, and retires the native
actor when the authoritative projectile ends. Transport orchestration never stores or resolves an `Actor*`.

`LocalGameplayCommandService` now implements that submission boundary for movement, weapon selection, scene entry,
fishing presentation/control/actions, projectile fire, and structure actions. It stamps the current authoritative life
epoch, rejects requests from stale lives, performs adapter validation, and selects reliable versus disposable delivery
in one place. A remote client sends the resulting command through encrypted transport; a listen-server player invokes
the same server gameplay-command service directly. `NetworkRuntime` only identifies which endpoint mode is active and
delegates semantic requests, so transport orchestration no longer owns local gameplay command policy.

`NativeLocalPlayerCommandController` is the native input boundary for the local player. It samples movement axes,
heading, aim pitch, selected weapon, primary/block/aim holds, and the one-frame evade edge; coordinates weapon
selection confirmation; and submits exactly one typed command sample to prediction. Its senders accept semantic
weapon and player commands only, so neither native input code nor prediction can depend on runtime or packet types.

`ServerGameplayPacketIngress` is the symmetric server boundary. It rejects invalid authenticated-sender IDs and
malformed gameplay packets, converts accepted wire values through the relevant adapter, and invokes exactly one
semantic `ServerGameplayCommandService` operation. Projectile admission also publishes its accepted/rejected intent
result from this boundary. `NetworkRuntime` protocol callbacks are now transport dispatch only and do not independently
interpret scene, movement, loadout, structure, fishing, or projectile packets.

## Runtime assets

The client mounts immutable `.o2r` archives through the read-only `O2rArchive` implementation. Runtime archive
creation and mutation are not supported. MPQ/StormLib, legacy `.otr`/ZIP mod archives, loose-folder archives, ROM
extraction manifests, and exporter helpers have been removed; they must not be reintroduced as compatibility paths.

Client structure interaction is protocol-independent before transport. `LocalStructureActionStream` owns its reliable
one-shot sequence, validates the semantic build/repair request, and restarts only at a new player life. Runtime stamps
the authoritative client life epoch and performs the sole conversion to `NetworkStructureActionPacket`; gameplay and
tests no longer construct that wire packet. Transport rejection consumes the request instead of retaining a build or
repair action until range, team, or structure state changes. The unused linear-level-advance packet and message name
are deleted. The same audit removes inherited state-request, updater, world-transfer, heartbeat, and application-level
disconnect names. Transport peer deletion already owns authoritative departure cleanup. All retired numeric wire slots
remain intentionally vacant so active message values do not shift, and `ValidAppMessageType` explicitly allowlists
active IDs instead of accepting every byte between the first and last enum value.
The shipping archive is the sole packaged game-data source and is maintained directly with ordinary ZIP tooling.

Audio is similarly fixed to the built-in sound-effect sequence and its two sound-effect fonts. Music sequences and
music-only fonts are not runtime-selectable, and samples are resolved lazily from those fonts rather than eagerly
loaded at startup. Packaged game samples use only the native ADPCM and small-ADPCM codecs; the former XML/custom
sample path and its WAV, MP3, FLAC, Vorbis, and streamed-Opus decoder dependencies have been deleted. Opus remains
only in the independent network voice codec. Sound pruning must preserve numeric sound-effect slots because native gameplay derives related
effects (for example surface footsteps and age variants) by adding offsets. An unused slot may be emptied only after
all direct and computed callers are excluded. A retained optional sample path that is absent from the archive resolves
to silence and is logged once; missing audio data must never terminate the client.

## Tick and command rules

- The server advances one fixed world clock and never derives game speed from render FPS or packet arrival time.
  A delayed host loop catches up at most 15 world steps per update and discards excess backlog, preventing a stall
  from turning into a prolonged fast-forward.
- Each client command has a monotonically increasing sequence. The server validates it, processes it once on the
  server's fixed tick, and includes the last processed sequence in snapshots. Client render-frame counters remain
  local sampling details and are never accepted as authoritative simulation time. The local command stream also
  rejects duplicate or reordered frame samples before assigning a command/action sequence; scene and life changes
  establish explicit new tick namespaces, and unsigned wraparound remains ordered.
- Movement commands are high-rate unreliable data because a newer command supersedes an older one.
- Attack, weapon change, interaction, build, chat, damage result, death, respawn, and entity lifecycle events use
  explicit reliable messages where loss would alter gameplay.
- A rejected one-shot gameplay intent is final for that sequence. Fish, lure, projectile, and structure commands are never
  retained for a later frame when scene, equipment, animation, or collision preconditions might become true. Clients
  may issue a new sequence for a still-current action after observing authoritative state.
- Player movement remains disposable. Equipment changes use their own reliable sequence and must be confirmed by a
  strictly newer authoritative snapshot before the client emits weapon action bits; stale equipment snapshots cannot
  authorize fire, block, or attack.
- Clients predict only their own movement and presentation. Damage, impacts, projectiles, blocking, inventory,
  objectives, and world changes are server decisions.
- Reconciliation rewinds the local predicted state to the acknowledged server state and reapplies unacknowledged
  commands. Presentation smoothing hides small corrections without changing collision state.

## Migration order

1. Replace indefinite compatibility-layer input latches with bounded action commands.
2. Introduce entity IDs, fixed ticks, player commands, authoritative snapshots, and metrics beside the current bridge.
3. Move locomotion authority and client prediction/reconciliation to the new player simulation.
4. Move action transitions and combat geometry, then projectiles, damage, death, and respawn.
5. Move persistent actors and fishing to entity components and server systems.
6. Add spatial interest management, teams, objectives, structures, persistence, and headless soak/load tests.
7. Remove superseded packet fields and bridge code as each migrated path becomes the only path.

## Remaining compatibility boundary

The authority audit leaves a narrow native rendering boundary that must not be expanded with gameplay rules:

- Player lifecycle establishes identity and scene, while `NetworkPlayerSnapshotPacket` supplies every movement,
  equipment, action, aim, health, and animation semantic needed to render Link. There is no general client-authored
  player presentation stream; native model groups and animations are selected locally at the final actor boundary.
- Fishing line, rod, lure, and fish pose data is presentation telemetry. Fish identity/hook ownership and lure motion
  are authoritative; hook/release no longer use generic actor events, and clients cannot submit lure coordinates.
  Wire telemetry is validated, generation-gated, ordered, and converted to protocol-independent
  `FishingPresentationState` during client inbox admission. `NetworkRuntime`, the game bridge, replica interpolation,
  and native rendering expose only that typed state; packet layout cannot leak into actor presentation. Queued pose
  state is retired with its exact player lifetime. Rod/line orientation and hooked-fish animation pose can be reduced
  further as native rendering is replaced.

At every stage the Release x64 MSVC client and dedicated server remain buildable. A legacy path is deleted in the
same change that replaces its final caller; disconnected files or dormant parallel implementations are not kept.
The production native bridge has no network smoke-test mutation mode. Automated movement, collision, damage, death,
and respawn scenarios drive the shipping command and server-simulation APIs; tests never offset native Link
coordinates or force save-context health to manufacture a successful observation.
`NativeRemotePlayerPresentationController` owns the semantic-to-native transition for remote player lifetimes,
snapshots, and fishing entities. It applies generation-safe replica admission before creating, updating, marking ready,
or retiring native actors, and cascades owner retirement through a semantic callback. Runtime polling remains in the
orchestrator, but it can no longer compose remote equipment/fishing state or manipulate player actor lifetimes.
`NativeRemoteProjectilePresentationController` similarly combines projectile presentation policy, exact-lifetime
replica admission, owner retirement, and native arrow tracking. `NativeCorpsePresentationController` combines corpse
registry admission with dead-player equipment/pose composition and native actor replacement. The bridge only routes
typed projectile and corpse states; it cannot independently establish or retire those native lifetimes.
Live-player body collision is resolved once by `PlayerSimulation` using deterministic player-ID ordering and a
scene-partitioned spatial grid. Remote native Link actors are presentation-only and register no Ocarina overlap
cylinder, so client frame order cannot create local-only blocking or compete with prediction reconciliation.
Static movement collision sweeps five points across the leading semicircle at three body heights. Link's authoritative
24-unit-wide cylinder therefore stops at its radius instead of allowing a zero-width center ray to enter walls, while
the existing axis-separated solver still permits wall sliding. `ServerCollisionWorld` indexes immutable triangles into
deterministic 256-unit three-dimensional cells; player probes, arrows, and fishing segments gather, sort, and deduplicate
only overlapping triangle IDs before narrow-phase testing. Oversized geometry and unusually broad queries have bounded
fallback paths. `ServerWorldBootstrap` owns archive loading, canonical/wild-fish registration, and binding those
read-only collision/water queries into `ServerWorld`; transport startup no longer parses world resources or installs
simulation callbacks itself. Rehosting rebuilds bootstrap state after `ServerWorld::Reset`, and its regression requires
the same canonical catalog size after restart. `ServerCollisionTests` is a first-class solution target that loads the shipping `bin/oot.o2r`, requires
exactly the embedded test01 collision scene, compares an indexed cast against the brute-force result through every
triangle centroid, and requires at least one real candidate reduction plus valid water-derived fish geometry. The
former orphan self-test source is no longer dependent on a manual compile command.
`LocalPlayerVitals` generation- and tick-gates local authoritative snapshots before projecting their health into the
native HUD/game-over value. Combat result events select hit presentation only: Ocarina receives zero native damage,
so animation, voice, knockback, and flicker cannot subtract health a second time or veto a server-confirmed hit with
client-only invincibility state.
`SimulationTests` is the fast Release x64 regression target for deterministic systems and must pass before either
executable is handed off. `NetworkRuntimeTests` launches a real encrypted host/client pair and verifies command,
snapshot, projectile, damage, death, respawn, corpse, world-event, and telemetry behavior across the transport. Once
the key exchange is complete, both processes deterministically discard every thirteenth disposable UDP datagram and
force one reliable datagram in each direction through the transport retry path. The complete scenario must still
finish, proving that snapshots tolerate loss and durable gameplay/chat traffic recovers without weakening handshake
behavior or enabling fault injection in production.
`SimulationLoadTests` runs 128 headless players for two simulated minutes, exercises commands, combat entities,
objectives, structures, persistent objects, replication visibility/lifecycle churn, death/respawn, and disconnect
cleanup, reports entity and visible-pair high-water marks, owned-entity churn, accepted/deferred replication traffic,
and elapsed time, and fails on stale visibility, missing budget pressure, unbounded growth, or a severe
simulation-time regression.
`NetworkFaultInjectionTests` deterministically drops, duplicates, delays, and reorders disposable movement while
retransmitting reliable action edges. It requires monotonic authoritative command processing, exactly-once action
execution, old-life rejection after respawn, generation-safe player/projectile/fish retirement, and replay-safe tower
build contributions. Its fixed seed makes every failure reproducible.
`NetworkTransportSoakTests` launches one encrypted host and four encrypted client processes on opposing teams. Each
client drives 600 command samples, sustained disposable packet loss, and one forced reliable retry. The gate requires
all 2,400 commands to reach authoritative acknowledgements, reliable completion messages to return through the host,
and server-generated PvP combat outcomes before any process may exit.

## Player presentation packet migration

Protocol v60 retained v59's separation of disposable movement ordering from reliable action-edge ordering in
`NetworkPlayerCommandPacket`. A newer movement sample can no longer cause a delayed reliable attack/evade edge to be
discarded, and retransmission of the same action sequence cannot execute it twice. The server accumulates distinct
edges only until the next fixed tick, neutralizes stale movement/held state after six player ticks, and preserves both
sequence floors across scene changes within one incarnation. Protocol v61's life epoch subsequently made respawn an
explicit new command namespace while retaining the same player entity generation.

Protocol v61 adds a server-issued player `lifeEpoch` to commands, authoritative snapshots, and reliable respawn
messages. Death rejects command admission immediately; respawn increments the epoch. Delayed movement or reliable
action packets from the previous life therefore cannot execute after respawn even when their packet sequence is newer.
The client cannot choose the epoch used by `NetworkRuntime::SendPlayerCommand`; runtime stamps the latest epoch learned
from server authority.

Protocol v62 makes network voice Opus-only. The removed IMA ADPCM implementation is no longer an alternate encoder,
decoder, or accepted wire value; a failed Opus encode drops that disposable frame instead of silently changing codec.
The same migration introduces `ServerCommandParser`, which owns command aliases, arguments, team values, usage errors,
and player-versus-administrator access requirements. Runtime command handling executes typed results and no longer
derives authorization policy from ad hoc string branches.

Fortification input is coordinated by `ServerWorld`. `ServerIntentAdmission` owns its replay floor and 15-world-tick
interaction cooldown alongside the other intent lifecycles; stateless `StructureActionAuthority` applies
living/team/scene checks, a 350-unit interaction radius, objective ownership, and fixed build/repair contributions
before mutating `StructureSimulation`. `NetworkRuntime` only maps the wire action and replicates accepted state.

Active fortifications also participate in authoritative projectile collision. Arrows use the same swept-cylinder
primitive as player bodies, resolve against static geometry, shields, players, and structures by nearest impact, and
apply team-checked structure damage without a client hit report. Structure changes are replicated reliably, and arrows attached to a fortification carry
its logical key so they are retired rather than left floating when the structure is destroyed or reset.

Combat resolution is also unified in v60. Melee, arrows, and environmental damage emit a reliable
`NetworkCombatResultPacket` with source/target entity generations, attack kind, damaged/blocked outcome, impact point,
and heading. Clients reject results for retired generations. Nearby observers receive the same authoritative result,
while projectile packets describe projectile lifecycle only. Melee and arrows intersect one shared oriented
mirror-shield rectangle, so presentation type cannot select a different blocking rule or manufacture a block merely
because the attacker is somewhere in front of the target.

Arrow creation is gated by the server-owned bow lifecycle rather than equipment and packet cooldown alone. A player
must remain in authoritative `Aiming` state for the fixed minimum draw duration before `ArrowFireIntent` can create a
projectile. Creation consumes that draw clock while preserving the aiming pose, so replayed, premature, or immediate
re-fire packets cannot manufacture arrows that the server never observed being drawn.

Combat routing no longer lives as an ad hoc transport loop. `CombatReplicationSystem` validates exact source/target
simulation lifetimes and deterministically selects the connected source, target, and observers indexed against the
target by `PlayerReplicationSystem`. It does not rescan every session or recompute a second distance rule that can
disagree with player visibility. `CombatNetworkAdapter` is the only simulation-to-wire mapping and owns packet
sanity/lifetime checks. `NetworkRuntime` drains coordinator-owned batches and sends them; it does not decide combat
relevance or reinterpret outcomes.
After each authoritative player step, the server reconciles player interest before draining combat results. Reliable
lifecycle enters/leaves therefore precede combat delivery from the same simulation tick, and a witness crossing an
interest boundary cannot be selected from the previous publication interval merely because player snapshots publish
at a lower cadence.

All 32-bit sequence, server-tick, life-epoch, and entity-generation ordering uses `SequenceNumber.h`. Its portable
serial arithmetic accepts a candidate only when the unsigned forward distance is nonzero and less than half the
counter space; the exactly-half-range case is unordered in both directions. Prediction, authoritative admission,
replication inboxes, interpolation, respawn deadlines, and presentation lifetime replacement no longer carry local
signed-cast variants that could drift or depend on compiler conversion behavior.

Protocol v63 gives every authoritative combat result a nonzero monotonic event ID. The client accepts an event only
once, in wrap-safe reliable order, and only while its exact source and target entity generations are active.
`ClientReplicationInbox` immediately converts the admitted packet into the simulation-domain `CombatResultEvent`;
runtime, presentation policy, the game bridge, and transport-soak consumers never receive packet fields or wire enum
constants. `ClientCombatPresentationPolicy` then maps that admitted semantic outcome to one explicit native action: a metal impact and
clank for every observed block, Link's stock damage response for the local victim, or a lightweight impact mark for an
observed remote hit. Scene changes and local invincibility suppress stale local damage without conflating it with a
successful shield block. Same-team non-neutral players are excluded from swept arrow collision, so an ally cannot
silently absorb a projectile whose damage the team policy would reject.

Protocol v64 removes native Ocarina `modelGroup` and `itemAction` values from client-submitted player pose packets.
Equipment is now derived at the final legacy draw boundary from the authoritative snapshot's semantic
`selectedWeapon`; blocking, bow-ready, and death presentation are derived from authoritative `actionState` and health.
Corpses likewise retain semantic weapon selection rather than a client-selected native display-list group. The only
remaining model/action values live inside `NativeRemotePlayerRenderer` as an Ocarina renderer implementation detail.
Holding primary fire with the bow now enters the server's aiming state even without the separate RMB aim modifier, so
remote bow-ready presentation follows the same deterministic command state as projectile admission rather than
untrusted pose telemetry.

Scene admission follows the same boundary. `SceneTransitionAuthority` owns configured server spawns, default fallback,
per-player request sequence floors, and accepted/rejected decisions. `SceneNetworkAdapter` owns intent/state sanity and
snapshot-to-wire mapping. The runtime applies accepted decisions to `ServerWorld`, clears scene-bound fishing state, and
routes the resulting presentation; it no longer owns scene-policy maps or replay ordering.

`LocalSceneAdmission` owns the corresponding client request stream. It correlates each reliable reply to the exact
nonzero request sequence, binds authorization to the server-issued player entity generation, accepts the sequence-zero
bootstrap once, and rejects stale, mismatched, or duplicate replies. A rejected request records the server's actual
scene without authorizing the rejected local scene or issuing a request every frame. Transport failure receives a new
sequence on retry, and a newer desired scene replaces the older pending transition. The bridge no longer keeps raw
`requestedSceneId`, `authorizedSceneId`, or scene sequence counters, and local command/presentation submission checks
this state machine rather than packet arrival order. Accepted authority retains a one-shot spawn placement until its
destination `PlayState` exists, while immediately retiring old-scene prediction. Delayed disposable snapshots whose
scene no longer matches current authorization are discarded before they can restore old prediction, vitals, or
equipment state. The same commit clears native projectile bindings, unresolved projectile correlations, fishing cadence,
and sample-local action edges from the departing scene while preserving every current-life request sequence; stale
actions therefore cannot execute in the destination and replay floors never rewind. `ClientReplicationInbox` validates player ownership and life epoch,
then converts the wire reply to `LocalSceneAuthority`; neither `NetworkRuntime` nor the gameplay bridge exposes or
reconstructs `NetworkSceneEntryStatePacket`.

Reliable player lifecycle packets are validated by `PlayerLifecycleNetworkAdapter` and applied to
`EntityLifetimeRegistry`. The client inbox immediately converts each admitted lifecycle into a protocol-independent
`RemotePlayerPresentationState`; runtime, the dedicated-server drain, tests, and the native gameplay bridge do not
consume the wire packet. Semantic packets such as combat results must match that exact active generation. A stale
retirement cannot erase its successor, and malformed lifecycle packets never reach the presentation bridge.

`PlayerSimulationNetworkAdapter` is the command/snapshot boundary between binary-angle, compact-input wire packets and
the fixed-step simulation's normalized commands. Reliable lifecycle establishes the exact player generation and
disposable authoritative snapshots update it. `ServerWorld` captures the final dead position, heading, and semantic
weapon directly from its authoritative player snapshot until tick-scheduled respawn creates the corpse.
`CorpseNetworkAdapter` only serializes that authoritative corpse snapshot; client pose data cannot create a corpse.

Fish and lure ownership use the same exact-generation registry instead of independent raw maps.
`ProjectileLifetimeRegistry` adds a typed logical identity (owner, command-correlated projectile id, and kind) for
projectiles. Snapshot acceptance requires an exact active server generation, and delayed retirement of an older
generation cannot remove or mutate its successor.

`FishingNetworkAdapter` owns fish/lure snapshot mapping, presentation and intent sanity, and exact-generation lifetime
admission. `OwnedEntityReplicationSystem` allocates each observer's fish/lure state sequence, while the client inbox
commits its sequence floor only after lifetime admission succeeds, so a stale retirement cannot suppress a valid
successor. Player lifetime retirement atomically purges pending fish/lure presentation and rejects new active
owner-entity packets while the player is absent. Exact fish/lure lifetimes and ordering floors remain long enough to
admit their reliable terminal leaves even when player visibility leaves first; only that exact terminal generation can
retire them. Raw fish/lure queue insertion is private to the inbox, so transport, tests, and presentation cannot bypass
sanity, ordering, owner-lifetime, or entity-lifetime admission. Canonical fish ownership, lure motion, hooking, and
release remain server simulation rules rather than presentation telemetry.

`ProjectileNetworkAdapter` is the sole projectile simulation-to-wire boundary. It maps arrow snapshots, validates
state and lifecycle packets, applies exact-generation establishment/replacement/retirement, and manufactures retirement
presentation state. Accepted/rejected fire acknowledgements are life-epoch validated in the client inbox and converted
to `LocalProjectileIntentDecision` before runtime or native gameplay sees them. `NetworkRuntime` retains transport
ordering and per-stream sequence fences only; it no longer duplicates projectile phase mapping or lifetime admission
policy.

`WorldPvpNetworkAdapter` is the wire boundary for objectives, fortifications, and build/repair intents. It validates
phase invariants such as planned structures having no team or health and active structures having a completed build and
positive health. Capture/build/damage policy remains in the authoritative simulations and action authority.

`ServerSessionManager` owns the ordered transport-peer roster, per-peer encryption session, admitted identity, and
pending moderation departure reason. A peer is not exposed to replication until key exchange, identity validation,
ban checks, and authoritative player creation succeed. Disconnect transfers the identity/reason out once and removes
all peer-owned state; `ServerPlayerSessionService` performs explicit simulation, projectile, fishing, scene, and
replication cleanup as downstream reactions rather than maintaining parallel peer/identity/crypto maps.

`SecureTransportChannel` owns application-envelope validation, plaintext handshake policy, per-client and per-peer
encryption, replication submission and budget priority, raw transport delivery, and exact successful-send byte
accounting. `NetworkConnectionTransport` is the sole owner of the underlying `NetTranspClient`/`NetTranspServer`,
asynchronous connect cancellation, host startup, disconnect ordering, raw send/kick operations, message/peer pumping,
and connection telemetry. It exposes byte delivery and typed lifecycle callbacks to `NetworkRuntime`; the runtime no
longer owns or dereferences transport objects. The unused parallel `ReliableUdpService` transport stack was deleted.
`NetworkProtocolIngress` records received wire bytes; `NetworkRuntime` no longer implements parallel crypto/send
helpers or decides which application messages may bypass encryption.

`NetworkSessionLifecycleService` owns state and work spanning one live host/client session: transport pumping,
client-handshake initiation, fixed-step host authority advancement, observer replication flushing, disconnect and
termination cleanup, security readiness, successful-byte rate sampling, and the monotonic nonzero session generation.
Its disconnect path resets crypto/admission, peer sessions, every client replication queue, server world and replication
state, authority cadence, and local voice sequencing as one operation. `NetworkRuntime` delegates this lifecycle and
retains only application-facing composition and forwarding methods.

Every lifecycle disconnect advances a nonzero session generation. The native bridge observes that generation
after UI host/join actions and uses one idempotent live-session reset boundary: native remote-player, corpse, and
projectile actors are retired first; then all replica stores, world state, local intent streams, scene admission,
prediction, and authoritative local-vitals projection are cleared and rebound. An immediate disconnect/reconnect in
one frame therefore cannot carry actors, sequence floors, pending input, health, or scene authority into the new
session. Final application shutdown remains a separate detach path because scene teardown already owns destroyed
native actors at that point.

`ClientGameplaySession` is the aggregate owner for local platform gameplay state used by the native bridge: world
mirrors, command and weapon-selection history, fish and fishing update streams, projectile intent correlation, local
vitals, scene admission, and prediction. It defines three distinct lifecycle operations. Session reset clears every
member and sequence; scene transition retires scene-owned fishing cadence and projectile correlations while preserving
their current-life monotonic sequences; respawn clears all action producers and prediction while advancing scene and
prediction life epochs together. `NativeClientSessionLifecycle` coordinates native actor/render bindings and no longer repeats
partial lists of platform resets at disconnect, transition, and respawn sites.

`ClientReplicationInbox` is the client-side counterpart to the server replication coordinator. It owns exact player,
fish, lure, and projectile lifetime registries; per-stream sequence fences; and all gameplay/presentation queues
consumed by the native bridge. Current-state streams coalesce by their complete logical identity: player lifecycle and
snapshot by player, fish and lure by owner, projectile by owner/correlation/kind, and corpse by exact entity. Reliable
ordered events such as combat results, scene admission, and respawn are retained in order instead of silently dropping
old events at an arbitrary local queue cap. Player snapshots, poses, fishing telemetry, combat results, and projectile
state must match a reliable active generation before queueing. Lifecycle replacement/removal purges queued state and
ordering floors for the old player generation. `ProtocolDispatcher` is the direction-aware wire boundary: it validates one
message header, decodes exactly the payload legal for that direction and admission phase, and emits a typed client or
server callback. Truncated payloads are malformed, packets legal only in the opposite direction are unsupported, and
an unadmitted server peer may send only key exchange followed by a compatible identity. `NetworkProtocolIngress` owns
the complete receive path above the raw connection: inbound byte accounting, secure-envelope authentication/decryption,
directional dispatch, admission-state selection, and rejection of malformed pre-identity peers. It targets the abstract
client/server protocol sinks, so decoding is independently testable from concrete gameplay endpoints. `NetworkRuntime`
only composes the ingress with transport and semantic endpoints; it has no raw receive/dispatch helpers, packet
trial-parsing, or replicated gameplay inbox maps and deques.

`ClientSessionIngress` owns the complete decoded-server-message admission boundary above those stores. It binds an
immutable positive local player assignment to the current connection, advances the local life epoch only when matching
authoritative state is accepted, filters local voice echo, and coordinates player lifecycle with voice admission.
Private-message sender labels are sanitized at this boundary. Disconnect resets local identity/incarnation, every
replication queue and sequence floor, active voice state, and remote private-chat keys in one operation while retaining
only the user's local sealed-box key and non-session chat history. `NetworkRuntime` client callbacks are now transport
adapters that delegate to this owner rather than mutating those states independently.

The outbound half of admission is owned by `LocalClientAdmissionService`. It creates the client key-exchange hello,
submits the persistent signed identity only after the session key is ready, registers the local sealed-box chat key,
and owns whether identity submission succeeded for the current connection. `NetworkRuntime` supplies only active-client
and plain/secure delivery callbacks; it no longer constructs admission packets or stores duplicate identity-sent state.
Resetting the connection clears this admission state together with the crypto session. The v80 packet layouts and
cryptographic identity binding are unchanged.

`ClientProjectilePresentationPolicy` is the final lifetime boundary between admitted projectile packets and native
render actors. A locally predicted flying arrow never receives a duplicate replicated actor; if one exists during a
prediction/authority transition it is explicitly retired. Authoritative stuck or blocked presentation may be
upserted, and every exact inactive lifetime immediately kills and erases its actor record, including projectiles owned
by the local player. This keeps client render cost within the server's 99-stuck-arrow cap and prevents repeated combat
from retaining invisible actors after their logical lifetime ends.

Communication state has the same explicit ownership. `CommunicationInbox` owns bounded sanitized chat and validated
voice receive queues. `PrivateChatService` owns the local sealed-box keypair plus the exact peer-name/public-key roster;
disconnect drops peer keys while retaining and securely erasing only its own key at service destruction. The
`ModerationRegistry` owns case-insensitive persistent ban and administrator identities.
`ServerAdministrationService` owns command parsing, permission checks, player-reference resolution, moderation
mutation, and user/list response composition. It receives snapshots and narrow callbacks for team mutation, message
delivery, and disconnects, so it has no transport or `ServerWorld` dependency. `ServerCommunicationService` binds that
policy to admitted sessions and authoritative world state. It owns public chat and command routing, private-key fanout,
sealed private-message forwarding, voice sequence admission, and voice delivery through authoritative player interest.
Its delivery interface exposes only direct/broadcast sends, encrypted voice payload delivery, a read-only administration
roster, and moderated disconnects. `NetworkRuntime` adapts those callbacks to the active host but no longer implements
server communication routing, voice visibility, private-key distribution, or administrator command execution.

Local text submission is likewise isolated in `LocalTextCommunicationService`. It owns chat sanitization, private
sealed-box encryption, public/private packet construction, mutually exclusive client and host routes, and the local
private-message echo after successful client delivery. Client echo labels come from the same peer-key roster used for
encryption, with runtime session names only as a fallback. `NetworkRuntime` now supplies role and delivery callbacks;
it does not encode chat packets or duplicate host/client policy. This is an ownership change only and preserves the
existing wire layouts.

Local input separates continuously sampled controls from one-shot action intent. Movement, aim, guard, reel, and held
weapon use are rebuilt from current Win32 key state every controller sample. Weapon selection, sheathe, evade, and bow
presses use `ActionIntentFrame`, which is cleared at the start of the next controller sample. Native states that cannot
accept an action in its originating sample therefore discard it, so a click made while climbing, releasing a fish,
typing chat, or transitioning actions cannot execute later. Once transmitted, action edges retain their independent
reliable sequence through movement loss/reordering and are consumed by exactly the next authoritative simulation tick;
the server never waits for a later animation state to accept them.

`LocalPlayerCommandStream` is the single client-side owner of movement and action sequence generation. It accepts one
platform-neutral `LocalPlayerInputSample` per native frame/scene, clamps normalized axes, rejects malformed actions,
suppresses duplicate sampling, and never emits sequence zero across wraparound. Its `PlayerCommand` goes directly to
local prediction and through `PlayerSimulationNetworkAdapter` for serialization, eliminating the former second command
reconstruction in `NetworkGameBridge`. Prediction and the server therefore consume the same sampled values and command
identity rather than independently normalized copies.
Command submission also owns the transport/prediction commit boundary: only a successfully submitted command is added
to reconciliation history, using that command's current sample duration. A rejected transport sample is final and its
elapsed time is never accumulated into a later sequence, preventing delayed movement jumps and action-window drift.

`LocalProjectileIntentStream` is the corresponding client command-correlation boundary for native arrow presentation.
The local player binds a predicted presentation when it nocks an arrow, and the semantic bow-fire action explicitly
commits the shot at the player action boundary. Merely creating, updating, or releasing a native arrow actor cannot
create network gameplay. The bridge no longer walks Ocarina's item-action actor list each frame or infers authority
from actor parent state. The stream owns presentation-lifetime tracking, nonzero intent sequences, transport retry,
and fire-once behavior. A
`NativePresentationBindingRegistry` is the sole native boundary allowed to associate those IDs with local `Actor*`
storage. Native presentation binding and destruction expose only an opaque handle and semantic scene ID; `PlayState*`,
actor fields, transforms, parameters, and ownership are not accepted by the networking boundary. Rejection and terminal
authority retire stream and binding state together; presentation destruction performs
the same exact retirement before storage reuse. Reconnect clears all bindings,
and IDs are not recycled, so delayed authority results cannot resolve to a successor Actor at the same address.
Disappearing presentations are retired immediately, and a changed scene starts a new command lifetime. Native pointers
and transforms never enter platform command state or serialization. The server derives arrow origin/direction and owns
all motion, collision, damage, and retirement.
The former `NetworkGameBridge` `Actor*` record map, phase flags, IDs, and sequence counters have been deleted.

Projectile action policy enters through `ServerWorld::ExecuteArrowFire`; transport validates and maps commands but
does not construct projectile physics. Creation reconciliation runs at the projectile-event boundary, so every newly
visible projectile receives a reliable exact-lifetime enter and authoritative baseline before disposable transforms
or a same-tick impact/removal can overtake its creation.

Arrow-fire and lure-control commands do not carry scene IDs. Their life epoch resolves an exact authoritative player,
and `ServerWorld` derives the scene from that player at the same operation that creates the projectile or lure. A
client therefore cannot nominate a world context or retain an old-scene value in either entity-creation path.
Lure deployment may be admitted between fixed ticks, so its cast orientation is taken from the latest sane
server-admitted command for that exact life/scene while position and all movement remain fixed-step snapshot state.
This preserves command-to-cast causality without applying client movement early. If a stripped dedicated package has
the canonical pond catalog but no archive water boxes, the known pond surface prevents its lure from falling out of
world; archive-derived water remains authoritative everywhere else.

Protocol v70 removed fish position from hook/release intents. Protocol v90 completes that authority migration by also
removing scene, room, actor, parameters, and home coordinates. A client now requests only `Hook` or `Release`.
`FishingSimulation` requires that authoritative lure to be settled, then deterministically chooses the nearest eligible
server catalog entry inside a bounded bite radius, validates scene/water bounds, places the fish, and enters the hooked
transition. Release resolves the exact server-owned fish. The semantic request is nine bytes and cannot select a loach,
weight, spawn identity, or another player's fish.

Protocol v98 removes the remaining Ocarina actor implementation fields from authoritative fish snapshots. Fish state
now carries only owner/entity lifetime, sequence, scene, stable spawn key, authoritative position, species, length, and
active state. Invalid zero spawn keys and unknown species are rejected before client lifetime or presentation state can
change. This keeps replication identity stable if the retained native fish actor or its parameters are later replaced.

Protocol v99 makes grounded versus surface-swimming locomotion explicit authoritative player state. The dedicated
collision world supplies water surfaces to `PlayerSimulation`; deep water holds the server player at its surface while
shallow water, bridges, and dry ground continue to resolve against floor geometry. The mode is replicated in player
snapshots, validated at both protocol and client-replica boundaries, reconciled with local prediction, and selects native
remote swimming animations without giving the client authority over the transition.

Protocol v100 extends that server-owned locomotion state with airborne movement. Leaving authoritative floor
collision now starts fixed-tick gravity, terminal velocity, and swept vertical landing on the server; no client
position or native actor flag can claim a landing. Airborne state and vertical velocity use the existing snapshot
stream, select only the retained native Link falling animation at the presentation boundary, and reconcile the local
native actor toward server height in the same way as water transitions. Collision-scene availability is explicit:
an archive-backed scene may infer a ledge from a missing floor hit, while a synthetic or unavailable scene may not
mistake missing geometry for empty space.

Locomotion-sensitive actions are constrained by that same authoritative state. Entering water cancels combat state;
leaving a ledge cancels an existing grounded swing or evade while permitting fresh airborne weapon/item input.
Ineligible presses are consumed on their original fixed tick and never execute after landing. Scene changes likewise clear
the prior scene's action, held-input presentation, aim pitch, evade velocity, and hit set rather than carrying a
partially completed attack into a replacement world membership. Prediction remembers the latest authoritative
locomotion mode and does not predict airborne evade movement, preventing a locally predicted backflip from fighting
the server while weapon/item state remains authoritative and usable in the air.

`CanPerformGroundedAction` is the common simulation-level eligibility rule for stable-footing gameplay such as bow
creation and fortification build/repair. Portable fishing uses the related `CanPerformFishingAction`: any living
player may keep using the item on ground, while swimming, or airborne. Explicit lure undeploy and fish-release
commands remain cleanup operations rather than privileged gameplay actions.

Protocol v101 adds the intentional airborne sword exception as an explicit authoritative `JumpSlashing` action.
A fresh sword-primary edge received while already airborne starts the jump slash, uses the retained native Link
jump-slash animation on every client, and participates in server melee evaluation until landing. Airborne shield guard,
bow aim/fire, and fishing remain legal weapon/item actions; evade remains grounded locomotion, construction still
requires stable footing, and swimming cancels combat states.

Projectile variant selection is not client-authored. Predicted-arrow bindings retain only a local correlation ID and
scene; explicit fire commands carry no actor parameter or projectile type. `ServerWorld` selects the normal arrow as fixed
authoritative loadout policy and replicates that choice for presentation.

Protocol v73 removes lure type from deploy/reel commands. Portable fishing uses the sinking lure, so `ServerWorld`
now applies type `2` as authoritative equipment policy and includes it only in server-owned lure snapshots. The local
fishing sampler and its C visual-state bridge no longer extract, retain, compare, or serialize native lure selection.
Packets carrying the retired client-selected lure byte fail exact decoding.

Protocol v74 introduced `PlayerLoadoutPolicy` as the server-owned equipment boundary. The completed command split no
longer places a weapon slot in `PlayerCommand` at all: equipment changes use the independently sequenced reliable
`WeaponSelectionCommand`, while movement/action commands contain input intent only. `PlayerLoadoutPolicy` evaluates
action bits against the weapon already stored on the authoritative player entity. Local prediction receives confirmed
weapon context as non-serialized metadata, so it can replay sword timing without creating a second equipment authority.

Protocol v75 binds every reliable pressed-action edge to its movement command window. If a newer disposable movement
sample has already won, a delayed edge from an older command is sequence-consumed but cannot execute against the
player's later weapon, pose, or location. When several fresh action packets arrive before one fixed tick, the newest
complete edge replaces the prior edge instead of OR-merging unrelated buttons. Edges are still cleared after exactly
one simulation tick, including while an existing attack is busy, so no input can fire when recovery later completes.

Protocol v76 moves PC evade/backflip out of the local-only actor path. Space creates two independently consumed views
of one input sample: the native player may respond immediately, while the command bridge emits one reliable `EVADE`
edge. The bridge copy expires at the next controller sample and is cleared whenever the native state rejects action
intents, so climbing, death, input capture, or another busy state cannot queue a later server evade. The server owns a
12-player-tick evade state and derives backward or lateral velocity from authoritative heading plus normalized command
input. Commands received while evading are still acknowledged, but their one-shot edges clear on that tick. Remote
presentation selects the stock backflip or side-hop animation from the authoritative evade velocity; clients do not
submit an animation or displacement.

Client reconciliation in v76 also replays action state, not just ordinary locomotion. Primary phase durations, evade
duration, fixed player tick length, and evade velocity derivation are shared simulation semantics rather than a second
set of client constants. Replay starts from the snapshot's authoritative action phase and remaining time, then applies
newer commands in order. An unacknowledged evade therefore predicts the same backward/lateral displacement as the
server, an already-running evade remains in control until its fixed deadline, and an edge received during primary or
evade busy time is consumed instead of being applied when replay reaches idle. Ocarina still supplies immediate local
animation and world collision; prediction no longer manufactures a forward-walk correction against that backflip.

Protocol v77 removes equipment selection from the disposable player movement packet. Weapon changes are independent,
reliable `NetworkWeaponSelectionIntentPacket` requests carrying their own replay-protected sequence and current life
epoch. `ServerWorld` consumes the sequence before validating life, loadout, or requested slot, so a denied request
cannot execute later after state changes. Only `PlayerSimulation::SelectWeapon` mutates the authoritative slot;
movement commands are normalized against that slot and incompatible primary, block, or aim bits are stripped. An
accepted change cancels incompatible combat state, and leaving the fishing pole immediately retires the player's lure,
hooked fish, and fishing presentation.

Protocol v78 removes player identity from the high-rate movement command. Clients send only input, action, scene,
life-epoch, and sequence data; `NetworkRuntime` binds each decoded command to the authenticated transport sender before
converting it to a simulation command. Host-local commands use the same adapter with explicit owner `0`. The movement
wire format can no longer express a spoofed owner and is four bytes smaller per command.

Protocol v79 applies that ownership rule to every remaining client-only gameplay intent: scene admission, weapon
selection, fish actions, lure control, arrow creation, and structure actions. Their wire
formats contain no player identifier; admission receives the authenticated session player separately and the payloads
remain immutable through validation. Bidirectional replication packets such as voice and cosmetic fishing presentation
retain an owner field only because the server stamps it before forwarding and receiving clients need that identity.

Protocol v80 removes that last shared-direction ambiguity. Client fishing telemetry uses
`NetworkFishingPresentationIntentPacket` and client voice uses `NetworkVoiceIntentPacket`; neither type contains a
player or entity identity. The server dispatcher accepts only those compact inbound layouts, binds the authenticated
sender, and constructs the replicated `NetworkFishingPresentationPacket` or `NetworkVoicePacket` sent to observers.
An outbound replicated layout sent toward the server is rejected as malformed, so identity assignment is enforced by
the codec boundary rather than by overwrite-after-decode convention.

Native voice capture no longer constructs that intent packet. It submits only an encoded Opus frame to
`LocalVoiceSubmissionService`; its `LocalVoiceFrameStream` owns the nonzero transport sequence and the service supplies
the fixed Opus codec, sample-rate, and frame-size fields. The service also owns the mutually exclusive client-to-server
and host-to-observer routes, while `NetworkRuntime` contributes only role and encrypted-delivery callbacks. Disconnect
resets the service with the rest of the session, and capture/UI code cannot forge protocol metadata. This is an API
ownership change only and does not alter the v80 wire layout.

Voice receive admission uses that sequence as a wrap-safe replay fence. The server keeps one inbound floor per
authenticated speaker before relaying a frame, while each client keeps an independent floor per replicated speaker
before playback queueing. Duplicate and reordered unreliable frames are discarded rather than replayed late. Player
departure removes queued audio and both floors so a newly admitted session that reuses a transport player ID starts
cleanly at sequence one; merely muting playback clears audio without weakening those session fences.

Voice is player-scoped replication, not a session-wide broadcast. The server relays a frame only to observers whose
player-interest graph currently contains its authenticated speaker, and a listen host queues it only when that same
relationship exists. Clients activate voice admission from an accepted player lifecycle enter and retire it on leave;
a delayed frame cannot recreate audio for an absent player. This keeps voice bandwidth proportional to nearby visible
players and gives audio the same scene, distance, and entity-lifetime boundary as player presentation.
`PlayerReplicationSystem` maintains the reverse subject-to-observer adjacency alongside its observer-to-subject view,
so each high-rate voice frame iterates only the speaker's visible observers rather than scanning every admitted player.
Scene reconciliation, lifetime replacement, and disconnect rebuild both views atomically.
The same reverse index drives high-rate authoritative player snapshots and player-scoped fishing presentation. Snapshot
owners still receive their reconciliation baseline directly, while remote fan-out touches only visible observers; a
listen host consumes the same indexed visibility result locally. These paths therefore scale with actual interest
pairs rather than multiplying every replicated subject by every connected session.

Owned-entity interest maintains the same reverse adjacency for arrows, fish, and lures. Projectile and lure state
fan-out iterates only observers that currently hold the entity's exact key/lifetime. Owner delivery remains an explicit
per-message decision: predicted routine arrow transforms are no longer sent back merely because lifecycle interest
includes the owner, while reliable terminal outcomes can still include that owner. Observer removal, scene/radius
leaves, and lifetime replacement rebuild the reverse view before later state can be emitted.

Spatial world entities use a third reverse adjacency keyed by objective, structure, or corpse identity. Periodic
objective/structure snapshots and immediate reliable structure-action results now iterate only observers that hold the
exact spatial lifetime, with observer zero routed directly into the listen-host inbox. Scene/radius leaves, entity
generation replacement, and observer departure remove reverse edges before any later world-state fan-out.

Protocol v81 binds every player-affecting gameplay intent to the latest server-issued life epoch. Arrow actions,
lure control, fish hook/release, structure build/repair, and optional fishing cosmetic
telemetry all carry that epoch; the client runtime stamps it from the newest authoritative lifecycle/snapshot rather
than accepting it from native gameplay callers. Server simulation compares the epoch with the current player before
replay-sequence admission. Delayed traffic from a dead or previous incarnation is therefore rejected without advancing
the new incarnation's sequence floor. Tests submit a stale request and then a current request with the same sequence
for each state-changing subsystem to verify the current request remains admissible.

Protocol v82 separates projectile transport submission from authority acceptance. Every arrow-fire intent receives a
reliable `NetworkProjectileIntentResultPacket` addressed only to its authenticated
owner. The result repeats the life epoch, command sequence, client correlation ID, and semantic intent kind; stale-life
or mismatched results are rejected before reaching native presentation. Reliable projectile lifecycle remains the
independent source of server entity slot/generation and is sent before an acceptance result. `LocalProjectileIntentStream`
retains submitted commands until the matching server decision arrives. Rejection terminates the corresponding native
predicted actor instead of silently treating successful UDP delivery as successful gameplay authority.

Protocol v83 keeps routine owner projectile motion locally predicted while making terminal outcomes authoritative.
Flying arrows are not reflected back into native owner motion, avoiding network jitter. The server replicates
exact-lifetime arrow stuck/hit/blocked/removal states to the owner.
The client admits those states only when their reliable entity slot/generation is still active, retires the predicted
native actor, and replaces persistent terminal presentation with the entity-keyed replicated actor.
Projectile lifecycle enter/leave includes the owner so terminal packets can be authenticated, but owner routine
transform baselines are discarded before presentation and cannot fight local prediction.

Protocol v84 binds reliable scene-entry requests and their authoritative replies to the player life epoch. The server
checks the epoch before scene replay admission, so a delayed request from a dead incarnation cannot consume a sequence
or relocate its respawned successor; a current-life request using that same sequence remains admissible. On respawn,
the client retains its current authorized scene but clears offered/pending/rejected transition state and rejects
old-life replies. Sequence zero remains the server bootstrap reply, while a listen server's first explicit scene entry
creates life epoch one without inventing a transport identity path. Deterministic tests cover stale-life rejection and
same-sequence current-life acceptance; the encrypted runtime scenario covers bootstrap, death, respawn, and subsequent
scene transitions under disposable loss and reliable retransmission.

The local command stream treats equipment as a server-confirmed revision. It records the latest authoritative server
tick when a selection is sent and does not emit weapon action bits until a strictly newer player snapshot confirms the
requested slot. Delayed snapshots from an earlier selection therefore cannot authorize an action, while locomotion and
evade remain responsive. The movement packet no longer has enough information to forge or reorder an equipment change.

Local prediction uses the same action replay while recording commands, not only after reconciliation. Pending samples
advance with fixed authoritative evade velocity and remaining action duration, and a reconciled in-progress action
seeds subsequently recorded commands. The former ordinary-walk recorder could place a backflip sample in front of Link
until the first snapshot arrived; that duplicate movement interpretation is removed. Starting an evade or primary
action consumes the first sample's elapsed time exactly once, matching the fixed-tick server duration.

Protocol v58 retains the v57 player split and also gives every projectile snapshot the authoritative server entity
slot/generation. A reliable `NetworkProjectileLifecyclePacket` is the only stream allowed to establish or retire a
remote projectile lifetime. Transform snapshots that arrive before creation, after removal, or for an earlier
generation are discarded. Remote actor maps are keyed by owner, projectile kind, command-correlation number, server
slot, and generation, preventing delayed traffic or reused client numbers from mutating a successor. Projectile
terminal state is sent before reliable removal so impact presentation can complete without allowing resurrection.
Fish and lure state in the same protocol now follows the owner visibility graph: reliable active/inactive packets are
the exact-lifetime enter/leave baselines, while only lure motion is disposable and coalesced. Per-observer, per-owned-
entity server sequences prevent a delayed lure transform from recreating a lifetime after its reliable leave without
coupling unrelated observers' ordering.

Protocol v67 deletes the general player-presentation stream after all of its fields became redundant. Reliable
lifecycle now establishes player identity and scene, and a generation-matched authoritative snapshot makes the remote
actor renderable. Fishing presentation owns and orders its independent sequence. Corpse pose is captured directly from
the authoritative player snapshot. The old packet, adapter, server component, inbox queue, dispatcher routes, and
project files are deleted rather than retained as a compatibility path.

`ClientReplicationInbox` is the wire-to-domain admission boundary for player state. After packet shape, active
lifetime, generation, server-tick, and life-epoch validation, it immediately converts
`NetworkPlayerSnapshotPacket` into a protocol-independent `PlayerSnapshot`. `NetworkRuntime`, local prediction,
remote replica storage, and native presentation coordinators can poll only that semantic state and cannot inspect or retain the
wire layout. Replacing or retiring a reliable player lifetime purges any pending typed snapshot for the old
generation. Fishing presentation follows the same rule: the inbox converts validated telemetry into
`FishingPresentationState` before runtime and rendering consumers can observe it.

Projectile snapshots also terminate their wire representation at this boundary. After exact projectile-lifetime and
sequence admission, the inbox queues `RemoteProjectileReplicaState`; runtime, presentation policy, the game bridge,
and the native renderer never receive `NetworkProjectileStatePacket`. A reliable generation replacement erases a
pending state for its predecessor immediately, while reliable retirement creates a semantic inactive state for the
exact entity and logical projectile identity. Packet-to-domain conversion therefore occurs once, before any client
gameplay or Ocarina-facing code can retain projectile state.

`ClientWorldState` consumes objective and structure packets into protocol-independent snapshots keyed
by both exact `EntityId` and stable logical identity. `ClientReplicationInbox` validates and orders each wire packet,
then emits `ReplicatedObjectiveState` or `ReplicatedStructureState`, combining the semantic simulation snapshot with
an explicit active/retired lifetime flag. Runtime, the game bridge, and tower-defense presentation therefore cannot
inspect packet fields or transport sequences. `ClientWorldState` rejects stale same-slot generations, conflicting identities,
and wrong-generation retirements. Reliable inbox streams coalesce repeated pending snapshots per logical
entity rather than discarding everything beyond a 256-packet FIFO limit, so a large late-join interest reconciliation
retains every relevant entity while high-frequency revisions collapse to their newest value. The game bridge no longer
leaves these authoritative packets queued and unused.

The listen-server presentation is observer `0`, not a privileged bypass. Once its local authoritative player exists,
player, owned-entity, corpse, objective, and structure reconciliation includes observer `0`; exact
lifecycle/state packets enter the same `ClientReplicationInbox` admission methods used by a socket client. Its own
authoritative player snapshots are admitted for local prediction reconciliation, while remote player, projectile,
fishing, and WvW state remains interest-scoped. A dedicated server has no player `0`, creates no phantom presentation
observer, and does not accumulate client combat state. Player snapshots and fishing presentation coalesce to the newest
pending value per player instead of sharing a 256-packet FIFO, preserving every player in a large interest set.

Remote movement presentation uses `RemotePlayerInterpolation`, a protocol-independent 30 Hz server-tick buffer. It
interpolates position and heading at a two-tick presentation delay, follows the shortest angular path, and permits at
most three ticks of velocity extrapolation during packet gaps. Scene, life, and entity-generation changes reset the
buffer. Native actor updates consume the time-derived pose directly; they no longer move a fixed percentage toward a
packet per frame, so render/update cadence does not alter remote movement speed or latency.

`RemotePlayerPresentationRegistry` owns the final client render lifetime after inbox admission. Live render records
are keyed by the authoritative player `EntityId`, while player ID remains ownership/name metadata. A private positive
`int16_t` handle is allocated only for the native Actor spawn API and is never sent over the network; therefore large
server player IDs are neither truncated nor rejected at the renderer boundary. Replacing a player generation reuses
the private handle, retires the exact old Actor and its owner-scoped fishing/projectile presentation, and prevents that
Actor's delayed destroy callback from clearing its successor. Same-slot stale generations, conflicting owners, and
wrong-generation retirements are rejected. The bridge's duplicated `entityIndex`, `entityGeneration`,
`hasEntityIdentity`, and player-ID-as-Actor-parameter paths are deleted.

`RemotePlayerReplicaStore` composes that exact lifetime with the latest protocol-independent `PlayerSnapshot`, motion
interpolation, and fishing-presentation interpolation. Scene changes, generation replacement, and retirement reset or
erase the complete replica atomically; stale server ticks and prior life epochs are rejected inside the store as well
as at the wire inbox. Native Actor resources remain downstream of this store and no longer create a second
snapshot/interpolation lifetime that can outlive or disagree with the authoritative presentation registry.

`NativePlayerPresentationComposer` is the explicit boundary between those protocol-independent replicas and the
Ocarina model. It derives equipment models, action flags, animation semantics, aim pose, and fishing visuals from
authoritative player/fishing state. Packet structs and global fishing lookups no longer mutate an anonymous render
struct inside the bridge.

`NativeRemotePlayerRenderer` is now the sole owner of native live-player and corpse Actor pointers, registration,
callbacks, skeletons, colliders, animation state, fishing-line solver buffers, spawn/cull reconciliation, and scene
shutdown detachment. The bridge submits entity-keyed presentation state and never dereferences a remote-player Actor.
Live players and corpses use distinct Actor types, so their independently allocated private `int16_t` handles cannot
alias and bind a live player to an unrelated corpse. The superseded bridge structs, maps, callbacks, registration,
and fishing drawing path are deleted rather than disabled.

Remote arrows use `RemoteProjectileInterpolation`. Disposable 20 Hz
motion snapshots are buffered by generation-bound actor records, binary rotations follow their shortest wrapped path,
and velocity extrapolation is capped at 100 ms. A phase change starts a fresh motion segment. Authoritative arrow
stuck states bypass interpolation and snap to the exact server position; the client no longer runs
a second projectile/world line test that could invent a conflicting impact. This layer moves render actors only and
never participates in damage, blocking, collision, or lifetime decisions.

`RemoteProjectilePresentationRegistry` owns the corresponding native Actor lifetime by authoritative projectile
`EntityId`. Owner player, command-correlation ID, and projectile kind remain a complete logical identity used for
validation and owner cleanup; they are not the render-map key. This fixes the former tuple key that omitted projectile
kind. A private positive `int16_t` handle lets the Actor initializer resolve its entity and bind its record before
`Actor_Spawn` returns. The old blank initialization followed by post-spawn mutation of owner/projectile/entity fields
is deleted. Exact retirement, logical replacement, owner retirement, and a delayed old Actor destroy callback cannot
remove a successor generation.

`RemoteProjectileReplicaStore` now composes that registry with the latest semantic projectile state and its bounded
interpolation buffer. Arrow phases are explicit client-domain values rather than wire constants. Duplicate
or stale sequences, kind/phase mismatches, malformed transforms, and any attempted transition away from an
authoritative terminal impact are rejected before native presentation changes. Generation replacement and owner
retirement erase the complete old replica atomically. `NativeProjectileRenderer` is the only Ocarina-facing layer: it
owns the native Actor registration, callbacks, spawn/cull reconciliation, and Actor-pointer records while reading the
replica store as immutable presentation truth. `NativeRemoteProjectilePresentationController` tracks, retires, and reconciles entities, so
packet state and interpolation can no longer outlive or disagree with the exact projectile lifetime.

Fishing cosmetic telemetry is independent of both render cadence and the Win32 move-loop pump.
`LocalFishingUpdateStream` coalesces unchanged rod/line telemetry and lure controls to 20 Hz, while deploy, reel,
lure-type, scene, and fishing-phase transitions are emitted immediately and deactivation sends one explicit undeploy.
Lure deploy/undeploy and scene/activation transitions use reliable transport because they create or retire an
authoritative entity; periodic and reel-only controls remain disposable high-priority updates.
Duplicate callbacks at one timestamp cannot allocate duplicate sequences. Remote cosmetic samples are consumed by
`RemoteFishingPresentationInterpolation`, which resets on exact entity, scene, or phase changes and interpolates
continuous rod/lure/fish pose values over one sample. The bridge's shared packet timestamp and copied
`previousState` blend are deleted. Authoritative lure and hooked-fish identity, position, species, and length remain
separate simulation entities and are never inferred from this cosmetic stream.
The native fishing reader returns one standard-layout `NetworkGameLocalFishingVisual` snapshot containing exactly the
fields transmitted by `FishingPresentationState`. The former output-pointer fanout and its unused lure position, hook
flag, fish position/species/length, room, parameters, and home-coordinate calculations are deleted.
`NativeLocalFishingController` is the only native-to-semantic adapter for this local stream. It samples the visual
snapshot and reel input, converts every field into typed presentation state, and asks `LocalFishingUpdateStream` for
cadence/lifecycle decisions. `NativeClientOutboundSubmission` receives only optional semantic presentation and lure-control
submissions, so it no longer interprets rod state or copies native fishing fields.

`LocalFishIntentStream` separately owns one-shot hook/release interaction sequence identity. It accepts semantic fish
action from the native callback, rejects malformed observations, skips sequence zero across
wraparound, and returns an immediate request for serialization. It deliberately retains no retry queue: once the
transport rejects an observation, that action cannot execute later after lure, scene, equipment, or ownership
preconditions have changed. The bridge-owned `nextFishIntentSequence` counter is deleted.

`RemoteFishingEntityState` is the protocol-independent client mirror for those authoritative fish and lure entities.
`ClientReplicationInbox` converts validated `NetworkFishStatePacket` and `NetworkLureStatePacket` values into
`RemoteFishEntity` and `RemoteLureEntity` immediately after exact-lifetime and sequence admission. Runtime and the
game bridge expose only those semantic entities. `RemoteFishingEntityState` therefore receives only inbox-admitted
updates and stores current semantic state rather than wire packets, sequence floors,
or a delayed queue. Exact `EntityId` is the primary store key; owner and stable fish identity are secondary indexes used
only for equipment composition and native fish binding. It rejects malformed semantic values, an entity claimed by a
different owner, a second owner claiming an active identity, and retirement of anything except the exact current
generation. Replacement, retirement, owner removal, and reset update every index atomically. Active fish/lure packet
admission additionally requires the owner's exact authoritative scene. A reliable player scene transition immediately
purges queued and already-presented old-scene fishing/projectile state instead of waiting for disposable updates. The
former action-keyed hook/release
`multimap` and its C consume API are deleted, so an old transition cannot execute later, reorder hook versus release,
or grow while its actor is absent. Player removal immediately drops active fish/lure presentation; ordering and exact
lifetime policy remain solely in `ClientReplicationInbox`.

`CorpsePresentationRegistry` is the client-side lifetime boundary for retained bodies. Corpse render records are keyed
by the authoritative `EntityId` and are stored separately from live player records; a source player ID is presentation
metadata, never corpse identity. The registry rejects stale active generations and stale retirements, replaces a reused
slot only with a newer generation, and binds each exact lifetime to a private negative `int16_t` handle solely because
the remaining native Actor spawn API exposes that parameter width. Reusing that private handle during a generation
replacement cannot let the retiring Actor erase its successor because each Actor also retains the exact entity key.
`ClientReplicationInbox` validates and orders the corpse packet, then converts it to `CorpsePresentationState` before
queueing it. Runtime, the game bridge, the presentation registry, and native rendering never receive
`NetworkCorpseStatePacket`; the former bridge-side field-by-field packet decoder is deleted. The former
negative-player-ID corpse records and bridge-owned pseudo-ID allocator are deleted.

Protocol v57 originally split player replication into authority and presentation streams and carried the authoritative player
entity slot/generation on every player-associated snapshot, pose, fishing, and lifecycle packet. Reliable ordered
`NetworkPlayerLifecyclePacket` create/remove events are the only messages allowed to establish or retire a client
render record. Disposable snapshots and visual telemetry must match that active lifetime exactly, so delayed data
from an older connection cannot replace or remove its successor. `NetworkPlayerSnapshotPacket` is the
only player movement/action/health authority. Protocol v67 subsequently removed the generic presentation packet;
`NetworkFishingPresentationPacket` remains the optional high-volume rod/line/fish-pose stream.

| Group | Current fields | Destination |
| --- | --- | --- |
| Server simulation | position, heading, aim, velocity, health, action state | `NetworkPlayerSnapshotPacket` only |
| Native presentation | model/equipment choice, animation, bow draw pose | derived locally from `NetworkPlayerSnapshotPacket` semantics |
| Fishing presentation | rod, line, lure draw transforms, hooked-fish animation pose | `NetworkFishingPresentationPacket`; lure/fish identity and position remain simulation-owned |
| Superseded gameplay telemetry | client life/action flags | deleted; melee state/base/tip were removed in protocol v55 |

Life, sword-action validation, projectile creation, and fishing equipment/lifecycle all read
explicit commands or simulation state. Native item, melee geometry, and life/action telemetry no longer authorize shared
state changes. The old mixed send/receive API and shared mixed-state packet are gone. The replication coordinator owns
the generation-bound `FishingPresentationState` cosmetic component. Fishing samples have an independent sequence;
older sequences and wrong entity generations are rejected. Death, weapon changes, scene changes, disconnect, and reset
remove the component centrally. `ServerWorld` and `NetworkRuntime` retain no presentation packet map.
The legacy actor bridge composes snapshots and optional fishing state only into a client-local
`RemoteRenderState` at the final rendering boundary.

Client prediction uses `ClientPrediction`: every accepted local command retains its exact sequence, player life epoch,
scene, normalized input, heading, actions, and measured command-frame duration. Scene admission supplies that epoch to
`LocalPlayerCommandStream`; the runtime rejects a send if the predicted epoch no longer equals current authority, so
the command replayed locally is exactly the incarnation transmitted. An authoritative snapshot removes all acknowledged
commands, then replays every newer command from the server position through the same pure horizontal locomotion function
used by `PlayerSimulation`. The replayed present is compared with the actual native Link presentation, so Ocarina world
collision or a server player/world collision divergence becomes an explicit correction instead of being hidden by a
parallel predicted-position sample. The former raw-transform recording and position-only reconciliation entry points
are removed; every pending sample is a replayable command and current-scene reconciliation always starts from a complete
authoritative `PlayerSnapshot`. Skipped disposable command numbers and acknowledgements whose samples were evicted
during a long stall retain and replay only commands newer than the acknowledgement; duplicate/out-of-order commands
cannot advance prediction. Prediction accepts commands, acknowledgements, and snapshots only for its explicitly
established life epoch. The local-vitals admission result gates weapon projection and reconciliation together, so a
snapshot rejected as stale cannot still move Link. Stale or duplicate snapshots cannot replace the correction.

Correction error decays by elapsed time with a 150 ms half-life rather than a fixed percentage per game update;
corrections therefore settle identically at 20, 38, or 60 updates per second. Large errors snap, stalls are capped, and
reliable respawn advances a generation-checked life epoch and clears the previous life's prediction state. Inbox epoch
fences reject a delayed pre-respawn snapshot even if it arrives after the reliable respawn command. The respawn inbox
boundary also purges admitted old-life combat events and projectile decisions that may have queued while native
gameplay was frozen. Client movement, action, equipment, projectile, fish, lure, and fishing-presentation streams then
restart their per-life sequence spaces together; `PlayerSimulation` clears the matching authoritative movement/action
floors and acknowledgement. Native Link remains
the immediate animation/Ocarina-collision adapter, while replay and all shared outcomes remain rooted in server state.

Reliable gameplay replay admission is scoped to a player's authoritative life epoch. `ServerIntentAdmission` receives
both the server snapshot epoch and claimed intent epoch, rejects mismatches before allocating or advancing state, and
atomically starts fresh per-action sequence floors and keyed action cooldowns when the authoritative incarnation changes.
Weapon selection, arrows, lure control, fish actions, scene entry, and structure actions all use this gate only after
resolving the authoritative player. A delayed high-sequence packet from a dead life therefore cannot poison a lower
sequence from the respawned life, and replay state is consumed before mutable equipment, scene, proximity, or ownership
conditions are evaluated.

Reliable action edges embedded in player commands have a second, movement-window fence. An edge executes only when it
arrives with a newer movement sample or matches the latest movement sequence before that command's six-tick timeout.
An edge arriving after the window expires still advances the reliable action replay floor, but it is not queued and can
never execute later when recovery, climbing, UI, or another native state happens to become idle. A fresh movement window
with a newer action sequence remains immediately eligible.

Protocol v86 removes bombs and grass rather than preserving dormant compatibility paths. Bomb simulation, intents,
snapshots, rendering, and replication are gone. Grass/boulder world-object actions, carry state, timed restoration,
dynamic-object packets, client mirrors, and persistence records are also gone. The test01-only runtime no longer
accepts room actor lists, so no compatibility slots are retained. The world-state schema is version 2 and rejects
older files that contained removed world-object records.

The native actor registry is no longer a 402-slot Ocarina compatibility array. Test01 supplies Link through its
start-position command and contains no room actor list. Four built-in IDs are contiguous: Link (0), arrow (1), fish
(2), and fishing controller (3). Network presentation actors register dynamically from ID 4. Removed actor names,
unset padding, descriptions, overlay headers, and actor-specific player branches are deleted rather than retained as
unreachable compatibility data.

Protocol v88 binds a reliable respawn to the exact authoritative player entity generation as well as its life epoch.
`ClientReplicationInbox` rejects a respawn for a retired or replacement entity before it can clear prediction, advance
the local epoch, or invoke native respawn. Accepted wire packets become semantic `PlayerRespawnEvent` values at that
boundary; the public runtime polling API and gameplay bridge no longer expose `NetworkPlayerRespawnPacket`.

Local one-shot controls are sample-local action edges. `ActionIntentFrame` is reset at every controller sample, so a
weapon selection, sheath toggle, evade, or bow click rejected by the current native action state cannot execute after
climbing, recovery, damage, or another busy state ends. Reliable network action edges remain independently sequenced
and are consumed by exactly one authoritative fixed tick; stale command windows and busy-state presses are discarded.

Protocol v89 removes the unused client render tick from player commands and their wire packet. Local input still uses
the gameplay-frame counter to avoid sampling one native frame twice, but only generation-bound movement/action
sequences cross the network. The authoritative command and simulation state therefore cannot imply that a client-owned
clock schedules server work, and each high-rate intent is four bytes smaller.

Protocol v90 reduces fish interaction to a sequence, life epoch, and semantic action. Native actor fingerprints remain
only in server catalog definitions and outbound presentation state needed to associate the chosen fish with an Ocarina
model; they no longer cross the client-to-server gameplay boundary.

Protocol v91 removes scene IDs from arrow-fire and lure-control intents. The packets are respectively twelve and nine
bytes; scene identity exists only in server-owned player/entity snapshots and outbound lifecycle state.

Protocol v92 removes the scene ID from client fishing-presentation telemetry. The server accepts telemetry only for the
sender's exact current life epoch and fishing loadout, derives its scene from the authoritative player snapshot, and
stamps that scene into outbound presentation state for observers.

Protocol v93 removes the scene ID from high-rate player movement/action packets. Local commands remain scene-aware for
prediction and reconciliation, while the server binds each decoded packet to the authenticated player's current scene
before simulation admission. The simulation still rejects stale life epochs and scene mismatches, but clients can no
longer assert world membership in movement traffic; each command is four bytes smaller.

Protocol v94 removes projectile identity from arrow-fire intents. The client sends only a life-scoped action sequence;
`ProjectileSimulation` allocates the replication ID when the server creates the arrow, and the reliable result binds that
ID back to the client's predicted native presentation. Rejected actions return no projectile ID. The server retains the
latest decision per player so a reliable duplicate receives the same assignment without creating a second arrow. Arrow
intents are now eight bytes, and no client-selected value enters projectile lifecycle or interest keys.

Protocol v95 removes destination scene IDs from scene-entry intents. Login always uses the server's default spawn. Later
requests can only consume a one-shot `SceneTransitionAuthority` grant bound to the player's current life epoch and a
server-configured destination; an ungranted request is rejected in the current scene. Reliable duplicates replay the
stored outcome without consuming another grant or changing scenes twice. Native scene IDs remain client-side correlation
data only, while the wire request is eight bytes and cannot choose world membership. An accepted scene authority packet
is queued before destination interest reconciliation on the same reliable peer stream. Client frame integration drains
that authority before destination-scoped lifecycles, poses, and world baselines, so presentation cannot enter a scene
before local admission does.

Protocol v96 adds one reliable strategic-topology snapshot containing a revision, typed camp/tower/keep records,
influence-region keys, and directed supply routes. This is global durable configuration rather than a spatial actor
stream: a newly admitted client receives the complete graph once, and administrative graph mutations replace it
atomically. Replayed revisions, duplicate identities or edges, missing endpoints, invalid kinds, and non-camp route
sources are rejected before client world state changes. An empty newer snapshot cleanly retires the previous graph.
Ownership and capture progress continue through interest-scoped objective snapshots; the topology packet contains no
score or point accumulator.

Protocol v97 extends that atomic snapshot with normalized undirected influence-region adjacency edges. Edge identities
and region pairs are unique, both region endpoints must be authored strategic sites, and server administrative mutation
also verifies that the regions belong to objectives in the same scene. Objective removal cascades attached adjacency;
malformed, reversed, duplicate, dangling, oversized, stale, and replayed graph data is rejected before client state
changes. No territory edge carries a score, weight, or points-per-tick value. The graph currently exists only in server
memory; no persistence file is loaded or saved.

Fishing presentation packets are cosmetic telemetry rather than entity authority. Before relaying one, the server binds
its player, entity generation, and scene to the authenticated player; derives lure phase and line endpoint from the
server-owned lure; and permits hooked-fish animation only for the server-owned fish. A missing lure clears lure and line
visuals, while an inconsistent player/lure/fish lifetime rejects the packet. Rod bend and hook animation values remain
disposable client presentation data and cannot create, move, hook, or release a simulated entity.

## Authenticated gameplay ingress

Wire gameplay intents terminate in their network adapters. Those adapters validate packet shape and convert movement,
weapon selection, scene entry, arrow fire, lure control, fish interaction, and structure actions into unbound semantic
commands; no adapter API can attach a player or scene supplied by the caller. `ServerGameplayIngress` is the single
authenticated boundary into `ServerWorld`. It overwrites every command owner with the transport session's player ID and
derives movement scene membership from the current authoritative player snapshot. The listen-server player uses the same
ingress instead of a local authority bypass. Old-life, replay, loadout, action-state, range, collision, ownership, and
team policy remain in `ServerWorld` and its specialized simulations after identity binding.

Decoded protocol ownership is split by direction. `ClientProtocolEndpoint` owns the encryption-accept transition and
admits every decoded server message through `ClientSessionIngress`; it cannot update a native actor or send gameplay
authority decisions. `ServerProtocolEndpoint` owns key exchange, signed identity verification, and routing of decoded
chat, voice, scene, movement, weapon, structure, fishing, and projectile messages into the authenticated communication
or gameplay ingress services. `NetworkRuntime` no longer implements either protocol sink and cannot grow packet-specific
authority branches merely because it owns the UDP transport. Protocol and runtime integration tests exercise endpoint
admission, encrypted identity establishment, every retained gameplay route, and rejection of malformed pre-identity
traffic.

Disposable fishing telemetry follows the same boundary. `FishingNetworkAdapter` produces an unbound
`FishingPresentationIntent` containing only its life epoch and cosmetic body. `ServerGameplayIngress` resolves the
authenticated player's exact entity, life, scene, loadout, lure, and fish; replaces all identity fields; and returns an
`AdmittedFishingPresentation` only after authority constraints succeed. Runtime has one common host/remote execution
path that stores and relays this admitted result, so transport no longer reads gameplay state or duplicates listen-server
admission policy. The removed adapter overload cannot bind telemetry to an arbitrary caller-supplied player snapshot.

Tests deliberately submit commands containing another player's ID and scene, then prove only the authenticated player
changes in the authoritative world. The former adapter overload that accepted arbitrary owner and scene values has been
deleted; transport fault tests bind identities only inside their isolated test harness.

## Asset residency

Archive membership does not imply startup residency. Audio startup enumerates sequence and sound-font filenames only to
build their numeric-ID maps; it does not deserialize the resources or preload the `audio` directory. An authentic
sequence and its sound fonts are loaded when the sequence is first requested. Sound-effect, drum, and instrument entries
retain their sample paths inside the loaded font and resolve each individual sample on its first playback request. Font
setup does not traverse or preload any complete sample table.

This separation lets archive pruning follow gameplay reachability without making startup cost proportional to the
historical Ocarina audio catalog. The central request filter admits only player-bank actions, Adult Link voices, the
retained swords/shield/bow/fishing effects, and their water interactions. Requests from removed banks never enter the
audio queue.

The runtime archive now contains only the sound-effect sequence, its two indexed fonts, and 82 samples for Adult Link
and the four retained item paths. Music, child, NPC, enemy, ambient, menu, and removed-item samples are absent. Numeric
sound-effect slots remain unchanged so the native sequence bytecode and computed surface offsets remain stable.

O2R is the sole packaged-resource format. Every typed resource is binary, so the XML loader, XML factories, scene-command
XML parsers, and tinyxml2 dependency have been removed. Archive reads are serialized around the shared libzip handle and
must consume the complete entry; partial, malformed, or absent audio resources fail validation without unsafe destructor
cleanup. Drum, instrument, and direct-effect playback all reject unresolved samples before synthesis.

## Articulated player hit geometry

Player hit geometry is defined as eleven capped regular hexagonal prisms spanning head, torso, waist, upper arms,
forearms, thighs, and shins. Each endpoint comes from an `AuthoritativePlayerSkeletonPose`, and each supplied radius must
be calibrated against the visible Adult Link model. Hands, feet, hair, hat, and sheath are deliberately excluded. The
collision geometry therefore follows bent and animated limbs instead of approximating the player with one upright
cylinder.

The retained renderer's body-part positions are presentation data and are not trusted for damage. A deterministic
headless pose sampler derives all joints from the authoritative snapshot's position, heading, velocity, action, aim
pitch, and server tick. Both melee and arrows use the resulting articulated rig as their live narrow phase; the former
broad body cylinder is no longer used for PvP damage. Clients retain the latest replicated copy of that exact rig so F1
can display real combat collision alongside legacy Zelda collision without accepting client-submitted limb positions.
Protocol v102 carries the server-selected limb region on reliable damaged melee and arrow results. Blocked hits carry no
body region, malformed region values are rejected at ingress, and clients never submit or choose the region used for
damage.

The F1 diagnostic keeps only one local rig source: the world-space matrices produced by Link's current skeleton draw.
Local authoritative snapshots are deliberately excluded from the debug registry so their lower update cadence cannot
overlay the animated renderer sample. Remote entries are replaced by their native skeleton sample whenever their actor
is drawn; headless authoritative geometry remains the server's combat source and is never accepted from a client.
Its regular hexagonal prism vertices are immutable; each animated limb supplies a dynamic model matrix. This avoids
backend vertex caching freezing a world-space debug mesh at its first uploaded pose.

Protocol v105 makes respawn a complete reliable incarnation baseline. The generation-bound packet now carries scene,
server tick, position, heading, and selected weapon alongside the new life epoch. `ClientReplicationInbox` advances the
epoch floor when that packet is admitted, so reordered old-life snapshots are rejected, while the semantic respawn does
not depend on a disposable snapshot arriving first. Native respawn atomically resets per-life command, prediction,
projectile, fishing, vitals, and input state; validates the authorized entity and scene; and seeds Zelda's DOWN respawn
record from the server baseline. The prior negative respawn flag was removed because `Player_Init` intentionally ignores
stored placement for negative values below -1.

Local primary-action animation is also a one-way presentation projection. A pure client policy converts prediction into
unavailable, idle, or active progress, and a native adapter writes that result into the local Player once prediction has
advanced. The actor no longer calls through the network bridge or reads prediction state while updating animation.

Native fishing is separated from networking in both directions. The retained fishing actor exports a plain
`FishingLocalVisual` snapshot and emits generic hook/release gameplay events through `FishingGameplay`; it never names or
calls the network runtime. `NativeLocalFishingController` converts those events into sequenced reliable intents and
converts visual snapshots into disposable presentation updates. A hook lifetime is provisional until transport accepts
its intent, and a failed submission restores the prior state, preventing an unavailable connection from permanently
wedging later hook or release actions.

Native arrow actors follow the same one-way boundary. Player reports predicted
presentation creation and the semantic fire edge through `ProjectileGameplay`;
EnArrow reports presentation retirement there. The installed native sink owns
the conversion to `NativeLocalProjectileController` bindings and sequenced
projectile intents. No retained C actor includes transport or session headers merely to
create, fire, or destroy an arrow.

Native gameplay notices and externally controlled fish follow provider
boundaries as well. Fishing emits catch text through `GameplayNotification`
without knowing whether the presenter is the multiplayer HUD, and reads an
optional `FishPresentation` keyed by stable scene/spawn identity without
knowing player IDs, replicas, interpolation, or transport. The networking
composition root installs and removes both providers with the client session.
The superseded remote-fish network bridge header and its network-named native
fishing helpers were deleted.

Inbound client replication is coordinated by `NativeClientInboundReplication`.
It drains admitted player lifecycle and snapshots, projectile decisions and
replicas, fishing presentation entities, scene authority, corpses, and WvW
objective/structure state into semantic client stores. `NativeClientNetworkSession`
constructs that dependency graph and schedules it; it no longer implements
those state transitions itself. The coordinator is destroyed before its
referenced runtime during shutdown, making its non-owning dependency lifetime
explicit.

Outbound client submission is coordinated by `NativeClientOutboundSubmission`.
It samples scene-authorized local commands and fishing presentation, submits
weapon, movement/action, lure and arrow intents through the runtime, and owns
the acceptance result for every provisional reliable stream. Failed sends are
resolved at this boundary so native actors cannot retain an unacknowledged hook
or projectile lifetime. `NativeClientNetworkSession` installs generic gameplay sinks and
schedules this coordinator; it no longer constructs those submissions itself.

Native per-frame projection is coordinated by
`NativeClientFrameReconciliation`. It consumes already-admitted respawn and
combat events, reconciles remote player/projectile renderers, applies local
prediction correction, projects the just-sampled local action state, and queues
occlusion-tested nameplates. Its transport-frame entry point remains active
while native gameplay is frozen by death, ensuring the reliable server respawn
command cannot be stranded behind the removed game-over UI. Packet draining,
authoritative decisions, and outbound command construction remain outside this
presentation boundary. These three coordinators leave `NativeClientNetworkSession` as a
composition root and scheduler rather than another gameplay implementation.

`NativeClientSessionLifecycle` is the complete generation boundary for native
and semantic client state. A newly established or replaced authenticated
transport generation retires native actors first, resets every player,
projectile, fishing, corpse, prediction and local-intent store, clears collision
diagnostics, then rebinds renderers to the fresh stores. Shutdown uses a separate
post-scene detach path that never dereferences Actor pointers after Ocarina has
destroyed its actor context. The bridge no longer owns a reset checklist, so a
future replicated entity must join this explicit lifecycle dependency graph
rather than surviving reconnect accidentally.

`NativeClientUpdateCoordinator` owns the remaining client scheduling contract.
Transport admission and high-rate inbox draining happen before presentation;
scene authority is committed before destination-scoped replicas; gameplay-frame
reconciliation precedes command submission; and local presentation is projected
only after prediction has consumed the current command. Reconnect observation
resets a pure `ClientFrameClock`, which supplies a deterministic first-frame
default, rejects negative elapsed time, and clamps stalls to 250 milliseconds.
`NativeClientNetworkSession` constructs this dependency graph, installs native
provider callbacks, and owns process startup/shutdown. The transport-neutral
`ClientRuntime` lifecycle ABI is the only surface retained C code calls; its
implementation forwards directly to that session without owning gameplay state.
