# Chery Omoda E5 Migration Design

## Goal

Migrate Chery Omoda E5 support from `khadafi-pilot` branch `e5-staging-new` into current sunnypilot `master-dev` using target-native opendbc and Panda safety patterns. Preserve source behavior already proven on the vehicle; change behavior only when required by target API compatibility or an evidence-based safety correction.

Deliver both lateral angle control and optional alpha longitudinal control. Alpha longitudinal remains disabled by default and is available only after its production safety path and tests pass.

## Source and Evidence

Source implementation:

- Repository: `/Users/macbook/Code/openpilot/khadafi-pilot`
- Branch: `e5-staging-new`
- Chery history: 22 Chery-related commits after the sunnypilot base commit

Validation route:

- Route: private full rlog supplied by vehicle owner, segment 0
- VIN evidence: WMI `MF7` identifies Chery; full VIN is intentionally not stored in git
- Firmware includes engine version `00.02.12`
- Firmware and CAN fingerprint match source `CHERY_OMODA_E5`

Observed full-rlog traffic:

| Address | Source bus | DLC | Frequency | Intended use |
| --- | ---: | ---: | ---: | --- |
| `0x03E` | 0 | 48 | 100 Hz | Engine and brake state |
| `0x1D3` | 0 | 8 | 100 Hz | Steering angle |
| `0x29A` | 0 | 8 | 50 Hz | Brake data |
| `0x316` | 0 | 8 | 50 Hz | Wheel speeds |
| `0x394` | 0 | 8 | 50 Hz | Driver steering torque |
| `0x360` | 0 | 6 | 20 Hz | Steering-wheel buttons |
| `0x4ED` | 0 | 8 | 10 Hz | Brake sensor |
| `0x345` | 2 | 8 | 50 Hz | Stock LKAS command |
| `0x3A2` | 2 | 8 | 50 Hz | Stock ACC command/state |
| `0x3A5` | 2 | 8 | 50 Hz | ACC status |

Forwarded and loopback copies appear on buses `128`, `130`, and `192`; safety RX checks use original vehicle buses only.

## Source Problems That Must Not Be Copied

The source is a working vehicle implementation and protocol evidence, but its safety gaps prevent treating it as production-ready without verification.

- Chery safety defines an empty `RxCheck` list. Current safety core calls the brand RX hook only for valid allowlisted RX messages, so speed, angle, brake, gas, torque, and ACC state never reach the hook.
- Longitudinal safety parameter parsing is inside `ALLOW_DEBUG`; debug and release behavior differ.
- No Chery libsafety test exists.
- Active steering has no explicit hard physical angle bound. Source comments conflict between `300 deg` and `+/-150 deg`.
- Driver torque is parsed but does not affect Panda authorization.
- Stock command forwarding is not explicitly blocked, allowing potential stock/openpilot command collisions.
- Controller and state code contain live debug prints, duplicate limiters, unused state, broad exception handling, stale stock-frame reuse, and a cruise-state ordering bug.
- Source car tests mostly assert constants and object construction; they do not verify safety behavior or bit-exact protocol encoding.
- Source longitudinal code treats raw ACC command values as acceleration limits without proving the conversion.
- Source marks stock AEB, FCW, door, and seatbelt states false instead of reporting verified values.

## Architecture

Vehicle and protocol code lives in the `opendbc_repo` submodule:

```text
opendbc_repo/opendbc/
  car/chery/
    __init__.py
    values.py
    fingerprints.py
    interface.py
    carstate.py
    carcontroller.py
    cherycan.py
  dbc/chery_canfd.dbc
  safety/modes/chery.h
  safety/tests/test_chery.py
```

Integration points:

- Add `cheryCanFd @35` to `opendbc/car/car.capnp` without renumbering existing values.
- Add Chery to global platform and fingerprint aggregation.
- Add `SAFETY_CHERY 35U`, hook declaration, mode include, and safety registry entry.
- Keep target sunnypilot catalog generation authoritative. Do not hand-edit generated catalog output unless its generator produces a required change.
- Record the resulting opendbc submodule pointer in the parent sunnypilot repository.

Do not copy source changes to `IGNORED_SAFETY_MODES`, joystick, maneuver tooling, branch scripts, editor settings, no-op MADS mixins, or debug documentation.

## Vehicle Interface

The target exposes one platform: `CHERY_OMODA_E5`.

Required behavior:

