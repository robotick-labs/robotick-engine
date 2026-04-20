# Operator Control And Connection Suppression

_Written 18th April 2026_

## Why This Note Exists

Studio operator control has moved from a dedicated robot-side RC model toward a
more general live-control surface.

The important shift is that Studio no longer needs special RC workloads to
temporarily override a connected input. Instead, it can suppress the incoming
data-connection for a real destination field, write that field directly, and
then release suppression so normal engine or REC control resumes.

This is worth naming because it is now a reusable Robotick/Studio pattern, not a
Barr.e-specific RC feature.

## Design Rules

The durable rules are:

- operator mappings live Studio-side, normally in `overlay/remote-controls`
- Studio writes true destination input fields, not RC proxy fields
- connected input suppression means "stop incoming data-connections writing this"
- suppression does not mean Studio permanently owns the field
- releasing suppression restores ordinary engine/REC-driven behavior
- unconnected fields do not need special suppression behavior

## The Pattern In One Sentence

Studio temporarily gates incoming data-connections at the destination input
handle, writes the real input field directly, and later re-enables the normal
connection path.

## Runtime Primitive

The engine-side primitive is `DataConnectionInputHandle`.

Each handle represents an incoming connected destination input/path. Local
data-connections and REC receiver writes both route their final destination copy
through that same handle.

The handle carries:

- destination path / field info
- destination memory and size
- an enabled flag for incoming data-connection writes
- copy helpers used by both local connections and REC

The semantic split is deliberately small:

- handle enabled: local/REC connection writes are allowed
- handle suppressed: local/REC connection writes are ignored
- telemetry writes are unaffected and may still write the field directly

This keeps suppression generic. It is not RC-specific, and it is not a Studio
ownership system.

## Tick-Time Consequence

The engine still sees a normal input field.

The difference is only whether incoming data-connections are allowed to perform
their final write for that destination. Once the connection writer reaches the
shared handle, the handle decides whether to copy or ignore the incoming value.

Studio telemetry writes remain ordinary input writes. They do not need a second
special override channel.

## Studio Field UX

Studio exposes suppression as field state:

- active connected fields show active connection chrome
- suppressed connected fields show suppressed connection chrome
- a field-level lightning control can suppress or re-enable the connection
- editing a connected field auto-suppresses before committing the write
- focus alone does not suppress

This gives the operator a clear visual answer to:

- is this field normally connected?
- is the connection currently allowed to drive it?
- am I currently overriding it from Studio?

## Studio Remote-Controls

`overlay/remote-controls` is now the preferred home for operator puppeteering.

The config describes:

- what each stick controls
- named modes for each stick
- per-mode dead-zone, transform, and scale
- the exact destination fields written by each mode
- button-to-field mappings

Selected stick modes suppress their connected destination fields before writing,
and reassert suppression as writes continue. When a mode is deselected or the
panel releases control, Studio re-enables those incoming connections.

Button writes can use the same direct-field model. For example, Barr.e's blink
button now targets the face model's real blink input rather than an RC proxy
field.

## Naming Convention

Mode names should describe operator intent, not the implementation detail.

Preferred examples:

- `locomotion` rather than `drive_wheels`
- `camera_orbit` rather than `chase_camera`
- `look_direction_eyes` for eye/gaze offset control
- `look_direction_head` for head/yaw-pitch amount control

This keeps config readable if the robot implementation changes.

## Launcher Semantics

The launcher should treat per-model play/restart consistently with the main
launcher controls.

Current rule:

- main profile run/restart performs the normal generate/build/deploy/run flow
- per-model play/restart should also use the full per-model run pipeline
- per-model stop remains a single-model stop

This matters because YAML/model/config changes often affect generated or
compiled output. A relaunch-only path can leave stale binaries running.

## Current Robot Shape

Barr.e, Alf.e, and Pip.e no longer need dedicated RC models for normal Studio
operator control.

Current direction:

- `robots/*/*.rc.yaml` holds Studio-side operator mappings
- real robot models own the actual behavior fields
- Studio writes those true destination fields
- connection suppression handles temporary operator override
- the legacy core `RemoteControlWorkload` remains available as an example or
  generic gamepad normalizer, but it is not the preferred puppeteering path

## Barr.e Example

Barr.e currently follows this pattern:

- left stick locomotion writes spine drive inputs
- right stick camera orbit writes simulator camera orbit inputs
- right stick look-direction modes write real face/spine expression inputs
- blink writes the face model's real `blink_request` input
- the former RC/mind toggle workload has been removed
- brain expressive outputs connect directly to spine and face remote inputs

The operator no longer takes control by switching Barr.e into a robot-side RC
mode. Studio temporarily suppresses the relevant incoming connections and writes
the destination fields directly.

## Practical Rule Of Thumb

If a control is about live operator puppeteering or tuning, prefer:

- Studio config
- true destination fields
- connection suppression for connected inputs

If a control is about autonomous robot behavior, prefer:

- normal model workloads
- local or REC data-connections
- telemetry only for observation and occasional tuning

That keeps the robot model honest: it defines behavior, while Studio defines the
operator surface used to puppeteer or adjust that behavior live.
