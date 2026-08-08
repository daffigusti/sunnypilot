# Chery Omoda E5 Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add target-native Chery Omoda E5 lateral angle control and default-off alpha longitudinal support with complete Panda safety enforcement.

**Architecture:** Keep all vehicle protocol, interface, controller, DBC, and safety logic inside the `opendbc_repo` submodule, matching sunnypilot's current car-port architecture. Preserve khadafi behavior that already works on the vehicle, adapting only where current target APIs or measurable safety requirements demand it. Use the supplied Omoda E5 rlog to verify changes, never guess missing signals, and update the parent repository only after the submodule passes focused and generic verification.

**Tech Stack:** Python 3.12, Cap'n Proto, opendbc CAN parser/packer, C Panda safety hooks, pytest/unittest, SCons, Ruff, sunnypilot replay tools.

---

## File Structure

Create these focused Chery units in `opendbc_repo`:

- `opendbc/car/chery/__init__.py`: brand package marker.
- `opendbc/car/chery/values.py`: platform metadata, vehicle parameters, actuation constants, flags, DBC map, firmware query configuration.
- `opendbc/car/chery/fingerprints.py`: Omoda E5 CAN and firmware fingerprints.
- `opendbc/car/chery/interface.py`: `CarParams` construction and alpha-long safety configuration.
- `opendbc/car/chery/carstate.py`: CAN parser definitions and `CarState` decoding.
- `opendbc/car/chery/cherycan.py`: bit-exact LKAS, ACC, and button frame construction.
- `opendbc/car/chery/carcontroller.py`: target-native VM lateral limiting and optional longitudinal command scheduling.
- `opendbc/car/chery/tests/fixtures.py`: reviewed raw rlog frames used as protocol golden vectors.
- `opendbc/car/chery/tests/test_chery.py`: platform, interface, parser, and state tests.
- `opendbc/car/chery/tests/test_cherycan.py`: bit-exact packer/checksum/counter tests.
- `opendbc/car/chery/KNOWN_GAPS.md`: explicit missing-signal and hardware-validation backlog.
- `opendbc/dbc/chery_canfd.dbc`: CAN signal definitions.
- `opendbc/safety/modes/chery.h`: RX validation, authorization, TX limits, and forwarding policy.
- `opendbc/safety/tests/test_chery.py`: libsafety behavior and release-parity tests.

Modify target registries without changing unrelated brands:

- `opendbc/car/car.capnp`
- `opendbc/car/values.py`
- `opendbc/car/fingerprints.py`
- `opendbc/safety/declarations.h`
- `opendbc/safety/safety.h`

Parent integration files:

- `opendbc_repo`: submodule pointer.
- `docs/superpowers/specs/2026-08-09-chery-omoda-e5-migration-design.md`: already committed design source of truth.
- `docs/CARS.md` and sunnypilot car catalog files only when the existing generator changes them.

## Execution Rules

- Run every Python/test command from `/Users/macbook/Code/openpilot/sunnypilot` with `.venv` active.
- Set the owner-provided private route in exported `CHERY_TEST_ROUTE` for Tasks 1 and 11. Enter it through `read -r CHERY_TEST_ROUTE` followed by `export CHERY_TEST_ROUTE` so the identifier is not committed or stored in shell history.
- Commit Tasks 1-11 inside `opendbc_repo`; commit Task 12 in parent sunnypilot.
- Never stage `openpilot/tools/cabana/signalview.h` in a Chery commit.
- Keep alpha longitudinal default OFF throughout development.
- Preserve source behavior unless a target API incompatibility, failing evidence-based test, or safety violation requires a change.
- If a signal cannot be identified from the DBC and supplied rlog, do not infer it. Add its required behavior and available CAN evidence to `KNOWN_GAPS.md`, use a safe fallback, and ask the user before further reverse engineering.
- Do not road-test from this plan. Hardware rollout remains a separate gated activity from the design spec.

### Task 1: Freeze Protocol Evidence

**Files:**
- Create: `opendbc_repo/opendbc/car/chery/tests/__init__.py`
- Create: `opendbc_repo/opendbc/car/chery/tests/fixtures.py`
- Create: `opendbc_repo/opendbc/car/chery/tests/test_route_evidence.py`

- [ ] **Step 1: Extract original-bus frames from the supplied full rlog**

Run:

```bash
source .venv/bin/activate
test -n "$CHERY_TEST_ROUTE"
python -c 'exec("""import os\nfrom collections import defaultdict\nfrom openpilot.tools.lib.logreader import LogReader, ReadMode\nids = {0x03E, 0x1D3, 0x29A, 0x316, 0x345, 0x360, 0x394, 0x3A2, 0x3A5, 0x4ED}\nseen = defaultdict(list)\nfor event in LogReader(os.environ[\"CHERY_TEST_ROUTE\"], ReadMode.RLOG):\n  if event.which() == \"can\":\n    for frame in event.can:\n      key = (frame.address, frame.src, len(frame.dat))\n      if frame.address in ids and frame.src in (0, 2) and len(seen[key]) < 8:\n        seen[key].append(bytes(frame.dat).hex())\nfor key, frames in sorted(seen.items()):\n  print(f\"0x{key[0]:03X} bus={key[1]} dlc={key[2]} frames={frames}\")\n""")'
```

Expected: original-bus samples for all ten addresses; bus 0 vehicle-state messages and bus 2 stock ADAS messages have their observed DLCs.

- [ ] **Step 2: Add immutable fixture constants**

Create `fixtures.py` with route identity, observed frequencies, and reviewed frame bytes copied from Step 1:

