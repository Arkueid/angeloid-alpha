---
name: debug-visualization
description: RigidBodyRenderer, WorldAxis, console dump, physics debug overlay
type: reference
---

## RigidBodyRenderer (framework/scene/)
- B key toggles wireframe display
- Shows rigid bodies as colored wireframe shapes + joints as lines
- Lazy-initialized on first draw (needs active GL context)
- `updateFromPhysics(world)` reads Bullet body transforms, packs into texture
- `render()` uses body matrix texture + local shape VAO via "rigidbody" shader
- `mShowPhysicsDebug` flag controls visibility (independent of physics enabled state)
- Doesn't require physics to be ON — just toggles display

## WorldAxis (framework/scene/)
- X key: RGB axis lines
- G key: ground grid
- Uses "axis" shader

## Console Dump
- F key: `physicsWorld.debugDump()` prints displaced bodies to stdout
- Requires physics enabled (Y)

## Colors
Body wireframes: yellow, orange, cyan, magenta (cycling)
Joint crosses: green lines
Axis: X=red, Y=green, Z=blue

## Known Issue
Rigid bodies show PMX file positions (T-pose). Mode 0 bodies follow bone pose via `updateMode0Bodies()` (runs every frame). Mode 1/2 bodies stay at initial positions until physics simulation runs.
