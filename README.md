# Temporal Rewind System

A production-ready temporal rewind plugin for Unreal Engine 5.5+.

World-level VCR controls — record, rewind, scrub, and commit any actor's state with zero engine modifications. Built on a ring-buffer history, opaque snapshot storage, and a delegate-driven lifecycle so you can wire in your own camera, UI, and game logic without touching the plugin source.

## Features

- One subsystem controls the entire world — no per-actor managers
- Implement `IRewindable` in C++ or Blueprint to make any actor rewindable
- Configurable buffer duration, scrub range, recording rate, and cooldown
- Delegate hooks: `OnRewindStarted`, `OnScrubUpdated`, `OnRewindCommitted`, `OnCooldownStarted`, and more
- Optional ready-made Scrub Widget UI included
- Debug console commands and Blueprint debug library

## Documentation

Full API reference, Quick Start (C++ and Blueprint), and integration guide:  
[TemporalRewind_Documentation.pdf](Docs/TemporalRewind_Documentation.pdf)

## Installation

1. Copy the `TemporalRewind` folder into your project's `Plugins/` directory
2. Regenerate project files and build
3. Enable the plugin in Edit → Plugins → Temporal Rewind

## Requirements

- Unreal Engine 5.5 or later
- C++ project (for IRewindable implementation — Blueprint-only path also available)

---

© 2026 Prasanna Keerthivasan. All Rights Reserved.