- Match the recorded CAN fingerprint and firmware versions.
- Use dynamic `CanBusBase` offsets for main, camera, and loopback buses.
- Use angle steering control.
- Use direct transmission type for the EV unless vehicle evidence contradicts it.
- Report parsed wheel speed, steering angle, driver torque, gas, brake, gear, cruise state, buttons, blind spots, and verified stock safety states.
- Never fabricate safety-relevant state as false. Unsupported signals remain explicitly unavailable or conservatively handled.
- If a required signal is absent or cannot be verified from the DBC and supplied rlog, do not infer it. Record desired behavior, candidate address/bus/DLC, existing evidence, and required capture in `opendbc/car/chery/KNOWN_GAPS.md`, then ask the vehicle owner before reverse engineering or selecting a signal.
- Remove manual steering-torque sign inference. Decode the verified signed DBC signal directly.
- Implement controller output with target-native vehicle-model angle limiting.
- Avoid stale stock checksum/counter reuse. Every transmitted frame receives a correct current counter and checksum.

Lateral and longitudinal capabilities:

- Lateral angle control is supported when all Chery safety checks are active.
- `alphaLongitudinalAvailable` is true only after longitudinal protocol and safety tests pass.
- Alpha longitudinal remains disabled by default and requires the existing sunnypilot alpha-long toggle.
- When alpha long is disabled, controller sends no ACC replacement frame and Panda rejects openpilot ACC TX.
- When alpha long is enabled, controller emits validated active and inactive ACC frames; no permissive bypass function is used.

## Safety Contract

### RX Checks

Required liveness checks use the original vehicle bus, observed DLC, and observed frequency:

- `0x03E`, bus 0, 48 bytes, 100 Hz
- `0x1D3`, bus 0, 8 bytes, 100 Hz
- `0x316`, bus 0, 8 bytes, 50 Hz
- `0x394`, bus 0, 8 bytes, 50 Hz
- `0x3A2`, bus 2, 8 bytes, 50 Hz
- `0x3A5`, bus 2, 8 bytes, 50 Hz

Add `0x29A` and `0x4ED` when their independent safety value is confirmed. Every RX entry must have a reason; no empty list, wildcard acceptance, blanket counter bypass, or blanket checksum bypass is permitted.

Counter and checksum hooks use algorithms verified against captured rlog frames. A check may be ignored only when the message demonstrably lacks that field, with a focused test documenting the decision.

### Authorization

- Unknown ACC state and state `0` are not treated as available.
- Controls engage only on verified ACC available/active transitions.
- Brake, gas, stock AEB, critical RX timeout, invalid checksum/counter, impossible speed/angle, or verified driver override revoke the relevant authorization.
- MADS lateral authorization is handled explicitly through target `controls_allowed_lateral` semantics.
- Driver torque threshold and sign must be validated from rlog before Panda override enforcement is enabled.

### TX Allowlist

Lateral mode permits only:

- `0x345`, bus 0, DLC 8: LKAS command
- `0x360`, bus 2, DLC 6: permitted cruise button command

Longitudinal mode additionally permits:

- `0x3A2`, bus 0, DLC 8: ACC command

All wrong-bus and wrong-DLC variants are rejected. Resume/set button commands are rejected when controls are unavailable; cancel remains permitted.

### Forwarding

Forward bus 0 to bus 2 and bus 2 to bus 0, with explicit collision prevention:

- Always block stock camera `0x345` toward main because the controller supplies the active or inactive replacement.
- Block stock camera `0x3A2` toward main only when alpha longitudinal is active.
- Forward stock `0x3A2` unchanged when alpha longitudinal is off.

### Lateral Limits

- Use the same verified wheelbase, steer ratio, slip factor, frequency, and conversion scale in Python and Panda.
- Apply target-native VM acceleration and jerk checks.
- Apply an explicit hard active steering-angle bound in addition to VM checks.
- Initial hard cap is `+/-150 deg`, matching the conservative source documentation, until physical rack/EPS range is measured and reviewed.
- Inactive steering commands must track measured steering angle within target safety tolerances.

### Longitudinal Limits

- Derive raw command conversion from the DBC and captured traffic; do not label raw `-511..511` values as m/s^2.
- Define raw minimum, maximum, and inactive command values corresponding to approved acceleration limits.
- Reject AEB requests from openpilot.
- Enforce inactive command whenever long control is unavailable, disabled, overridden, or disengaged.
- Parse the longitudinal safety flag identically in debug and release builds.

