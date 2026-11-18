"""Prepare native ATRI renderer inputs. Only project labels/counts are bound at build time."""
from pathlib import Path
from PIL import Image, ImageDraw
import json, struct, zlib

def prepare_native(root,user,obj,font,menu,art,entries):
    src=root/'runtime/atri-ui';quote=lambda s:json.dumps(s,ensure_ascii=True)
    text=(src/'ui.c').read_text('utf8');first=text.index('    static const char *cats[]');last=text.index('    float opacity=',first)
    cats=','.join(quote(g['label']) for g in menu) or '0';items=','.join('{'+','.join(quote(i['label']) for i in g['items'])+'}' for g in menu) or '{0}'
    text=text[:first]+f'    static const char *cats[4]={{{cats}}};\n    static const char *items[4][8]={{{items}}};\n    if(category>={len(menu)})return;\n'+text[last:]
    text=text.replace('i<4;++i){float distance','i<'+str(len(menu))+';++i){float distance') if menu else text.replace('if(category>=0)return;','return;').replace('i<4;++i){float distance','i<4;++i){float distance')
    (obj/'ui.c').write_text(text,encoding='utf8')
    counts=([len(g['items']) for g in menu]+[1]*4)[:4];motion=(src/'ui_motion.c').read_text('utf8');(obj/'ui_motion.c').write_text(motion.replace('{8,3,3,6}',str(counts).replace('[','{').replace(']','}')),encoding='utf8')
    texts=''.join((src/n).read_text('utf8') for n in ('ui.c','gallery.c'))+(root/'runtime/main.c').read_text('utf8')+json.dumps(menu,ensure_ascii=False)
    chars=sorted(set(chr(c) for c in range(32,127))|{c for c in texts if ord(c)>127 and c!='\ufeff'})
    pages=[Image.new('L',(512,512)) for _ in range((len(chars)+1023)//1024)]
    for i,c in enumerate(chars):ImageDraw.Draw(pages[i//1024]).text((i%32*16,i%1024//32*16+13),c,font=font,fill=255,anchor='ls')
    blob=struct.pack('<4sIII',b'FNT2',2,len(chars),len(pages))+b''.join(struct.pack('<I',ord(c)) for c in chars)
    for page in pages:
        raw=page.tobytes();blob+=bytes(a//17|(b//17<<4) for a,b in zip(raw[::2],raw[1::2]))
    (user/'CHINESE.FNT').write_bytes(blob)
    if not art:return
    if len(art)>64:raise ValueError('Native gallery supports at most 64 groups')
    dest=user/'IMAGES';dest.mkdir(exist_ok=True);records=[]
    with (user/'BACK.BIN').open('rb') as backgrounds:
        for i,bg in enumerate(art):
            path=dest/f'I{bg:05}.IMG'
            if not path.exists():
                backgrounds.seek(bg*480*272*2);raw=backgrounds.read(480*272*2);tiles=[]
                for y in range(0,272,128):
                    for x in range(0,480,128):
                        rows=[]
                        for j in range(128):
                            off=(min(271,y+j)*480+x)*2;row=raw[off:off+min(128,480-x)*2];rows.append(row+row[-2:]*((256-len(row))//2))
                        tiles.append(zlib.compress(b''.join(rows)))
                header=struct.pack('<4s8I5I',b'IMG2',2,480,272,480,272,1,12,0,480,272,4,3,0);off=len(header)+8*12
                for tile in tiles:header+=struct.pack('<II',off,len(tile));off+=len(tile)
                path.write_bytes(header+b''.join(tiles))
            records.append(struct.pack('<7i32s',i,bg,480,272,-1,0,0,f'art{i}'.encode()))
    (user/'GALLERY.BIN').write_bytes(struct.pack('<4s3I',b'GAL1',1,len(records),60)+b''.join(records))
