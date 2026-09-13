![](https://user-images.githubusercontent.com/47793918/233812617-beab2e71-57b9-479e-8bff-c3931347ca40.png)

## Chery Omoda E5 fork (`feature/chery-omoda-e5`)

This branch is a personal fork of sunnypilot that adds the **Chery Omoda E5 2024**. Everything below the next horizontal rule is the upstream sunnypilot README.

> **Warning:** This port is alpha. It has been driven on urban roads up to 32 kph, but several safety-relevant signals are still unverified on the vehicle (see [Known gaps](#known-gaps)). Drive with your hands ready to take over at all times.

### What works

| Feature | State |
| --- | --- |
| Lateral (angle control, `0x345`) | Enabled. Capped at ±150°. Rate limited by the vehicle model, and at most 100°/s below ~32 kph to keep engagement smooth. |
| Steer ratio | 17, fitted from yaw rate against wheel angle on a real route (16.8-17.2 across 11-32 kph). |
| Gas override | Lateral stays active while you press the accelerator (`DisengageOnAccelerator` off). |
| Driver steering override | `abs(TORQUE_DRIVER) > 70`, with 1 s hysteresis. |
| MADS | Partial, like Tesla and Rivian. Lateral engages with the ACC, stays on after an ACC cancel, and disengages on brake. |
| ICBM | Available. Taps `RES+`/`RES-` on `0x360` so the stock ACC set speed follows sunnypilot's target. |
| Longitudinal (`ACC_CMD` `0x3A2`) | Alpha and **off by default**. Requires the alpha-longitudinal toggle. |
| Stop and hold | Stock hold encoding (`CMD=400`). Panda allows it only while the car is already stopped. |
| Resume from hold | Taps `RES+` on `0x360` (camera bus) while stopped. |
| Stock AEB | Detected. The OEM `0x3A2` is passed through and openpilot stops sending ACC commands. |
| Blinkers, BSM, EPS watchdog, cluster LKA icon | Enabled. |
| Door open, seatbelt | Parsed from `BCM_SIGNAL_1` door bits and `NEW_MSG_430.SEATBELT`, confirmed on one route. |

### Not supported yet

Compared with a mature port such as Hyundai, these are still missing. Most need a labelled CAN capture before they can be added.

| Feature | Notes |
| --- | --- |
| Forward collision warning (`stockFcw`) | Not parsed. `HUD_ALERT` and `AEB_COMMAND` are candidates. |
| Parking brake, ESP off, ACC fault | Not parsed. `EPB` is in the DBC. |
| Steering rate | `steeringRateDeg` is always 0. |
| Sending cruise cancel | openpilot cannot cancel the stock ACC by itself. |
| Full MADS | Needs an LKAS toggle signal. None has been found. |
| Stock lead and radar | `LEAD_FRONT` is parsed but unused, so openpilot sees no radar lead. |
| Cluster lane lines and lane departure warning | Not sent. Only the LKA active state is. |
| Speed limit from the car | No signal identified. |
| Longitudinal tuning | The accel-to-command mapping is uncalibrated. |

### Where the code lives

The car port and panda safety live in the `opendbc_repo` submodule, which points at [daffigusti/opendbc](https://github.com/daffigusti/opendbc) (branch `feature/chery-omoda-e5`).

| Path | Contents |
| --- | --- |
| `opendbc_repo/opendbc/car/chery/` | `carcontroller.py`, `carstate.py`, `cherycan.py`, `interface.py`, `values.py`, `fingerprints.py` |
| `opendbc_repo/opendbc/sunnypilot/car/chery/icbm.py` | ICBM button taps |
| `opendbc_repo/opendbc/car/chery/README.md` | Safety-hook CAN layout and the remaining hardware gates |
| `opendbc_repo/opendbc/car/chery/KNOWN_GAPS.md` | Unverified signals and the evidence still needed |
| `opendbc_repo/opendbc/safety/modes/chery.h` | Panda safety mode |
| `opendbc_repo/opendbc/dbc/chery_canfd.dbc` | DBC |
| `openpilot/sunnypilot/mads/helpers.py` | Marks Chery as a MADS brand without a main button |

### Getting started

Clone with submodules:

```sh
git clone --recurse-submodules -b feature/chery-omoda-e5 https://github.com/daffigusti/sunnypilot.git
cd sunnypilot
tools/op.sh setup
```

If you already have a clone, run `git submodule update --init --recursive` after checking out the branch. This matters because `.gitmodules` points `opendbc` at the personal fork.

### Updating a comma device

Enable SSH in Settings → Developer, then:

```sh
ssh comma@<device-ip>
cd /data/openpilot
git fetch origin
git checkout feature/chery-omoda-e5
git reset --hard origin/feature/chery-omoda-e5
git submodule update --init --recursive
sudo reboot
```

Skipping `git submodule update` leaves the device on the old Chery code. `reset --hard` discards any local edits on the device. After the reboot the device rebuilds and reflashes the panda.

The comma installer URL (`installer.comma.ai/daffigusti/feature/chery-omoda-e5`) looks for a repository named `openpilot`, and this fork is named `sunnypilot`, so a fresh install that way may fail.

### Tests

```sh
cd opendbc_repo
source ./setup.sh
pytest opendbc/car/chery/tests -v
pytest opendbc/safety/tests/test_chery.py -v
./test.sh   # full opendbc lint and test suite
```

### Known gaps

[`KNOWN_GAPS.md`](opendbc_repo/opendbc/car/chery/KNOWN_GAPS.md) has the full list. The main ones:

- No driver gas pedal signal has been found. `gas_pressed` comes only from the camera's `ACC_CMD.GAS_PRESSED` bit.
- The sign of `TORQUE_DRIVER` and the override threshold of 70 have not been measured.
- Steer ratio 17 and actuator delay 0.15 s are measured only up to 32 kph. If you change `steerRatio`, change panda's `steer_ratio` to match.
- ICBM and resume taps are unconfirmed: kph per tap, auto-repeat on a held press, and whether the camera accepts spoofed presses while moving.
- The raw ACC command to real acceleration mapping, the full-stop hold, and the cluster's response to the substituted `LKAS_STATE` (`0x307`) are unconfirmed.
- Stock AEB interaction still needs hardware validation.

When you share logs, strip route IDs, VINs, locations, and timestamps first.

---

## 🌞 What is sunnypilot?
[sunnypilot](https://github.com/sunnyhaibin/sunnypilot) is a fork of comma.ai's openpilot, an open source driver assistance system. sunnypilot offers the user a unique driving experience for over 300+ supported car makes and models with modified behaviors of driving assist engagements. sunnypilot complies with comma.ai's safety rules as accurately as possible.

## 💭 Join our Community Forum
Join the official sunnypilot community forum to stay up to date with all the latest features and be a part of shaping the future of sunnypilot!
* https://community.sunnypilot.ai/

## Documentation
https://docs.sunnypilot.ai/ is your one stop shop for everything from features to installation to FAQ about the sunnypilot

## 🚘 Running on a dedicated device in a car
First, check out this list of items you'll need to [get started](https://community.sunnypilot.ai/t/getting-started-using-sunnypilot-in-your-supported-car/251).

## Installation
Next, refer to the sunnypilot community forum for [installation instructions](https://community.sunnypilot.ai/t/read-before-installing-sunnypilot/254), as well as a complete list of [Recommended Branch Installations](https://community.sunnypilot.ai/t/recommended-branch-installations/235).

## 🎆 Pull Requests
We welcome both pull requests and issues on GitHub. Bug fixes are encouraged.

Pull requests should be against the most current `master` branch.

## 📊 User Data

By default, sunnypilot uploads the driving data to comma servers. You can also access your data through [comma connect](https://connect.comma.ai/).

sunnypilot is open source software. The user is free to disable data collection if they wish to do so.

sunnypilot logs the road-facing camera, CAN, GPS, IMU, magnetometer, thermal sensors, crashes, and operating system logs.
The driver-facing camera and microphone are only logged if you explicitly opt-in in settings.

By using this software, you understand that use of this software or its related services will generate certain types of user data, which may be logged and stored at the sole discretion of comma. By accepting this agreement, you grant an irrevocable, perpetual, worldwide right to comma for the use of this data.

## Licensing

sunnypilot is released under the [MIT License](LICENSE). This repository includes original work as well as significant portions of code derived from [openpilot by comma.ai](https://github.com/commaai/openpilot), which is also released under the MIT license with additional disclaimers.

The original openpilot license notice, including comma.ai’s indemnification and alpha software disclaimer, is reproduced below as required:

> openpilot is released under the MIT license. Some parts of the software are released under other licenses as specified.
>
> Any user of this software shall indemnify and hold harmless Comma.ai, Inc. and its directors, officers, employees, agents, stockholders, affiliates, subcontractors and customers from and against all allegations, claims, actions, suits, demands, damages, liabilities, obligations, losses, settlements, judgments, costs and expenses (including without limitation attorneys’ fees and costs) which arise out of, relate to or result from any use of this software by user.
>
> **THIS IS ALPHA QUALITY SOFTWARE FOR RESEARCH PURPOSES ONLY. THIS IS NOT A PRODUCT.
> YOU ARE RESPONSIBLE FOR COMPLYING WITH LOCAL LAWS AND REGULATIONS.
> NO WARRANTY EXPRESSED OR IMPLIED.**

For full license terms, please see the [`LICENSE`](LICENSE) file.

## 💰 Support sunnypilot
If you find any of the features useful, consider becoming a [sponsor on GitHub](https://github.com/sponsors/sunnyhaibin) to support future feature development and improvements.


By becoming a sponsor, you will gain access to exclusive content, early access to new features, and the opportunity to directly influence the project's development.


<h3>GitHub Sponsor</h3>

<a href="https://github.com/sponsors/sunnyhaibin">
  <img src="https://user-images.githubusercontent.com/47793918/244135584-9800acbd-69fd-4b2b-bec9-e5fa2d85c817.png" alt="Become a Sponsor" width="300" style="max-width: 100%; height: auto;">
</a>
<br>

<h3>PayPal</h3>

<a href="https://paypal.me/sunnyhaibin0850" target="_blank">
<img src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" alt="PayPal this" title="PayPal - The safer, easier way to pay online!" border="0" />
</a>
<br></br>

Your continuous love and support are greatly appreciated! Enjoy 🥰

<span>-</span> Jason, Founder of sunnypilot