## Failure Handling

All uncertain or invalid states fail closed:

- Missing critical CAN: no actuation or immediate disengagement.
- Invalid counter/checksum: reject RX, revoke authorization according to safety core behavior.
- Unsupported bus layout: no-output configuration or platform rejection.
- Unknown ACC/AEB state: longitudinal unavailable.
- Invalid requested angle or acceleration: reject TX; never clamp silently inside Panda.
- Controller receives invalid actuator input: emit safe inactive command and record a rate-limited diagnostic.

No permissive fallback, forced `controls_allowed`, ignored safety mode, or debug-only production behavior is allowed.

Known missing signals do not block migration of already-working behavior unless they are required by the active safety contract. Door, seatbelt, and stock FCW remain documented gaps until a dedicated capture identifies them. Any safety-critical missing signal uses a fail-closed fallback and blocks only the dependent capability.

## Testing

### Golden Vectors

Extract deterministic vectors from the supplied rlog for:

- Motorola signed steering angle, driver torque, LKAS command, and ACC command fields.
- Wheel speed and brake/gas/AEB signals.
- Counters and checksums.
- Active, inactive, stop, resume, and driver-override states where present.

Tests must prove bit-exact extraction and packing, including checksum corruption and counter discontinuity.

### Car Tests

- Platform and dynamic interface discovery.
- CAN and firmware fingerprint matching.
- DBC availability and validation.
- `CarParams` for lateral-only and alpha-long configurations.
- Parser bus, DLC, frequency, and validity behavior.
- State transitions for speed, steering, pedals, cruise, buttons, BSM, and stock AEB.
- Controller active/inactive output and counter progression.
- Bus-offset variants for multi-Panda layouts.

### Panda Safety Tests

Add `opendbc/safety/tests/test_chery.py` covering:

- Safety model registration in debug and release builds.
- TX whitelist, wrong bus, wrong DLC, relay malfunction, and forwarding blacklist.
- RX liveness, timeout, checksum, counter, and malformed-frame behavior.
- ACC engagement and disengagement edge cases.
- Brake, gas, AEB, and driver override.
- Inactive steering angle matching.
- Hard angle bound and VM acceleration/jerk limits across a speed sweep.
- Longitudinal flag off/on behavior, raw command boundaries, inactive command, stop/resume, and controls-disabled behavior.
- Button policy.
- Python VehicleModel and Panda VM boundary consistency.

### Repository Verification

Run:

- Chery-specific car and safety tests.
- Generic opendbc car-interface tests.
- Complete opendbc safety suite.
- Safety release build tests.
- DBC validation.
- Ruff and project lint checks for touched files.
- Parent sunnypilot SCons build.
- Replay of the supplied Omoda E5 route with stable parser validity and no safety mismatch.

Replay proves protocol interpretation and software integration, not physical actuation.

## Rollout Gates

1. Compile, static checks, unit tests, and route replay.
2. C3 and Panda bench test with vehicle stationary and actuation physically constrained.
3. Parking-lot lateral validation at low speed.
4. Controlled-road lateral validation.
5. Alpha-long closed-course validation, initially disabled for normal users.

No public or normal-road enablement occurs while any of these remain unresolved:

- Physical steering rack/EPS angle range.
- Verified steer ratio and driver-torque sign/threshold.
- Longitudinal raw-command-to-acceleration mapping.
- Stock AEB interaction.
- Counter/checksum algorithms for safety-critical frames.

## Git Boundaries

- Make opendbc implementation commits inside `opendbc_repo`.
- Update parent sunnypilot submodule pointer in a separate integration commit.
- Do not include unrelated local Cabana changes in Chery commits.
- Do not rewrite source history or copy source commits wholesale; preserve provenance in commit messages and this design document.

## Acceptance Criteria

- Omoda E5 matches the supplied route by CAN and firmware fingerprint.
- All required parsers remain valid during replay.
- Lateral and alpha-long TX are impossible without their corresponding safety authorization.
- Debug and release Panda safety behavior match.
- No safety-relevant RX validation is empty or blanket-disabled.
- Every permitted TX message has explicit bus, DLC, state, and numerical-limit tests.
- Full safety suite and release build pass.
- Alpha long is available but default OFF.
- Hardware rollout follows the documented gates; replay success is not represented as road-safety validation.
