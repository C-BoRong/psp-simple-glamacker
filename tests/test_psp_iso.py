import io
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from psp_iso import SECTOR, records, repack, check_name, name_of, directory_plan


def record(name, lba=40, size=12, flags=0):
    length = (33 + len(name) + 1) & ~1
    data = bytearray(length)
    data[0] = length
    struct.pack_into('<I', data, 2, lba)
    struct.pack_into('>I', data, 6, lba)
    struct.pack_into('<I', data, 10, size)
    struct.pack_into('>I', data, 14, size)
    data[25] = flags
    data[28:32] = b'\x01\x00\x00\x01'
    data[32] = len(name)
    data[33:33 + len(name)] = name
    return bytes(data)


def directory(items, sectors=1):
    out = bytearray(sectors * SECTOR)
    pos = 0
    for r in items:
        if pos % SECTOR + len(r) > SECTOR:
            pos = (pos // SECTOR + 1) * SECTOR
        if pos + len(r) > len(out):
            raise ValueError('fixture overflow')
        out[pos:pos + len(r)] = r
        pos += len(r)
    return bytes(out)


def psp_find(data, wanted):
    # PSP's C-string lookup reads beyond nameLen until the first NUL.
    return any(data[pos + 33:].split(b'\0', 1)[0] == wanted for pos, _ in records(data))


class PspIsoTests(unittest.TestCase):
    def test_reproduces_title_success_font_failure(self):
        before = directory([record(b'CHINESE.FNT'), record(b'CHOICE.TEX'), record(b'TITLE.RGBA')])
        self.assertFalse(psp_find(before, b'CHINESE.FNT'))
        self.assertTrue(psp_find(before, b'TITLE.RGBA'))
        with self.assertRaisesRegex(ValueError, 'NUL terminator'):
            check_name(next(records(before))[1])
        after = repack(before)
        for _, r in records(after):
            check_name(r)
            self.assertTrue(psp_find(after, name_of(r)))
        self.assertEqual(repack(after), after)
        # Metadata (including payload LBA/size) and exact identifiers survive.
        self.assertEqual([(r[1:33], name_of(r)) for _, r in records(before)],
                         [(r[1:33], name_of(r)) for _, r in records(after)])

    def test_sector_boundary_and_odd_directory(self):
        items = [record(b'AUDIO', flags=2)] + [record(f'F{i:06}.BIN'.encode()) for i in range(80)]
        after = repack(directory(items, 3))
        self.assertEqual(len(list(records(after))), len(items))
        for pos, r in records(after):
            check_name(r)
            self.assertLessEqual(pos % SECTOR + len(r), SECTOR)

    def test_refuses_overflow_and_extensions(self):
        # 46 * 44 fits one sector; 46 * 46 does not.
        with self.assertRaisesRegex(ValueError, 'reserved extent'):
            repack(directory([record(b'CHINESE.FNT')] * 46))
        with self.assertRaisesRegex(ValueError, 'Unsupported PSP ISO identifier'):
            repack(directory([record(b'FONT.FNT;1')]))
        r = record(b'FONT.FNT') + b'XX'
        r = bytes([len(r)]) + r[1:]
        with self.assertRaisesRegex(ValueError, 'system-use'):
            repack(directory([r]))

    def test_plan_does_not_write_and_rejects_file_overlap(self):
        image = bytearray(42 * SECTOR)
        pvd = 16 * SECTOR
        image[pvd:pvd + 7] = b'\x01CD001\x01'
        image[pvd + 156:pvd + 190] = record(b'\0', 20, SECTOR, 2)
        image[20 * SECTOR:21 * SECTOR] = directory([record(b'CHINESE.FNT'), record(b'TITLE.RGBA')])
        stream = io.BytesIO(image)
        plan = directory_plan(stream, len(image))
        self.assertEqual(len(plan), 1)
        self.assertEqual(stream.getvalue(), image)
        image[20 * SECTOR:21 * SECTOR] = directory([record(b'CHINESE.FNT', lba=20)])
        with self.assertRaisesRegex(ValueError, 'overlaps directory'):
            directory_plan(io.BytesIO(image), len(image))


if __name__ == '__main__':
    unittest.main()
