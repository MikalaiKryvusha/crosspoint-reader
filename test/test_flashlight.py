#!/usr/bin/env python3
"""Exercise production flashlight activity/HAL with host peripheral and UI adapters."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
case=root/'test/flashlight'
with tempfile.TemporaryDirectory(prefix='flashlight-') as temp:
 binary=Path(temp)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(case/'stubs'),'-I',str(root/'lib/hal'),'-I',str(root/'src'),str(case/'FlashlightTest.cpp'),str(root/'src/activities/util/FlashlightActivity.cpp'),str(root/'lib/hal/HalFrontlight.cpp'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
