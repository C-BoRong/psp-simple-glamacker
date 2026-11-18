"""Inspect the actual PSP PRX; this is not a firmware/physical boot emulator."""
import json
import struct
import sys
from pathlib import Path


def inspect(data):
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected little-endian ELF32 PSP PRX')
    kind, machine = struct.unpack_from('<HH', data, 16)
    if kind != 0xffa0 or machine != 8:
        raise ValueError('Expected relocatable PSP MIPS PRX')
    phoff, shoff = struct.unpack_from('<II', data, 28)
    phsize, phcount, shsize, shcount, strings = struct.unpack_from('<5H', data, 42)
    if phsize != 32 or shsize != 40 or not phcount or strings >= shcount:
        raise ValueError('Invalid ELF tables')
    segments = [struct.unpack_from('<8I', data, phoff+i*phsize) for i in range(phcount)]
    loads = [p for p in segments if p[0] == 1]
    if not loads or min(p[2] for p in loads) != 0:
        raise ValueError('Expected relocatable user partition image')
    for p in loads:
        if p[4] > p[5] or p[1]+p[4] > len(data):
            raise ValueError('Invalid load segment')
    resident = max(p[2]+p[5] for p in loads)
    headers = [struct.unpack_from('<10I', data, shoff+i*shsize) for i in range(shcount)]
    names = data[headers[strings][4]:headers[strings][4]+headers[strings][5]]
    sections = {names[h[0]:].split(b'\0', 1)[0].decode(): h for h in headers}
    def section(name):
        h = sections[name]
        return data[h[4]:h[4]+h[5]]
    def address(addr, size):
        for p in loads:
            if p[2] <= addr and addr+size <= p[2]+p[4]:
                offset = p[1]+addr-p[2]
                return data[offset:offset+size]
        raise ValueError('Address outside file-backed load segment')
    module = section('.rodata.sceModuleInfo')
    if struct.unpack_from('<H', module)[0] != 0:
        raise ValueError('Expected user-mode module')
    imports = []
    stubs = section('.lib.stub')
    offset = 0
    while offset < len(stubs):
        name, version, flags, words, variables, functions, nids, calls = struct.unpack_from('<IHHBBHII', stubs, offset)
        if words < 5 or offset+words*4 > len(stubs):
            raise ValueError('Invalid import stub')
        library = address(name, 1)
        for i in range(1, 80):
            c = address(name+i, 1)
            if c == b'\0': break
            library += c
        else: raise ValueError('Unterminated import library')
        library = library.decode('ascii')
        imports.append({'library': library, 'weak': bool(flags & 8), 'functions': functions})
        if library in ('sceJpeg', 'sceNetInet') and not flags & 8:
            raise ValueError('Optional module imported strongly: '+library)
        offset += words*4
    # PRX generation strips symbols. Heap is the project's explicit source
    # configuration, not a value inferred from the stripped executable.
    heap = 6*1024*1024
    if resident+heap > 20*1024*1024:
        raise ValueError('Unexpected PSP-1000 fixed heap/resident budget')
    return {'resident_bytes': resident, 'configured_heap_bytes': heap,
            'static_plus_heap_bytes': resident+heap, 'imports': imports,
            'limitation': 'Excludes system modules, thread stacks, heap fragmentation and hardware validation'}


if __name__ == '__main__':
    report = inspect(Path(sys.argv[1]).read_bytes())
    print(json.dumps(report, indent=2))
