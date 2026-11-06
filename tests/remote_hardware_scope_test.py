"""Check resolved PlatformIO flags; no board flashing or dependency download."""
from platformio.project.config import ProjectConfig

config = ProjectConfig()
flag = "-DMESHTASTIC_EXCLUDE_REMOTEHARDWARE=1"
for target, excluded in {
    "heltec-v3": True,
    "nrf52_promicro_diy_tcxo": False,
    "nrf52_promicro_diy-inkhud": True,
}.items():
    section = "env:" + target
    actual = flag in config.get(section, "build_flags") and flag not in config.get(section, "build_unflags", [])
    assert actual == excluded, (target, actual, excluded)
    print(target, "RemoteHardware excluded:", actual)
