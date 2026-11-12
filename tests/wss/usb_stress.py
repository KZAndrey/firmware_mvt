"""Bounded physical USB stress run. Requires opt-in MVT_WSS_STRESS_TEST build.

Does not change saved settings, publish firmware, or restart Relay.
Raw logs may contain credentials: keep output prefix in ignored .tools.
"""
import argparse
import datetime
import json
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '.tools/usb-check'))
import serial
from usb_heap_guard import validate_heap


def command_bytes(command):
    value = 0x4d560000 | command
    data = bytearray([8])
    while value > 127:
        data.append((value & 127) | 128)
        value >>= 7
    data.append(value)
    payload = bytes([0x3a, len(data)]) + data
    return bytes([0x94, 0xc3, 0, len(payload)]) + payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', required=True)
    parser.add_argument('--prefix', required=True)
    parser.add_argument('--cycles', type=int, default=50)
    parser.add_argument('--reconnect-only', action='store_true')
    parser.add_argument('--min-largest', type=int, choices=[4096,6144], default=6144)
    parser.add_argument('--observe-seconds', type=int, default=0)
    parser.add_argument('--investigate', action='store_true')
    parser.add_argument('--staged-recovery', action='store_true')
    parser.add_argument('--network-check', action='store_true')
    parser.add_argument('--tx-limit', type=int, choices=[4])
    parser.add_argument('--static-tx', action='store_true')
    parser.add_argument('--static-tx-two', action='store_true')
    parser.add_argument('--rx-limit', type=int, choices=[8])
    args = parser.parse_args()
    if args.staged_recovery and not args.investigate:
        parser.error('--staged-recovery requires --investigate')
    if args.rx_limit and not args.tx_limit:
        parser.error('--rx-limit requires --tx-limit 4')
    if args.static_tx_two and (args.static_tx or args.tx_limit or args.rx_limit):
        parser.error('--static-tx-two is a separate profile')
    if args.static_tx and (args.tx_limit or args.rx_limit):
        parser.error('--static-tx is a separate profile')
    prefix = Path(args.prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    result = {'started': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'scenarios': [], 'status': 'running', 'minimumLargestBlockGuard':args.min_largest}
    pending = bytearray()
    samples = []
    events = []
    passive = False
    profile_enabled = False
    restoring = False
    deadline = time.monotonic() + 3600
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.dtr = False
    port.rts = False
    port.port = args.port

    def save():
        prefix.with_suffix('.result.json').write_text(json.dumps(result, indent=2), encoding='utf8')

    with prefix.with_suffix('.bin').open('xb') as raw, prefix.with_suffix('.jsonl').open('x', encoding='utf8') as log:
        def pump(seconds):
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                if not restoring and (time.monotonic() > deadline or prefix.with_suffix('.stop').exists()):
                    raise RuntimeError('Bounded run stopped')
                block = port.read(4096)
                raw.write(block)
                raw.flush()
                if raw.tell() > 128 * 1024 * 1024:
                    raise RuntimeError('Capture size limit')
                pending.extend(block)
                while b'\n' in pending:
                    line, _, rest = pending.partition(b'\n')
                    pending[:] = rest
                    text = line.decode('utf8', errors='replace')
                    marker = re.search(r'MVT_(?:HEAP|STRESS|NET)\b.*', text)
                    if not marker:
                        continue
                    text = marker.group(0)
                    event = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'mono': time.monotonic(), 'text': text}
                    log.write(json.dumps(event) + '\n')
                    log.flush()
                    events.append(event)
                    if text.startswith(('MVT_STRESS memory ', 'MVT_STRESS queues ')) and samples:
                        values = dict(re.findall(r'(\w+)=([^\s]+)', text))
                        if values.get('cycle') == samples[-1].get('cycle'):
                            samples[-1].update(values)
                        if 'uptime' in values:
                            uptime = int(values['uptime'])
                            prior = result.get('lastUptime', uptime)
                            if uptime < prior and prior < 0xffff0000:
                                raise RuntimeError('Unexpected board restart during stress run')
                            result['lastUptime'] = uptime
                    if text.startswith('MVT_HEAP'):
                        values = dict(re.findall(r'(\w+)=([^\s]+)', text))
                        values.update(utc=event['utc'], mono=event['mono'])
                        samples.append(values)
                        validate_heap(values,args.min_largest)
                    if 'HALTED' in text:
                        raise RuntimeError(text)
                if len(pending) > 65536:
                    pending.clear()

        def send(command):
            port.write(command_bytes(command))
            port.flush()

        def ready(timeout=180):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                before = len(samples)
                send(1)
                pump(2)
                fresh = samples[before:]
                for sample in fresh:
                    if sample.get('phase') == 'sample' and sample.get('auth') == '1' and sample.get('wifi') == '3':
                        return sample
            raise RuntimeError('No authenticated WSS recovery before deadline')

        def scenario(command, label, hold=0):
            baseline = ready()
            start = time.monotonic()
            old_events = len(events)
            send(command)
            pump(2)
            if not any('begin command=' + str(command) + ' ' in e['text'] for e in events[old_events:]):
                raise RuntimeError('Stress command not accepted: ' + label)
            pump(hold + 3)
            recovered = ready(180)
            elapsed = time.monotonic() - start
            # Compare idle queues, not a transient snapshot/TLS write allocation.
            settle_deadline = time.monotonic() + 60
            quiet = 0
            while time.monotonic() < settle_deadline:
                settled = ready()
                empty = all(settled.get(k) == '0' for k in ('incoming', 'outgoing', 'txMessages', 'txBytes'))
                quiet = quiet + 1 if empty else 0
                if quiet >= 3:
                    validate_heap(settled,args.min_largest,settled=True)
                    break
            else:
                raise RuntimeError('Queues did not settle; memory comparison is inconclusive')
            result['scenarios'].append({'label': label, 'command': command, 'seconds': round(elapsed, 3), 'before': baseline, 'recovered': recovered, 'settled': settled})
            save()
            print(json.dumps({'scenario': label, 'seconds': round(elapsed, 1), 'free': settled.get('free'), 'largest': settled.get('largest')}), flush=True)

        try:
            port.open()
            save()
            if args.observe_seconds:
                observe_until = time.monotonic() + args.observe_seconds
                result['observations'] = []
                while time.monotonic() < observe_until:
                    before = len(samples)
                    send(1)
                    pump(2)
                    result['observations'].extend(samples[before:])
                    save()
                result['status'] = 'observed'
                return
            result['baseline'] = ready()
            if args.tx_limit or args.static_tx or args.static_tx_two:
                profile_enabled=True
                before_events=len(events)
                send(18 if args.static_tx_two else 17 if args.static_tx else 14 if args.rx_limit else 12)
                pump(5)
                result['limitedTxBaseline']=ready(180)
                config_events=[e['text'] for e in events[before_events:] if 'config tx_type=' in e['text']]
                expected_profile='limit=32' if args.static_tx_two else 'limit=16' if args.static_tx else 'limit=8' if args.rx_limit else 'limit=4'
                if not any(expected_profile in e and
                           (('tx_type=0 ' in e and 'tx_static=2 ' in e) if args.static_tx_two else ('tx_type=0 ' in e and 'tx_static=4 tx_cache=4' in e) if args.static_tx else
                            ('tx_dynamic=4 ' in e and (not args.rx_limit or 'rx_static=6 rx_dynamic=8 ba=6' in e))) for e in config_events):
                    raise RuntimeError('Temporary TX limit not confirmed by WiFi init')
                result['txProfile']=config_events
                save()
            if args.network_check:
                send(10)
                pump(12)
                result['beforeRadioReset'] = ready()
                start = time.monotonic()
                reset_events = len(events)
                send(11)
                pump(5)
                result['afterRadioReset'] = ready(180)
                reset_text = '\n'.join(e['text'] for e in events[reset_events:])
                if 'radio_off ok=1' not in reset_text or 'radio_on ok=1' not in reset_text:
                    raise RuntimeError('Radio driver reset was not confirmed by device')
                result['radioResetSeconds'] = round(time.monotonic()-start, 3)
                result['status'] = 'radio-reset-verified'
                return
            for cycle in range(args.cycles):
                scenario(2, 'reconnect-' + str(cycle + 1))
                if cycle >= 9:
                    warm = result['scenarios'][2]['settled']
                    latest = result['scenarios'][-1]['settled']
                    if int(warm['free']) - int(latest['free']) > 8192:
                        raise RuntimeError('Free heap is over 8192 bytes below warm baseline; investigate cache and allocations before more cycles')
            if not args.reconnect_only:
                for command, seconds in [(3, 5), (4, 30), (5, 120), (3, 5), (3, 5), (3, 5), (7, 60), (8, 0), (9, 0)]:
                    scenario(command, 'fault-' + str(command) + '-' + str(len(result['scenarios'])), seconds)
                scenario(2, 'disconnect-after-snapshot')
            result['status'] = 'passed'
        except Exception as exc:
            result['status'] = 'stopped'
            result['error'] = str(exc)
            investigable = str(exc).startswith(('No authenticated', 'Free heap', 'Largest internal', 'Queues did not settle'))
            if not args.investigate or not investigable:
                raise
            # Keep the failed state for observation before a single explicitly
            # requested test of radio-only recovery. Never reset the board.
            passive = True
            result['failureObservation'] = []
            send(10)
            end = time.monotonic()+(20 if args.staged_recovery else 120)
            while time.monotonic()<end:
                before = len(samples)
                send(1)
                pump(2)
                result['failureObservation'].extend(samples[before:])
                save()
            last = samples[-1] if samples else {}
            if args.staged_recovery and last.get('auth') == '0':
                result['recoveryStages'] = []
                for recovery_command, label in [(15, 'wss-tls-recreate'), (16, 'wifi-rejoin')]:
                    stage = {'label': label, 'before': samples[-1].copy()}
                    result['recoveryStages'].append(stage)
                    begin = time.monotonic()
                    first_event = len(events)
                    send(recovery_command)
                    pump(2)
                    accepted = any('recovery_begin command='+str(recovery_command)+' ' in e['text'] for e in events[first_event:])
                    stage['accepted'] = accepted
                    if not accepted:
                        stage['error'] = 'USB recovery command not accepted'
                        save()
                        break
                    try:
                        stage['after'] = ready(90)
                        stage['recovered'] = True
                    except RuntimeError as stage_error:
                        if str(stage_error) != 'No authenticated WSS recovery before deadline':
                            raise
                        stage['error'] = str(stage_error)
                        stage['after'] = samples[-1].copy()
                        stage['recovered'] = False
                    stage['seconds'] = round(time.monotonic()-begin, 3)
                    stage['events'] = [e for e in events[first_event:] if 'rejoin_requested' in e['text'] or 'radio_' in e['text']]
                    save()
                    print(json.dumps({'recoveryStage': label, 'recovered': stage['recovered'], 'seconds': stage['seconds']}), flush=True)
                    if stage['recovered']:
                        result['recoveredStage'] = label
                        break
                last = samples[-1] if samples else {}
            if last.get('auth') == '0' and last.get('wifi') == '3':
                result['beforeRadioReset'] = last.copy()
                start = time.monotonic()
                reset_events = len(events)
                send(11)
                pump(5)
                try:
                    result['afterRadioReset'] = ready(180)
                    reset_text = '\n'.join(e['text'] for e in events[reset_events:])
                    if 'radio_off ok=1' not in reset_text or 'radio_on ok=1' not in reset_text:
                        raise RuntimeError('Radio driver reset was not confirmed by device')
                    result['radioResetSeconds'] = round(time.monotonic()-start, 3)
                    result['status'] = 'fault-recovered-by-radio-reset'
                except RuntimeError as recovery_error:
                    result['recoveryError'] = str(recovery_error)
                    result['status'] = 'fault-persists-after-radio-reset'
            else:
                result['status'] = 'fault-observed-without-radio-reset'
        finally:
            if port.is_open:
                try:
                    if profile_enabled:
                        restoring=True
                        passive=True
                        before_events=len(events)
                        send(13)
                        try:
                            pump(5)
                            result['restoredTxBaseline']=ready(180)
                            config_events=[e['text'] for e in events[before_events:] if 'config tx_type=' in e['text']]
                            result['txProfileRestored']=any('limit=0' in e for e in config_events)
                            result['restoredTxProfile']=config_events
                            if not result['txProfileRestored']:
                                result['restorationError']='Original WiFi init profile not confirmed'
                        except Exception as restore_error:
                            result['restorationError']=str(restore_error)
                        if result.get('restorationError'):
                            result['status']='restoration-unconfirmed'
                    send(6)
                finally:
                    port.close()
            result['ended'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            result['portClosed'] = not port.is_open
            save()


if __name__ == '__main__':
    main()
