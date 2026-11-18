"""Build a bounded ATRAC3 RIFF preview for the PSP shell from original music."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

RATE = 44100
DURATION = 20
MAX_BYTES = 500000  # Project budget for the shell preview, separate from ISO cap.


def chunks(blob):
    if len(blob) < 12 or blob[:4] != b'RIFF' or blob[8:12] != b'WAVE' or struct.unpack_from('<I', blob, 4)[0] != len(blob)-8:
        raise ValueError('Invalid ATRAC RIFF envelope')
    result = {}
    offset = 12
    while offset < len(blob):
        if offset+8 > len(blob):
            raise ValueError('Truncated RIFF chunk')
        name, size = struct.unpack_from('<4sI', blob, offset)
        end = offset+8+size
        if end+(size&1) > len(blob) or name in result:
            raise ValueError('Truncated or repeated RIFF chunk')
        result[name] = blob[offset+8:end]
        offset = end+(size&1)
    return result


def riff(parts):
    body = b'WAVE'+b''.join(struct.pack('<4sI', name, len(data))+data+b'\0'*(len(data)&1) for name, data in parts)
    return b'RIFF'+struct.pack('<I', len(body))+body


def preview_container(wave):
    parts = chunks(wave)
    fmt, data = parts[b'fmt '], parts[b'data']
    block = struct.unpack_from('<H', fmt, 12)[0]
    if not block or not data or len(data)%block:
        raise ValueError('Incomplete ATRAC frames')
    samples = len(data)//block*1024
    # Whole-clip forward loop. Encoded silence at both ends avoids a hard seam.
    smpl = struct.pack('<15I', 0, 0, round(1e9/RATE), 60, 0, 0, 0, 1, 0,
                       0, 0, 0, samples-1, 0, 0)
    result = riff([(b'fmt ', fmt), (b'fact', struct.pack('<II', samples, 0)),
                   (b'smpl', smpl), (b'data', data)])
    validate(result)
    return result


def validate(blob):
    if len(blob) > MAX_BYTES:
        raise ValueError('Shell audio exceeds preview budget')
    parts = chunks(blob)
    fmt, fact, smpl, data = (parts.get(key, b'') for key in (b'fmt ', b'fact', b'smpl', b'data'))
    if len(fmt) != 32 or struct.unpack_from('<HHI', fmt) != (0x270, 2, RATE) or struct.unpack_from('<H', fmt, 16)[0] != 14:
        raise ValueError('Expected stereo 44.1 kHz ATRAC3 with complete codec parameters')
    block = struct.unpack_from('<H', fmt, 12)[0]
    if block != 384 or not data or len(data)%block:
        raise ValueError('Expected complete 132.3 kbps ATRAC3 frames')
    samples = len(data)//block*1024
    if len(fact) != 8 or struct.unpack('<II', fact) != (samples, 0) or not RATE*19 <= samples <= RATE*21:
        raise ValueError('Invalid preview duration/sample count')
    if len(smpl) != 60 or struct.unpack_from('<II', smpl, 28) != (1, 0) or struct.unpack_from('<6I', smpl, 36) != (0, 0, 0, samples-1, 0, 0):
        raise ValueError('Invalid preview loop')
    return samples


def run(args):
    subprocess.run([str(arg) for arg in args], check=True, capture_output=True)


def build(source, destination, encoder, ffmpeg):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='xmb-audio-', dir=destination.parent) as temp:
        temp = Path(temp)
        wav, oma, at3 = (temp/name for name in ('input.wav', 'encoded.oma', 'remux.wav'))
        run([ffmpeg, '-v', 'error', '-nostdin', '-y', '-i', source, '-t', DURATION,
             '-af', 'afade=t=in:d=0.8,afade=t=out:st=18.5:d=1.5', '-ar', RATE,
             '-ac', 2, '-c:a', 'pcm_s16le', wav])
        run([encoder, '-e', 'atrac3', '-i', wav, '-o', oma])
        run([ffmpeg, '-v', 'error', '-nostdin', '-y', '-i', oma, '-c:a', 'copy', '-f', 'wav', at3])
        blob = preview_container(at3.read_bytes())
        destination.write_bytes(blob)
    # Independent decoder check; the encoder cannot validate its own output.
    run([ffmpeg, '-v', 'error', '-nostdin', '-i', destination, '-f', 'null', '-'])
    print(f'XMB sound: ATRAC3 stereo, {validate(blob)/RATE:.3f} s, {len(blob)} bytes, forward loop')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--encoder', type=Path, required=True)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()
    build(args.source, args.destination, args.encoder, args.ffmpeg)