```python
ROUTE_SOURCE = "private owner full rlog, segment 0"
VIN_WMI = "MF7"
ENGINE_FW = b"00.02.12"

RX_LAYOUT = {
  0x03E: (0, 48, 100),
  0x1D3: (0, 8, 100),
  0x29A: (0, 8, 50),
  0x316: (0, 8, 50),
  0x360: (0, 6, 20),
  0x394: (0, 8, 50),
  0x4ED: (0, 8, 10),
  0x345: (2, 8, 50),
  0x3A2: (2, 8, 50),
  0x3A5: (2, 8, 50),
}

GOLDEN_FRAMES: dict[tuple[int, int], tuple[bytes, ...]] = {
  (0x03E, 0): (bytes.fromhex("bc0680007ccd80006c0680647869741d0d067cf215040000ab0644d000002000ab067fd9400000000000000000000000"),
                 bytes.fromhex("680780007cc880003b0780647865741d7c077cee15040000f60744d000002000f6077fd9400000000000000000000000")),
  (0x1D3, 0): (bytes.fromhex("78c0010000000523"), bytes.fromhex("78c000000000066e")),
  (0x29A, 0): (bytes.fromhex("000002c000800e10"), bytes.fromhex("000002c400800f43")),
  (0x316, 0): (bytes.fromhex("057e057e8c115efe"), bytes.fromhex("059105988c115fd0")),
  (0x345, 2): (bytes.fromhex("78c4000cbefe2f27"), bytes.fromhex("78c800000000c01f")),
  (0x360, 0): (bytes.fromhex("338000000000"), bytes.fromhex("dd9000000000")),
  (0x394, 0): (bytes.fromhex("17b000000800038a"), bytes.fromhex("14b000000800043e")),
  (0x3A2, 2): (bytes.fromhex("7d1102027f710f57"), bytes.fromhex("7d1102027f7100ec")),
  (0x3A5, 2): (bytes.fromhex("0000000000000fb1"), bytes.fromhex("000000000000000a")),
  (0x4ED, 0): (bytes.fromhex("1c0a1e0060040562"), bytes.fromhex("1c0a1e0060040645")),
}
```

The fixture values above came from the supplied full rlog. Re-run Step 1 and require exact equality before committing.

- [ ] **Step 3: Write evidence consistency tests**

```python
from opendbc.car.chery.tests.fixtures import ENGINE_FW, GOLDEN_FRAMES, RX_LAYOUT, VIN_WMI


def test_route_identity():
  assert VIN_WMI == "MF7"
  assert ENGINE_FW == b"00.02.12"


def test_golden_frames_cover_safety_layout():
  assert {addr for addr, _bus in GOLDEN_FRAMES} == set(RX_LAYOUT)
  for (addr, bus), frames in GOLDEN_FRAMES.items():
    expected_bus, expected_dlc, _frequency = RX_LAYOUT[addr]
    assert bus == expected_bus
    assert len(frames) >= 2
    assert all(len(frame) == expected_dlc for frame in frames)
```

- [ ] **Step 4: Run tests and confirm fixture completeness**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_route_evidence.py -v`

Expected: `2 passed`.

- [ ] **Step 5: Commit protocol evidence**

```bash
git -C opendbc_repo add opendbc/car/chery/tests
git -C opendbc_repo commit -m "test(chery): capture Omoda E5 CAN vectors"
```

### Task 2: Add DBC and Platform Registration

**Files:**
- Create: `opendbc_repo/opendbc/dbc/chery_canfd.dbc`
- Create: `opendbc_repo/opendbc/car/chery/__init__.py`
- Create: `opendbc_repo/opendbc/car/chery/values.py`
- Create: `opendbc_repo/opendbc/car/chery/fingerprints.py`
- Modify: `opendbc_repo/opendbc/car/car.capnp:614-651`
- Modify: `opendbc_repo/opendbc/car/values.py`
- Modify: `opendbc_repo/opendbc/car/fingerprints.py`
- Test: `opendbc_repo/opendbc/car/chery/tests/test_chery.py`

- [ ] **Step 1: Write failing platform registration test**

```python
from opendbc.car import Bus, structs
from opendbc.car.chery.fingerprints import FW_VERSIONS, FINGERPRINTS
from opendbc.car.chery.values import CAR, DBC
from opendbc.car.values import PLATFORMS


def test_chery_platform_registered():
  assert CAR.CHERY_OMODA_E5 in PLATFORMS
  assert DBC[CAR.CHERY_OMODA_E5][Bus.pt] == "chery_canfd"
  assert structs.CarParams.SafetyModel.cheryCanFd.raw == 35
  assert CAR.CHERY_OMODA_E5 in FINGERPRINTS
  assert CAR.CHERY_OMODA_E5 in FW_VERSIONS
```

- [ ] **Step 2: Run test to verify RED**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py::test_chery_platform_registered -v`

Expected: FAIL with missing `opendbc.car.chery.values` or missing enum member.

- [ ] **Step 3: Copy DBC protocol source and validate syntax**

Run:

```bash
cp ../khadafi-pilot/opendbc_repo/opendbc/dbc/chery_canfd.dbc opendbc_repo/opendbc/dbc/chery_canfd.dbc
```

Run: `pytest opendbc_repo/opendbc/can/tests/test_dbc_parser.py -v`

Expected: all DBC parser cases, including `chery_canfd`, PASS.

- [ ] **Step 4: Implement target-native platform values**

Use current `PlatformConfig`, `Platforms`, `CarSpecs`, and `AngleSteeringLimitsVM` APIs:

