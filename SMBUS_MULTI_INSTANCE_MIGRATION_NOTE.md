# SMBus Multi-Instance Migration Note

Date: `2026/06/27`

## Background

`M031BSP_Software_SGPIO_I2C_Slave` currently keeps three SMBus slave ports
alive:

- `I2C0`
- `I2C1`
- `USCI0`

`I2C1` shares `PA2/PA3` with the SGPIO profile and is selected by the
`BP_TYPE` boot strap. `I2C0` and `USCI0` are initialized as additional SMBus
slave ports.

## Current Rule Clarification

The final per-port command/profile rule is still under review.

Confirmed rules:

- `BP_TYPE=LOW` selects the SGPIO profile.
- `BP_TYPE=HIGH` selects the SMBus slave profile.
- The `BP_TYPE` rule must continue to control `PA2/PA3` ownership:
  `BP_TYPE=LOW` keeps `PA2/PA3` for SGPIO, and `BP_TYPE=HIGH` keeps `PA2/PA3`
  for I2C1 SMBus.
- `I2C0` is expected to continue running the SMBus Generic transaction-layer
  command set directly.

Rules still under clarification:

- `USCI0` may become a profile-selected SMBus path. Depending on an external
  I/O selection, this path may switch between a VPP vendor-protocol profile and
  a UBM Controller profile.
- The concrete `USCI0` selector pin, polarity, default profile, and VPP command
  contract still need to be defined before implementation.

## Migration Constraint

Do not directly replace the current SMBus path with the standalone
`M031BSP_I2C_Slave_SMBus` core without first making that core multi-instance.

The newer SMBus sample currently uses:

- one driver context
- one board-port binding
- global dispatch shadows/counters

That shape is correct for a single SMBus slave port, but it is not sufficient
for simultaneous `I2C0`, `I2C1`, and `USCI0` slave behavior.

## Required Future Work

A complete future replacement must provide:

- per-port driver context
- per-port IO binding
- per-port command/dispatch state
- per-port counters and writable shadows
- independent recovery state for each bus
- independent debug/event queues or port-tagged event queues

This avoids `I2C0`, `I2C1`, and `USCI0` affecting each other's transaction
state, counters, PEC/checksum behavior, or recovery path.

## Cleanup Target

When the multi-instance replacement is implemented:

- Remove legacy PMBus sample command names, logs, images, and macros from this
  workspace.
- Keep this workspace as `SGPIO + SMBus transaction-layer` only.
- Do not present this workspace as a PMBus-capable sample.
- Preserve simultaneous `I2C0`, `I2C1`, and `USCI0` SMBus slave behavior.
- Preserve `BP_TYPE=0` SGPIO ownership of `PA2/PA3`.
- Preserve `BP_TYPE=1` I2C1 SMBus ownership of `PA2/PA3`.
- Reuse the validated SMBus transaction-layer behavior from
  `M031BSP_I2C_Slave_SMBus` only after the core, IO layer, and dispatch layer
  are instance-safe.
