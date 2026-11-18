"""Terminate directory identifiers for PSP ISOFS without moving file extents.

mkisofs -omit-version-number follows ISO9660's length-delimited identifiers.
PSP ISOFS also compares these identifiers as C strings (uOFW's
isofsFindIsoDirectory). Odd-length identifiers have no mandatory ISO padding,
so add zero system-use bytes. This accepts only our plain, unversioned images;
it deliberately fails rather than relocate an overflowing directory.
"""
import struct
import sys
from pathlib import Path

SECTOR = 2048


def records(data):
    pos = 0
    while pos < len(data):
        length = data[pos]
        if not length:
            end = min((pos // SECTOR + 1) * SECTOR, len(data))
            if any(data[pos:end]):
                raise ValueError('Nonzero directory sector padding')
            pos = end
            continue
        if length < 34 or pos + length > len(data) or pos % SECTOR + length > SECTOR:
            raise ValueError('Invalid directory record boundary')
        record = data[pos:pos + length]
        if not record[32] or 33 + record[32] > length:
            raise ValueError('Invalid directory identifier length')
        yield pos, record
        pos += length


def name_of(record):
    return record[33:33 + record[32]]


def check_name(record):
    name = name_of(record)
    if name in (b'\0', b'\1'):
        return
    if b';' in name or b'\0' in name or b'/' in name or len(name) >= 32:
        raise ValueError('Unsupported PSP ISO identifier: ' + repr(name))
    end = 33 + len(name)
    if end >= len(record) or record[end] != 0:
        raise ValueError('PSP ISO filename lacks NUL terminator: ' + name.decode('ascii'))


def repack(data):
    output = bytearray(len(data))
    pos = 0
    for _, record in records(data):
        name = name_of(record)
        if name not in (b'\0', b'\1'):
            end = 33 + len(name)
            if any(record[end:]):
                raise ValueError('Unsupported nonzero system-use metadata')
            # Include a terminator, then align the record length to two bytes.
            length = (end + 2) & ~1
            record = bytearray(record[:end]) + bytearray(length - end)
            record[0] = length
            check_name(record)
        if pos % SECTOR + len(record) > SECTOR:
            pos = (pos // SECTOR + 1) * SECTOR
        if pos + len(record) > len(output):
            raise ValueError('PSP directory padding exceeds reserved extent')
        output[pos:pos + len(record)] = record
        pos += len(record)
    return bytes(output)


def directory_plan(stream, image_size):
    stream.seek(16 * SECTOR)
    pvd = stream.read(SECTOR)
    if pvd[:7] != b'\x01CD001\x01' or len(pvd) != SECTOR:
        raise ValueError('Expected ISO9660 primary volume')
    pending = [pvd[156:190]]
    visited = set()
    plan = []
    files = []
    while pending:
        record = pending.pop()
        lba, size = struct.unpack_from('<I', record, 2)[0], struct.unpack_from('<I', record, 10)[0]
        offset = lba * SECTOR
        if lba in visited or offset < 18 * SECTOR or not size or size % SECTOR or offset + size > image_size:
            raise ValueError('Invalid or repeated directory extent')
        visited.add(lba)
        stream.seek(offset)
        data = stream.read(size)
        plan.append((offset, data, repack(data)))
        for _, child in records(data):
            if name_of(child) in (b'\0', b'\1'):
                continue
            if child[25] & 2:
                pending.append(child)
            else:
                start = struct.unpack_from('<I', child, 2)[0] * SECTOR
                length = struct.unpack_from('<I', child, 10)[0]
                if start + length > image_size:
                    raise ValueError('File outside image')
                if length:
                    files.append((start, start + length))
    ranges = sorted((offset, offset + len(old)) for offset, old, _ in plan)
    if any(a[1] > b[0] for a, b in zip(ranges, ranges[1:])):
        raise ValueError('Overlapping directory extents')
    if any(start < b and a < end for start, end in files for a, b in ranges):
        raise ValueError('File overlaps directory extent')
    return plan


def normalize(path):
    path = Path(path)
    # Validate every directory and available capacity before the first write.
    with path.open('r+b') as stream:
        plan = directory_plan(stream, path.stat().st_size)
        changed = 0
        for offset, old, new in plan:
            if old != new:
                stream.seek(offset)
                if stream.write(new) != len(new):
                    raise OSError('Incomplete directory write')
                changed += 1
        stream.flush()
    print(f'PSP directory terminators: {len(plan)} directories checked, {changed} rewritten; file extents unchanged')
    return changed


if __name__ == '__main__':
    normalize(sys.argv[1])
