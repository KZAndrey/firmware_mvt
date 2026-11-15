def validate_heap(sample, minimum_largest, settled=False):
    """Integrity is unconditional; compare capacity only with idle queues."""
    if sample.get('integrity') == '0':
        raise RuntimeError('Heap integrity failure')
    if 'workerStack' in sample and int(sample['workerStack']) < 1024:
        raise RuntimeError('WSS worker stack reserve below 1024 bytes')
    if settled and int(sample.get('largest', 999999)) < minimum_largest:
        raise RuntimeError('Largest internal block below '+str(minimum_largest)+' bytes')