```python
from dataclasses import dataclass, field
from enum import IntFlag

from opendbc.car import Bus, CarSpecs, DbcDict, PlatformConfig, Platforms
from opendbc.car.docs_definitions import CarDocs, CarHarness, CarParts
from opendbc.car.lateral import AngleSteeringLimitsVM


class CherySafetyFlags(IntFlag):
  LONG_CONTROL = 1


@dataclass
class CheryCarDocs(CarDocs):
  package: str = "All"
  car_parts: CarParts = field(default_factory=lambda: CarParts.common([CarHarness.custom]))


@dataclass
class CheryPlatformConfig(PlatformConfig):
  dbc_dict: DbcDict = field(default_factory=lambda: {Bus.pt: "chery_canfd"})


class CAR(Platforms):
  CHERY_OMODA_E5 = CheryPlatformConfig(
    [CheryCarDocs("Chery Omoda E5", video="https://youtu.be/9kGGh8sLcHc")],
    CarSpecs(mass=1785., wheelbase=2.63, steerRatio=17.5),
  )


class CarControllerParams:
  STEER_STEP = 2
  ACC_CONTROL_STEP = 2
  BUTTONS_STEP = 5
  ANGLE_LIMITS = AngleSteeringLimitsVM(STEER_ANGLE_MAX=150., MAX_ANGLE_RATE=5.)
  ACCEL_MIN = -3.5
  ACCEL_MAX = 2.0
  RAW_ACCEL_MIN = -511
  RAW_ACCEL_MAX = 511
  RAW_ACCEL_INACTIVE = -24

  def __init__(self, CP):
    pass


DBC = CAR.create_dbc_map()
```

Add the source fingerprint and firmware values exactly, but remove unused `CANFD` flags and duplicate static `CanBus`.

- [ ] **Step 5: Register platform and safety enum**

Add `cheryCanFd @35;` after `volkswagenMeb @34;` in `car.capnp`. Import Chery `CAR` in global `car/values.py` and include it in the `Platform` union. Import Chery `CAR`, `FINGERPRINTS`, and `FW_VERSIONS` through the same dynamic aggregation pattern used by existing brands; do not add special-case matching logic.

- [ ] **Step 6: Run focused and generic platform tests**

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py::test_chery_platform_registered -v
pytest opendbc_repo/opendbc/car/tests/test_platform_configs.py opendbc_repo/opendbc/car/tests/test_can_fingerprint.py opendbc_repo/opendbc/car/tests/test_fw_fingerprint.py -v
```

Expected: all tests PASS.

- [ ] **Step 7: Commit platform registration**

```bash
git -C opendbc_repo add opendbc/dbc/chery_canfd.dbc opendbc/car/chery opendbc/car/car.capnp opendbc/car/values.py opendbc/car/fingerprints.py
git -C opendbc_repo commit -m "feat(chery): register Omoda E5 platform"
```

### Task 3: Implement Interface and Bus Mapping

**Files:**
- Create: `opendbc_repo/opendbc/car/chery/interface.py`
- Create: `opendbc_repo/opendbc/car/chery/cherycan.py`
- Modify: `opendbc_repo/opendbc/car/chery/tests/test_chery.py`

- [ ] **Step 1: Write failing parameter and bus-offset tests**

```python
from opendbc.car import structs
from opendbc.car.chery.cherycan import CanBus
from opendbc.car.chery.interface import CarInterface
from opendbc.car.chery.values import CAR, CherySafetyFlags


def fingerprint():
  return {bus: {} for bus in range(8)}


def test_interface_lateral_and_alpha_long():
  lateral = CarInterface.get_params(CAR.CHERY_OMODA_E5, fingerprint(), [], alpha_long=False, is_release=False, docs=False)
  assert lateral.brand == "chery"
  assert lateral.steerControlType == structs.CarParams.SteerControlType.angle
  assert lateral.transmissionType == structs.CarParams.TransmissionType.direct
  assert lateral.alphaLongitudinalAvailable
  assert not lateral.openpilotLongitudinalControl
  assert lateral.safetyConfigs[-1].safetyParam == 0

  long = CarInterface.get_params(CAR.CHERY_OMODA_E5, fingerprint(), [], alpha_long=True, is_release=False, docs=False)
  assert long.openpilotLongitudinalControl
  assert long.safetyConfigs[-1].safetyParam & CherySafetyFlags.LONG_CONTROL


def test_can_bus_offsets():
  assert (CanBus(fingerprint=fingerprint()).main, CanBus(fingerprint=fingerprint()).camera) == (0, 2)
```

- [ ] **Step 2: Run test to verify RED**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py -v`

Expected: FAIL because `CarInterface` and `CanBus` do not exist.

- [ ] **Step 3: Implement `CanBus` and interface**

```python
from opendbc.car import CanBusBase


class CanBus(CanBusBase):
  @property
  def main(self) -> int:
    return self.offset

  @property
  def radar(self) -> int:
    return self.offset + 1

  @property
  def camera(self) -> int:
    return self.offset + 2

  @property
  def loopback(self) -> int:
    return 128
```

Implement `_get_params` with:

```python
ret.brand = "chery"
ret.safetyConfigs = [get_safety_config(structs.CarParams.SafetyModel.cheryCanFd)]
ret.radarUnavailable = True
ret.alphaLongitudinalAvailable = True
ret.openpilotLongitudinalControl = alpha_long
if alpha_long:
  ret.safetyConfigs[-1].safetyParam |= CherySafetyFlags.LONG_CONTROL
ret.steerControlType = structs.CarParams.SteerControlType.angle
ret.transmissionType = structs.CarParams.TransmissionType.direct
ret.steerActuatorDelay = 0.1
ret.steerLimitTimer = 1.0
ret.longitudinalActuatorDelay = 0.05
ret.stopAccel = CarControllerParams.ACCEL_MIN
ret.vEgoStarting = 0.1
ret.vEgoStopping = 0.1
ret.minEnableSpeed = -1.
ret.minSteerSpeed = -1.
ret.autoResumeSng = True
```

