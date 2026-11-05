"""Check actual Xtensa wrapper entries, not only host-side behavior."""
import subprocess, sys, re
objdump, elf = sys.argv[1:]
for operation in ('read', 'write'):
    symbol = '__wrap_esp_transport_' + operation
    s = subprocess.check_output([objdump, '-d', '--disassemble='+symbol, elf], text=True)
    m = re.search('<'+symbol+r'>:\s*\n([^\n]+)', s)
    if not m:
        raise RuntimeError(symbol+' missing from linked image')
    if not re.search(r'\bentry\s+a1,', m[1]):
        raise RuntimeError(symbol+' invalid entry: '+m[1])
    print('Linked Xtensa TLS '+operation+' wrapper entry: PASS')
