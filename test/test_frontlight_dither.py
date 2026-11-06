#!/usr/bin/env python3
"""Compile the real frontlight HAL and SDK with host PWM/timer adapters."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
case = root / "test/frontlight_dither"
sdk = root / "freeink-sdk/libs/hardware"
with tempfile.TemporaryDirectory(prefix="frontlight-dither-") as temp:
    binary = Path(temp) / "frontlight-test"
    command = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
               "-DFREEINK_FRONTLIGHT_LOG_CURVE=1", "-DFREEINK_X4PRO_FRONTLIGHT_BITS=11",
               "-DFREEINK_X4PRO_FRONTLIGHT_HZ=39062"]
    for include in [case / "stubs", sdk / "FrontlightManager/test/host/stubs",
                    root / "lib/hal", sdk / "FrontlightManager/include",
                    sdk / "BoardConfig/include"]:
        command += ["-I", str(include)]
    command += [str(case / "FrontlightDitherTest.cpp"),
                str(root / "lib/hal/HalFrontlight.cpp"),
                str(sdk / "FrontlightManager/src/FrontlightManager.cpp"), "-o", str(binary)]
    for board, dither in [("X4PRO", 1), ("X4PRO", 0), ("X4", 0)]:
        flags = [f"-DFREEINK_DEVICE_{board}=1", f"-DFREEINK_FRONTLIGHT_DITHER={dither}"]
        if board == "X4":
            # Clang diagnoses pre-existing SDK fields compiled out with FREEINK_CAP_FRONTLIGHT=0.
            flags += ["-Wno-unused-private-field"]
        subprocess.run(command + flags, check=True)
        subprocess.run([str(binary)], check=True)