Keep `_get_params_sp` limited to verified BSM availability. Do not add no-op `init`/`deinit` methods.

- [ ] **Step 4: Run interface tests**

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py -v
MAX_EXAMPLES=2 pytest 'opendbc_repo/opendbc/car/tests/test_car_interfaces.py::TestCarInterfaces::test_car_interfaces_CHERY_OMODA_E5' -v
```

Expected: PASS.

- [ ] **Step 5: Commit interface**

```bash
git -C opendbc_repo add opendbc/car/chery
git -C opendbc_repo commit -m "feat(chery): configure Omoda E5 interface"
```

### Task 4: Implement Bit-Exact CAN Packing

**Files:**
- Modify: `opendbc_repo/opendbc/car/chery/cherycan.py`
- Create: `opendbc_repo/opendbc/car/chery/tests/test_cherycan.py`

- [ ] **Step 1: Write failing CRC and steering golden-vector tests**

Use `CANPacker("chery_canfd")`, fixture stock values, and exact expected bytes from `GOLDEN_FRAMES`:

```python
from opendbc.can import CANPacker
from opendbc.car.chery.cherycan import calculate_crc, create_steering_control


def test_crc_known_vector():
  payload = bytes.fromhex("00000000000000")
  assert calculate_crc(payload) == 0x0A


def test_inactive_steering_tracks_stock_frame():
  packer = CANPacker("chery_canfd")
  stock = {
    "CMD": -392,
    "NEW_SIGNAL_3": 0,
    "LKA_ACTIVE": 0,
    "SET_X0": 0,
    "NEW_SIGNAL_5": 0,
    "NEW_SIGNAL_6": 0,
    "NEW_SIGNAL_7": 0,
    "NEW_SIGNAL_1": 0,
  }
  _addr, dat, bus = create_steering_control(packer, 0, 0.0, False, stock)
  assert bus == 0
  assert len(dat) == 8
  assert dat[-1] == calculate_crc(dat[:-1])
```

Use the captured `0x345` fixtures to verify stock-field extraction and packed counter/checksum bytes. Preserve the source command semantics unless the golden-vector test disproves them.

- [ ] **Step 2: Run tests to verify RED**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_cherycan.py -v`

Expected: FAIL with missing CAN functions.

- [ ] **Step 3: Implement one CRC function and deterministic frame builders**

```python
STEER_ANGLE_OFFSET = -392
STEER_ANGLE_SCALE = 10
CRC_POLY = 0x1D
CRC_XOR = 0x0A


def calculate_crc(data: bytes) -> int:
  crc = 0
  for byte in data:
    crc ^= byte
    for _ in range(8):
      crc = ((crc << 1) ^ CRC_POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
  return crc ^ CRC_XOR
```

Implement `create_steering_control`, `create_button_control`, and `create_acc_control` so each builder:

- copies only explicitly named verified stock fields;
- sets current active/state/command/counter fields;
- packs once to calculate checksum;
- packs again with checksum;
- never returns a stale stock dictionary unchanged.

- [ ] **Step 4: Verify every packed message against golden vectors**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_cherycan.py -v`

Expected: CRC, counter progression, active/inactive LKAS, buttons, and ACC vectors PASS.

- [ ] **Step 5: Commit CAN packing**

```bash
git -C opendbc_repo add opendbc/car/chery/cherycan.py opendbc/car/chery/tests/test_cherycan.py
git -C opendbc_repo commit -m "feat(chery): pack verified control frames"
```

### Task 5: Implement CarState Parsing

**Files:**
- Create: `opendbc_repo/opendbc/car/chery/carstate.py`
- Modify: `opendbc_repo/opendbc/car/chery/tests/test_chery.py`

- [ ] **Step 1: Write failing parser layout and state tests**

```python
from opendbc.car import Bus, structs
from opendbc.car.chery.carstate import CarState
from opendbc.car.chery.interface import CarInterface
from opendbc.car.chery.values import CAR


def test_parser_layout_matches_route():
  cp = CarInterface.get_non_essential_params(CAR.CHERY_OMODA_E5)
  parsers = CarState.get_can_parsers(cp, structs.CarParamsSP())
  assert set(parsers) == {Bus.pt, Bus.cam, Bus.loopback}
  assert parsers[Bus.pt].bus == 0
  assert parsers[Bus.cam].bus == 2
  assert parsers[Bus.loopback].bus == 128
```

Add state assertions using `CANPacker` messages for wheel speeds, signed steering angle, signed driver torque, brake, gas, cruise, buttons, BSM, and AEB.

- [ ] **Step 2: Run tests to verify RED**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py -v`

Expected: FAIL with missing `CarState`.

- [ ] **Step 3: Implement parsers and state update**

Use route frequencies exactly:

```python
pt_messages = [
  ("STEER_ANGLE_SENSOR", 100),
  ("WHEEL_SPEED_FRNT", 50),
  ("WHEEL_SPEED_REAR", 50),
  ("BRAKE_DATA", 50),
  ("ENGINE_DATA", 100),
  ("STEER_SENSOR_2", 50),
  ("STEER_BUTTON", 20),
]
cam_messages = [
  ("ACC_CMD", 50),
  ("ACC", 50),
  ("LKAS_CAM_CMD_345", 50),
  ("LKAS_STATE", 20),
  ("SETTING", 20),
  ("LEAD_FRONT", 20),
]
loopback_messages = [("LKAS_CAM_CMD_345", 0), ("ACC_CMD", 0)]
```

