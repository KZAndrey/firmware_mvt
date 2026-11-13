"""Compatibility entry point: exercise GPIO_v2 channel selection and delivery."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("test_sensor_delivery.py")), run_name="__main__")
