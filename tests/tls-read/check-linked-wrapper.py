"""Check the actual Xtensa ELF, not just host-side wrapper behavior."""
import subprocess, sys, re
objdump, elf = sys.argv[1:]
s = subprocess.check_output([objdump, '-d', '--disassemble=__wrap_esp_transport_read', elf], text=True)
m = re.search(r'<__wrap_esp_transport_read>:\s*\n([^\n]+)', s)
if not m:
    raise RuntimeError('TLS wrapper missing from linked image')
if not re.search(r'\bentry\s+a1,', m[1]):
    raise RuntimeError('TLS wrapper points inside another function: ' + m[1])
print('Linked Xtensa TLS wrapper has a valid windowed-ABI function entry: PASS')