Decode signed DBC values directly. Assign cruise state before EPS-fault logic. Preserve verified `stockAeb`; do not overwrite it. Keep the source behavior for confirmed signals. For door, seatbelt, and FCW, use conservative unavailable behavior and record the missing message/signal evidence in `KNOWN_GAPS.md`; do not fabricate a healthy value or reverse engineer without user confirmation.

- [ ] **Step 4: Run state and DBC tests**

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py -v
pytest opendbc_repo/opendbc/can/tests/test_dbc_parser.py -v
```

Expected: PASS.

- [ ] **Step 5: Commit state parsing**

```bash
git -C opendbc_repo add opendbc/car/chery/carstate.py opendbc/car/chery/tests/test_chery.py
git -C opendbc_repo commit -m "feat(chery): parse Omoda E5 vehicle state"
```

### Task 6: Implement Target-Native Lateral Controller

**Files:**
- Create: `opendbc_repo/opendbc/car/chery/carcontroller.py`
- Modify: `opendbc_repo/opendbc/car/chery/tests/test_chery.py`

- [ ] **Step 1: Write failing lateral scheduling and limiting tests**

```python
from types import SimpleNamespace

from opendbc.car import structs
from opendbc.car.chery.carcontroller import CarController
from opendbc.car.chery.interface import CarInterface
from opendbc.car.chery.values import CAR, DBC


def make_control(lat_active: bool, angle: float):
  control = structs.CarControl()
  control.latActive = lat_active
  control.actuators.steeringAngleDeg = angle
  return control.as_reader()


def make_state(measured_angle: float, speed: float = 1.0):
  state = structs.CarState()
  state.vEgo = speed
  state.vEgoRaw = speed
  state.steeringAngleDeg = measured_angle
  state.steeringTorque = 0.0
  return SimpleNamespace(
    out=state.as_reader(),
    lkas_cmd={
      "NEW_SIGNAL_5": 0,
      "NEW_SIGNAL_6": 0,
      "NEW_SIGNAL_7": 0,
      "NEW_SIGNAL_1": 0,
    },
    acc_cmd={},
    buttons_stock_values={},
  )


def make_controller():
  cp = CarInterface.get_non_essential_params(CAR.CHERY_OMODA_E5)
  cp_sp = structs.CarParamsSP()
  return CarController(DBC[CAR.CHERY_OMODA_E5], cp, cp_sp)


def test_lateral_controller_sends_50_hz():
  controller = make_controller()
  control = make_control(True, 5.0)
  state = make_state(0.0, 10.0)
  steer_messages = 0
  for frame in range(100):
    _actuators, sends = controller.update(control, structs.CarControlSP(), state, frame * 10_000_000)
    steer_messages += sum(addr == 0x345 for addr, _dat, _bus in sends)
  assert steer_messages == 50


def test_lateral_inactive_tracks_measured_angle():
  controller = make_controller()
  measured_angle = 17.5
  actuators, _sends = controller.update(make_control(False, 80.0), structs.CarControlSP(), make_state(measured_angle), 0)
  assert actuators.steeringAngleDeg == measured_angle


def test_lateral_hard_cap_is_150_degrees():
  controller = make_controller()
  controller.apply_angle_last = 149.0
  actuators, _sends = controller.update(make_control(True, 500.0), structs.CarControlSP(), make_state(149.0), 0)
  assert abs(actuators.steeringAngleDeg) <= 150.
```

- [ ] **Step 2: Run tests to verify RED**

Run: `pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py -k lateral -v`

Expected: FAIL with missing controller behavior.

- [ ] **Step 3: Implement minimal lateral update**

Use current shared limiter:

```python
self.apply_angle_last = apply_steer_angle_limits_vm(
  actuators.steeringAngleDeg,
  self.apply_angle_last,
  CS.out.vEgoRaw,
  CS.out.steeringAngleDeg,
  lat_active,
  CarControllerParams,
  self.VM,
)
```

Send `0x345` every `STEER_STEP=2`, calculate every checksum/counter through `cherycan`, and copy measured angle when inactive. Do not add custom smoothing, duplicate VM instances, broad exception catches, or live prints.

- [ ] **Step 4: Run lateral and shared lateral-limit tests**

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests/test_chery.py -k lateral -v
pytest opendbc_repo/opendbc/car/tests/test_lateral_limits.py -v
```

Expected: PASS.

- [ ] **Step 5: Commit lateral controller**

```bash
git -C opendbc_repo add opendbc/car/chery/carcontroller.py opendbc/car/chery/tests/test_chery.py
git -C opendbc_repo commit -m "feat(chery): add VM-limited lateral control"
```

### Task 7: Register Chery Panda Safety and RX Liveness

**Files:**
- Create: `opendbc_repo/opendbc/safety/modes/chery.h`
- Create: `opendbc_repo/opendbc/safety/tests/test_chery.py`
- Modify: `opendbc_repo/opendbc/safety/declarations.h:7-37,340-370`
- Modify: `opendbc_repo/opendbc/safety/safety.h:9-32,400-430`

- [ ] **Step 1: Write failing safety registration and RX-layout tests**

