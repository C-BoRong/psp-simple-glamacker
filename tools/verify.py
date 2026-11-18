import hashlib,struct,sys
from pathlib import Path
from psp_iso import records,check_name,name_of,directory_plan
from check_psp_binary import inspect

def verify(image,stage):
    size=image.stat().st_size
    if size>1500000000:raise ValueError('ISO exceeds 1,500,000,000 bytes')
    expected={p.relative_to(stage).as_posix():p for p in stage.rglob('*') if p.is_file()};seen=set()
    with image.open('rb') as f:
        directory_plan(f,size);f.seek(16*2048+156);root=f.read(34)
        def walk(r,prefix):
            lba=struct.unpack_from('<I',r,2)[0];length=struct.unpack_from('<I',r,10)[0];f.seek(lba*2048)
            for _,c in records(f.read(length)):
                name=name_of(c)
                if name in (b'\0',b'\1'):continue
                check_name(c);key=prefix+name.decode('ascii');
                if c[25]&2:walk(c,key+'/');continue
                p=expected[key];n=struct.unpack_from('<I',c,10)[0];f.seek(struct.unpack_from('<I',c,2)[0]*2048)
                if key in seen or n!=p.stat().st_size:raise ValueError(key)
                with p.open('rb') as disk:
                    while n:
                        k=min(n,1024*1024)
                        if f.read(k)!=disk.read(k):raise ValueError('ISO differs: '+key)
                        n-=k
                seen.add(key)
        walk(root,'')
    if seen!=set(expected):raise ValueError('ISO omissions')
    prx=(stage/'PSP_GAME/SYSDIR/EBOOT.BIN').read_bytes();inspect(prx)
    if prx!=(stage/'PSP_GAME/SYSDIR/BOOT.BIN').read_bytes():raise ValueError('Boot copies differ')
    print(f'ISO verified: {len(seen)} files, {size:,} bytes')
if __name__=='__main__':verify(Path(sys.argv[1]),Path(sys.argv[2]))
