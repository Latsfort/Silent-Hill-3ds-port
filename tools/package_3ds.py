"""Verify every embedded resource and prepare a self-contained SD-card package."""
import hashlib
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def embedded_files(data):
    assert data[:4] == b'3DSX' and struct.unpack_from('<H', data, 4)[0] >= 44
    romfs = struct.unpack_from('<I', data, 40)[0]
    header = struct.unpack_from('<10I', data, romfs)
    assert header[0] == 40
    dirs, files, payload = (romfs + header[index] for index in (3, 7, 9))
    result = {}

    def walk(offset, parent):
        _, _, child_dir, child_file, _, length = struct.unpack_from('<6I', data, dirs + offset)
        name = data[dirs + offset + 24:dirs + offset + 24 + length].decode('utf-16le')
        path = '/'.join(p for p in (parent, name) if p)
        while child_file != 0xffffffff:
            _, sibling, start, size, _, length = struct.unpack_from('<IIQQII', data, files + child_file)
            name = data[files + child_file + 32:files + child_file + 32 + length].decode('utf-16le')
            key = '/'.join(p for p in (path, name) if p)
            assert key not in result and payload + start + size <= len(data)
            result[key] = memoryview(data)[payload + start:payload + start + size]
            child_file = sibling
        while child_dir != 0xffffffff:
            walk(child_dir, path)
            child_dir = struct.unpack_from('<I', data, dirs + child_dir + 4)[0]

    walk(0, '')
    return result


if __name__ == '__main__':
    game = ROOT / 'silent-hill-3ds.3dsx'
    data = game.read_bytes()
    files = embedded_files(data)
    expected = {p.relative_to(ROOT / 'romfs').as_posix(): p for p in (ROOT / 'romfs').rglob('*') if p.is_file()}
    assert files.keys() == expected.keys(), 'Embedded resource list differs from romfs/'
    for name, path in expected.items():
        assert hashlib.sha256(files[name]).digest() == hashlib.sha256(path.read_bytes()).digest(), name
    print(f'PASS: all {len(files)} embedded resources match romfs/ byte for byte')
    output = ROOT / 'dist/silent-hill-3ds-old3ds.zip'
    output.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(data).hexdigest()
    instructions = (
        'Silent Hill 3DS - installation Old 3DS\r\n\r\n'
        '1. Fermer le jeu et eteindre la console avant de retirer la carte SD.\r\n'
        '2. Extraire le dossier 3ds de cette archive a la racine de la carte SD, en remplacant les anciens fichiers.\r\n'
        '3. Lancer Silent Hill 3DS depuis le Homebrew Launcher.\r\n'
        '4. START + SELECT : retour au menu des points de progression.\r\n\r\n'
        'Les cartes, textures, sons et animations sont inclus dans le fichier .3dsx.\r\n'
        'Ne pas utiliser le fichier .elf sur la console.\r\n'
        'En cas de probleme : recuperer sh3ds_load.log a la racine de la carte SD.\r\n\r\n'
        f'Fichier .3dsx : {len(data)} octets\r\nSHA256 : {digest}\r\n'
    )
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        archive.writestr('3ds/silent-hill-3ds/silent-hill-3ds.3dsx', data)
        archive.write(ROOT / 'silent-hill-3ds.smdh', '3ds/silent-hill-3ds/silent-hill-3ds.smdh')
        archive.writestr('INSTALLATION.txt', instructions)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert hashlib.sha256(archive.read('3ds/silent-hill-3ds/silent-hill-3ds.3dsx')).hexdigest() == digest
    print(f'PASS: SD-card package verified: {output}')