```python
from opendbc.car.structs import CarParams
from opendbc.safety.tests.libsafety import libsafety_py
from opendbc.safety.tests.common import CANPackerSafety


class TestCherySafetyRegistration:
  def setup_method(self):
    self.safety = libsafety_py.libsafety
    self.packer = CANPackerSafety("chery_canfd")

  def test_mode_35_registered(self):
    assert self.safety.set_safety_hooks(CarParams.SafetyModel.cheryCanFd, 0) == 0
```

Add RX tests for exact bus/DLC and missing-message invalidation for `0x03E`, `0x1D3`, `0x316`, `0x394`, `0x3A2`, and `0x3A5`.

- [ ] **Step 2: Run tests to verify RED**

Run: `pytest opendbc_repo/opendbc/safety/tests/test_chery.py -v`

Expected: FAIL because Chery hooks are not registered.

- [ ] **Step 3: Register safety model and implement non-empty RX config**

Add:

```c
#define SAFETY_CHERY 35U
extern const safety_hooks chery_hooks;
```

Add the Chery include and registry entry. Define named RX checks:

```c
static RxCheck chery_rx_checks[] = {
  {.msg = {{0x03E, 0, 48, 100U, .ignore_quality_flag = true}, { 0 }, { 0 }}},
  {.msg = {{0x1D3, 0, 8, 100U, .ignore_quality_flag = true}, { 0 }, { 0 }}},
  {.msg = {{0x316, 0, 8, 50U, .ignore_quality_flag = true}, { 0 }, { 0 }}},
  {.msg = {{0x394, 0, 8, 50U, .ignore_quality_flag = true}, { 0 }, { 0 }}},
  {.msg = {{0x3A2, 2, 8, 50U, .ignore_quality_flag = true}, { 0 }, { 0 }}},
  {.msg = {{0x3A5, 2, 8, 50U, .ignore_quality_flag = true}, { 0 }, { 0 }}},
};
```

Set `ignore_checksum` and `ignore_counter` per message only after Task 1 golden-vector analysis proves the field absent. Do not blanket-ignore them.

- [ ] **Step 4: Run Chery safety and release-build tests**

Run:

```bash
pytest opendbc_repo/opendbc/safety/tests/test_chery.py -v
pytest opendbc_repo/opendbc/safety/tests/test_release_build.py -v
```

Expected: mode registration and non-empty RX config PASS in development and release builds.

- [ ] **Step 5: Commit safety registration**

```bash
git -C opendbc_repo add opendbc/safety
git -C opendbc_repo commit -m "feat(chery): register safety RX checks"
```

### Task 8: Enforce RX Integrity and Authorization

**Files:**
- Modify: `opendbc_repo/opendbc/safety/modes/chery.h`
- Modify: `opendbc_repo/opendbc/safety/tests/test_chery.py`

- [ ] **Step 1: Write failing checksum, counter, and disengagement tests**

Add tests proving:

- each protected frame accepts golden checksum/counter sequences;
- one-bit checksum corruption rejects RX;
- repeated/skipped counters follow current safety tolerance, then invalidate;
- ACC unavailable/unknown does not engage;
- verified available+active rising edge engages;
- ACC off, brake, gas, stock AEB, RX timeout, and driver override disengage.

Example boundary test:

```python
def test_unknown_acc_state_never_engages(self):
  self.safety.set_controls_allowed(False)
  self._rx(self._acc_cmd_msg(acc_state=0, gas_pressed=False))
  self._rx(self._acc_status_msg(active=True, stock_aeb=False))
  assert not self.safety.get_controls_allowed()
```

- [ ] **Step 2: Run tests to verify RED**

Run: `pytest opendbc_repo/opendbc/safety/tests/test_chery.py -k 'checksum or counter or disengage or acc_state' -v`

Expected: FAIL because hooks do not yet compute integrity or authorization.

- [ ] **Step 3: Implement Motorola extraction and integrity callbacks**

Implement `chery_get_counter`, `chery_get_checksum`, `chery_compute_checksum`, and `chery_get_quality_flag_valid` only for fields verified by the DBC and golden frames. Implement RX extraction with unsigned shifts and explicit sign extension for:

- wheel speed `0x316`;
- angle `0x1D3` in `deg * 100`;
- driver torque `0x394`;
- brake `0x03E`;
- ACC main/gas `0x3A2`;
- ACC active/AEB `0x3A5`.

Authorize only `ACC_STATE in {2, 3}` combined with verified active state. Call `pcm_cruise_check` once with the combined boolean to avoid arrival-order races.

- [ ] **Step 4: Run RX safety tests**

Run: `pytest opendbc_repo/opendbc/safety/tests/test_chery.py -k 'rx or checksum or counter or disengage or acc_state' -v`

Expected: PASS.

- [ ] **Step 5: Commit RX safety**

```bash
git -C opendbc_repo add opendbc/safety/modes/chery.h opendbc/safety/tests/test_chery.py
git -C opendbc_repo commit -m "feat(chery): enforce CAN RX integrity"
```

### Task 9: Enforce Lateral TX and Forwarding Safety

**Files:**
- Modify: `opendbc_repo/opendbc/safety/modes/chery.h`
- Modify: `opendbc_repo/opendbc/safety/tests/test_chery.py`

- [ ] **Step 1: Write failing angle, whitelist, button, and forwarding tests**

Derive the safety class from `common.CarSafetyTest`, `common.AngleSteeringSafetyTest`, and `common.VehicleSpeedSafetyTest`. Configure:

```python
TX_MSGS = [[0x345, 0], [0x360, 2]]
RELAY_MALFUNCTION_ADDRS = {0: (0x345,), 2: (0x360,)}
FWD_BLACKLISTED_ADDRS = {2: [0x345]}
STEER_ANGLE_MAX = 150
DEG_TO_CAN = 100
LATERAL_FREQUENCY = 50
```

