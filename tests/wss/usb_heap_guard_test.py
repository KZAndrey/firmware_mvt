import unittest
from usb_heap_guard import validate_heap

class GuardTest(unittest.TestCase):
    def test_recorded_handshake_then_authenticated_idle(self):
        validate_heap({'integrity':'1','auth':'0','largest':'4084'},4096)
        validate_heap({'integrity':'1','auth':'1','largest':'4596'},4096,settled=True)
    def test_same_threshold_stops_idle_pressure(self):
        with self.assertRaisesRegex(RuntimeError,'below 4096'):
            validate_heap({'integrity':'1','largest':'4084'},4096,settled=True)
    def test_corruption_always_stops(self):
        for settled in (False,True):
            with self.assertRaisesRegex(RuntimeError,'integrity'):
                validate_heap({'integrity':'0','largest':'9000'},4096,settled)

if __name__=='__main__':unittest.main()
