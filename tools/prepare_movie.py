"""Prepare a retail SysState_Fmv movie from raw extracted XA sectors, without new content."""
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DECOMP = ROOT / 'vendor/silent-hill-decomp'


def source_movie(movie):
    source = (DECOMP / 'src/bodyprog/events/game_sys_states.c').read_text(encoding='utf-8')
    assert 'BASE_AUDIO_FILE_IDX - g_MapEventParam' in source
    base_name = re.search(r'#define BASE_AUDIO_FILE_IDX (FILE_\w+)', source)[1]
    entries = {name: (int(index), path) for name, index, path in re.findall(
        r'(FILE_\w+)\s*=\s*(\d+),\s*//\s*(\S+)', (ROOT / 'dump/assets/fileenum.h.inc').read_text())}
    target = entries[base_name][0] - movie
    return next((name, path) for name, (index, path) in entries.items() if index == target)


def sector_metadata(raw):
    assert len(raw) % 2336 == 0
    frames, audio = [], []
    for index in range(len(raw) // 2336):
        sector = raw[index * 2336:(index + 1) * 2336]
        assert sector[:4] == sector[4:8]
        if sector[2] & 4:
            audio.append((sector[3], sector[8:2312]))
        if sector[8:12] == b'\x60\x01\x01\x80' and struct.unpack_from('<H', sector, 12)[0] == 0:
            number = struct.unpack_from('<I', sector, 16)[0]
            width, height = struct.unpack_from('<HH', sector, 24)
            frames.append((index, number, width, height))
    assert len(frames) > 1 and len({(f[2], f[3]) for f in frames}) == 1
    spacing = {frames[i + 1][0] - frames[i][0] for i in range(len(frames) - 1)}
    assert len(spacing) == 1
    assert [f[1] for f in frames] == list(range(frames[0][1], frames[0][1] + len(frames)))
    stream = (DECOMP / 'src/screens/stream/stream.c').read_text(encoding='utf-8')
    assert 'CdRead2(CdlModeStream | CdlModeSpeed | CdlModeRT | CdlModeSize1)' in stream
    # CD double speed supplies 150 sectors/second; retain the actual inter-frame sector spacing.
    fps = Fraction(150, next(iter(spacing)))
    assert audio and len({coding for coding, _ in audio}) == 1
    xac = b'XAC1' + struct.pack('<HBB', len(audio), audio[0][0], 0) + b''.join(data for _, data in audio)
    return frames, fps, xac


def prepare(movie, check):
    name, relative = source_movie(movie)
    raw = (ROOT / 'dump/assets' / relative).read_bytes()
    frames, fps, xac = sector_metadata(raw)
    width, height = frames[0][2:]
    out = ROOT / f'romfs/game/movie/{movie:03d}.shv'
    audio = out.with_suffix('.xac')
    manifest = out.with_suffix('.json')
    info = {'source': name, 'sha256': hashlib.sha256(raw).hexdigest(), 'width': width,
            'height': height, 'frames': len(frames), 'fps_num': fps.numerator, 'fps_den': fps.denominator}
    header = struct.pack('<8s6I', b'SH3DV001', width, height, len(frames), fps.numerator, fps.denominator, 0)
    expected_size = len(header) + width * height * 2 * len(frames)
    if check:
        saved = json.loads(manifest.read_text())
        assert all(saved[k] == value for k, value in info.items())
        assert out.stat().st_size == expected_size and out.read_bytes()[:32] == header
        assert hashlib.sha256(out.read_bytes()).hexdigest() == saved['output_sha256']
        assert audio.read_bytes() == xac
    else:
        executable = shutil.which('ffmpeg')
        if not executable:
            sys.path.insert(0, str(ROOT / 'build/video-tools'))
            import imageio_ffmpeg
            executable = imageio_ffmpeg.get_ffmpeg_exe()
        build = ROOT / 'build'
        build.mkdir(exist_ok=True)
        wrapped = build / f'movie-{movie:03d}.str'
        with wrapped.open('wb') as file:
            for offset in range(0, len(raw), 2336):
                file.write(b'\x00' + b'\xff' * 10 + b'\x00\x00\x00\x00\x02' + raw[offset:offset + 2336])
        decoded = build / f'movie-{movie:03d}.bgr555'
        subprocess.run([executable, '-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-i', str(wrapped),
            '-map', '0:v:0', '-an', '-fps_mode', 'passthrough', '-pix_fmt', 'bgr555le', '-f', 'rawvideo', str(decoded)], check=True)
        assert decoded.stat().st_size == expected_size - len(header)
        out.parent.mkdir(parents=True, exist_ok=True)
        with out.open('wb') as file, decoded.open('rb') as source:
            file.write(header)
            shutil.copyfileobj(source, file)
        audio.write_bytes(xac)
        info['output_sha256'] = hashlib.sha256(out.read_bytes()).hexdigest()
        info['decoder'] = subprocess.run([executable, '-version'], capture_output=True, text=True, check=True).stdout.splitlines()[0]
        manifest.write_text(json.dumps(info, indent=2) + '\n', encoding='ascii')
    print(f'PASS: FMV {movie} = {relative}, {width}x{height}, {len(frames)} frames, {fps} fps; original XA packets retained')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('movie', type=int)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    prepare(args.movie, args.check)