Add explicit tests for wrong DLC, active command over `+/-150 deg`, inactive command not matching measured angle, VM lateral acceleration/jerk at speeds `0, 1, 5, 10, 15, 30, 50 m/s`, resume/set while controls disallowed, and cancel while disallowed.

- [ ] **Step 2: Run tests to verify RED**

Run: `pytest opendbc_repo/opendbc/safety/tests/test_chery.py -k 'angle or tx or fwd or button' -v`

Expected: FAIL because TX and forwarding hooks are incomplete.

- [ ] **Step 3: Implement lateral TX limits and forwarding**

Use:

```c
static const AngleSteeringLimits CHERY_STEERING_LIMITS = {
  .max_angle = 15000,
  .angle_deg_to_can = 100,
  .frequency = 50U,
};

static const AngleSteeringParams CHERY_STEERING_PARAMS = {
  .slip_factor = -0.000503295541,
  .steer_ratio = 17.5,
  .wheelbase = 2.63,
};
```

Reject any active desired angle outside `+/-15000` before calling `steer_angle_cmd_checks_vm`. Validate LKAS active bit and button policy. Implement a forwarding hook that blocks bus-2 `0x345` and leaves stock `0x3A2` forwarding enabled while longitudinal mode is off.

- [ ] **Step 4: Run complete lateral safety tests**

Run: `pytest opendbc_repo/opendbc/safety/tests/test_chery.py -v`

Expected: all lateral, RX, whitelist, forwarding, and release-parity tests PASS.

- [ ] **Step 5: Commit lateral safety**

```bash
git -C opendbc_repo add opendbc/safety/modes/chery.h opendbc/safety/tests/test_chery.py
git -C opendbc_repo commit -m "feat(chery): enforce lateral safety limits"
```

### Task 10: Add Default-Off Alpha Longitudinal Control

**Files:**
- Modify: `opendbc_repo/opendbc/car/chery/values.py`
- Modify: `opendbc_repo/opendbc/car/chery/cherycan.py`
- Modify: `opendbc_repo/opendbc/car/chery/carcontroller.py`
- Modify: `opendbc_repo/opendbc/car/chery/tests/test_cherycan.py`
- Modify: `opendbc_repo/opendbc/car/chery/tests/test_chery.py`
- Modify: `opendbc_repo/opendbc/safety/modes/chery.h`
- Modify: `opendbc_repo/opendbc/safety/tests/test_chery.py`

- [ ] **Step 1: Write failing controller and safety tests**

Test all observable states:

- alpha long OFF sends no `0x3A2` and safety rejects `0x3A2` TX;
- alpha long ON sends exactly 50 Hz;
- inactive uses raw `-24`;
- active command conversion clamps Python acceleration to `[-3.5, 2.0]` before mapping;
- Panda accepts raw `-511`, `511`, and inactive `-24`, rejects outside bounds;
- Panda rejects any openpilot AEB bit;
- brake/gas/AEB/controls-off permits only inactive command;
- camera→main `0x3A2` forwarding is blocked only when LONG_CONTROL flag is set;
- release build parses LONG_CONTROL identically to development build.

```python
class TestCheryLongitudinalSafety(TestCherySafetyBase, common.LongitudinalAccelSafetyTest):
  LONGITUDINAL = True

  def setup_method(self):
    self.safety.set_safety_hooks(CarParams.SafetyModel.cheryCanFd, CherySafetyFlags.LONG_CONTROL)
    self.safety.init_tests()
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests -k longitudinal -v
pytest opendbc_repo/opendbc/safety/tests/test_chery.py -k longitudinal -v
```

Expected: FAIL because long controller and safety policy are incomplete.

- [ ] **Step 3: Implement controller longitudinal scheduling**

Map clamped acceleration through verified breakpoints:

```python
ACCEL_LOOKUP_BP = [ACCEL_MIN, 0., ACCEL_MAX]
ACCEL_LOOKUP_V = [RAW_ACCEL_MIN, RAW_ACCEL_INACTIVE, RAW_ACCEL_MAX]
raw_accel = int(round(np.interp(accel, ACCEL_LOOKUP_BP, ACCEL_LOOKUP_V)))
```

When `openpilotLongitudinalControl` is false, send no ACC frame. When true, send active or explicit inactive ACC at `ACC_CONTROL_STEP=2`; set openpilot AEB request to zero in every generated frame.

- [ ] **Step 4: Implement release-safe longitudinal Panda policy**

Parse `CherySafetyFlags.LONG_CONTROL` unconditionally in `chery_init`, select a separate TX allowlist that adds `{0x3A2, 0, 8}`, validate raw command bounds/inactive command, reject AEB bits, and make forwarding conditional on `chery_longitudinal`. Do not use `ALLOW_DEBUG` around safety-param behavior.

- [ ] **Step 5: Run longitudinal and release tests**

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests -k longitudinal -v
pytest opendbc_repo/opendbc/safety/tests/test_chery.py -k longitudinal -v
pytest opendbc_repo/opendbc/safety/tests/test_release_build.py -v
```

Expected: PASS.

- [ ] **Step 6: Commit alpha longitudinal support**

```bash
git -C opendbc_repo add opendbc/car/chery opendbc/safety
git -C opendbc_repo commit -m "feat(chery): add guarded alpha longitudinal"
```

### Task 11: Run Integration, Replay, and Documentation Gates

**Files:**
- Modify: `opendbc_repo/opendbc/car/chery/values.py` only if docs metadata tests require correction.
- Create: `opendbc_repo/opendbc/car/chery/README.md`
- Create: `opendbc_repo/opendbc/car/chery/KNOWN_GAPS.md`
- Modify generated car docs only through existing generators.

- [ ] **Step 1: Add factual port documentation**

Document only:

- supported platform and supplied validation route;
- lateral and default-off alpha-long status;
- verified bus/DLC/frequency table;
- provisional `+/-150 deg` cap;
- unresolved physical steer ratio/rack range and closed-course alpha-long validation gates;
- exact focused test commands.

Do not copy source claims such as “production-ready” or ISO compliance.

Create `KNOWN_GAPS.md` with this initial explicit backlog:

```markdown
# Chery Omoda E5 Known Gaps

## Signals requiring another capture/reverse-engineering session

- Door-open state: CAN address and signal not verified. Current port must not use this state as an engagement safety condition.
- Seatbelt-unlatched state: CAN address and signal not verified. Current port must not report a verified latched state.
- Stock FCW state: source forces false; address and signal require identification.
- Driver torque sign and Panda override threshold: DBC decode exists, but physical left/right and override threshold need stationary vehicle correlation.
- Physical steering rack/EPS range: provisional safety cap remains +/-150 degrees until measured.
- Raw ACC command to physical acceleration mapping: software bounds exist; closed-course measurement remains required before normal use.

For each new gap, record desired behavior, candidate address/bus/DLC, route evidence, and exact capture needed. Ask the vehicle owner before selecting an unverified signal.
```

- [ ] **Step 2: Run focused Chery suites**

Generate target-owned catalogs first:

```bash
python opendbc_repo/opendbc/car/docs.py
python opendbc_repo/opendbc/sunnypilot/car/platform_list.py
```

Expected: `opendbc_repo/docs/CARS.md` and `opendbc_repo/opendbc/sunnypilot/car/car_list.json` contain `CHERY_OMODA_E5`; only generator-produced differences remain.

Run:

```bash
pytest opendbc_repo/opendbc/car/chery/tests -v
pytest opendbc_repo/opendbc/safety/tests/test_chery.py -v
```

Expected: PASS with no skipped Chery safety behavior.

- [ ] **Step 3: Run generic opendbc suites**

Run:

```bash
MAX_EXAMPLES=3 pytest opendbc_repo/opendbc/car/tests/test_car_interfaces.py -v
pytest opendbc_repo/opendbc/car/tests/test_platform_configs.py opendbc_repo/opendbc/car/tests/test_can_fingerprint.py opendbc_repo/opendbc/car/tests/test_fw_fingerprint.py opendbc_repo/opendbc/car/tests/test_lateral_limits.py -v
pytest opendbc_repo/opendbc/safety/tests -v
```

Expected: PASS.

- [ ] **Step 4: Run lint and build**

Run:

```bash
ruff check opendbc_repo/opendbc/car/chery opendbc_repo/opendbc/safety/tests/test_chery.py
tools/op.sh build
```

Expected: Ruff exits 0 and SCons prints `done building targets`.

- [ ] **Step 5: Replay supplied route**

Run replay in terminal 1:

```bash
source .venv/bin/activate
test -n "$CHERY_TEST_ROUTE"
tools/op.sh replay "$CHERY_TEST_ROUTE" --no-loop --no-hw-decoder
```

Run UI in terminal 2:

```bash
source .venv/bin/activate
BIG=1 ./openpilot/selfdrive/ui/ui.py
```

Acceptance: fingerprint resolves to `CHERY_OMODA_E5`, parsers remain valid, no safety mismatch appears, lateral state is coherent, and alpha long remains OFF unless explicitly enabled.

- [ ] **Step 6: Commit documentation and final opendbc corrections**

```bash
git -C opendbc_repo add opendbc/car/chery docs/CARS.md opendbc/sunnypilot/car/car_list.json
git -C opendbc_repo commit -m "docs(chery): record validation evidence"
```

### Task 12: Integrate Opendbc into Parent Sunnypilot

**Files:**
- Modify: `opendbc_repo` submodule pointer.
- Modify generated car catalog/docs only if generator output changes.
- Preserve: `openpilot/tools/cabana/signalview.h` unrelated local modification.

- [ ] **Step 1: Inspect submodule history and parent diff**

Run:

```bash
git -C opendbc_repo status --short
git -C opendbc_repo log --oneline -12
git status --short
git diff --submodule=log -- opendbc_repo
```

Expected: opendbc worktree clean; parent shows only submodule pointer plus pre-existing Cabana modification.

- [ ] **Step 2: Run fresh parent verification**

Run:

```bash
source .venv/bin/activate
tools/op.sh check
tools/op.sh build
```

Expected: environment check passes and SCons prints `done building targets`.

- [ ] **Step 3: Stage only Chery integration files**

Run:

```bash
git add opendbc_repo
git status --short
git diff --cached --submodule=log
```

Expected: `opendbc_repo` staged; `openpilot/tools/cabana/signalview.h` remains unstaged.

- [ ] **Step 4: Commit parent integration**

```bash
git commit -m "feat(chery): integrate Omoda E5 support"
```

- [ ] **Step 5: Record remaining hardware gates**

Report these as unresolved, not as software failures:

- physical steering rack/EPS angle range measurement;
- steer ratio and driver-torque sign/threshold validation;
- closed-course raw acceleration mapping validation;
- stock AEB interaction under alpha longitudinal;
- staged C3/Panda bench and controlled-drive rollout.

Do not claim road readiness until all five gates have evidence.
