import argparse, array, json, re, shutil, subprocess, sys
import struct
from display_list import Library, Pen
from pathlib import Path
from PIL import Image, ImageDraw, ImageOps, ImageColor
from font_content import chinese_bold, common_text

ROOT=Path(__file__).resolve().parents[1]
def require(ok,msg):
    if not ok: raise ValueError(msg)
def asset(name):
    p=(ROOT/name).resolve();require(any(p.is_relative_to((ROOT/x).resolve()) for x in ('inputs/original','project/assets')) and p.is_file(),f'无效素材路径: {name}');return p
def fields(obj,allowed):require(isinstance(obj,dict) and set(obj)<=set(allowed.split()),f'不支持的字段: {obj}')
def picture(path,size):
    with Image.open(asset(path)) as im:return ImageOps.fit(ImageOps.exif_transpose(im).convert('RGB'),size,method=Image.Resampling.LANCZOS)
def packed(im,alpha):
    if alpha:v=array.array('H',[(r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12) for r,g,b,a in im.getdata()])
    else:v=array.array('H',[(r>>3)|((g>>2)<<5)|((b>>3)<<11) for r,g,b in im.getdata()])
    if sys.byteorder!='little':v.byteswap()
    return v.tobytes()
def prepare(cfg,story,out,headers,preview):
    fields(cfg,'title disc_id theme menu media gallery');fields(story,'start nodes');colors=cfg['theme'];media=cfg['media'];menu=cfg['menu'];nodes=story['nodes']
    fields(media,'icon xmb_background scene_background bgm xmb_music');require(set(colors)==set('sky land ink paper accent menu'.split()) and all(re.fullmatch(r'#[0-9a-fA-F]{6}',v) for v in colors.values()),'theme: 需要六个 #RRGGBB 色值')
    require(re.fullmatch('[A-Z]{4}[0-9]{5}',cfg['disc_id']) and 0<len(cfg['title'].strip())<=30,'无效标题或 disc_id');require(0<len(nodes)<=100000 and len(menu)<=4,'场景或菜单类别超出限制')
    ids={n['id']:i for i,n in enumerate(nodes)};require(len(ids)==len(nodes) and story['start'] in ids,'节点重复或起点缺失');start=ids[story['start']];labels=[cfg['title'],'终','●'];rows=[];targets=[];operations=[]
    for n in nodes:
        fields(n,'id speaker text next choices end background');require(sum(k in n for k in ('next','choices','end'))==1,f"节点 {n['id']} 需要唯一出口");labels += [n['speaker'],n['text']];require(n['text'].strip(),'对白不能为空')
        if 'choices' in n:
            require(1<=len(n['choices'])<=4,'每页 1 至 4 个选项');
            for c in n['choices']:fields(c,'label target');labels.append(c['label'])
            dest=[c['target'] for c in n['choices']]
        elif 'next' in n:dest=[n['next']]
        else:require(n['end'] is True,'end 必须为 true');dest=[]
        require(all(x in ids for x in dest),f"未知目标: {n['id']} -> {dest}");targets.append([ids[x] for x in dest]);
    for g in menu:
        fields(g,'label items');labels.append(g['label']);require(1<=len(g['items'])<=5,'分类需要 1 到 5 个菜单项');group=[]
        for it in g['items']:
            a=it['action'];fields(it,'label action target' if a=='jump' else 'label action');labels.append(it['label']);special={'resume':-1,'save':-2,'load':-3,'gallery':-4};require(a in (*special,'restart','jump'),f'未实现菜单动作: {a}');group.append(ids[it['target']] if a=='jump' else start if a=='restart' else special[a])
        operations.append(group)
    user=out/'PSP_GAME/USRDIR';user.mkdir(parents=True,exist_ok=True);headers.mkdir(parents=True,exist_ok=True);preview.mkdir(parents=True,exist_ok=True)
    font=chinese_bold(ROOT,14,''.join(map(common_text,labels)))
    from io import BytesIO
    small=font.font_variant(font=BytesIO(font.font_bytes),size=12);big=font.font_variant(font=BytesIO(font.font_bytes),size=20)
    def text(d,pos,s,fill,font=font,width=440):
        s=common_text(s);require(d.textlength(s,font=font)<=width,f'文字过宽，请分页或缩短：{s}');d.text(pos,s,font=font,fill=fill,anchor='lt')
    def lines(s,width):
        result=[]
        for part in common_text(s).split('\n'):
            line=''
            for ch in part:
                if font.getlength(line+ch)>width:result.append(line);line=''
                line+=ch
            result.append(line)
        require(len(result)<=2,'对白超出两行，请拆成多个节点');return result
    def scene_bg(path):
        if path:return picture(path,(480,272))
        im=Image.new('RGB',(480,272),colors['sky']);d=ImageDraw.Draw(im);d.polygon([(0,114),(98,58),(147,90),(230,36),(342,100),(410,63),(480,96),(480,272),(0,272)],fill=colors['land']);d.rectangle((0,129,479,271),fill=colors['ink']);d.rectangle((0,145,479,148),fill=colors['accent']);return im
    bg=scene_bg(media.get('scene_background'));lib=Library(user);bg_ids={};node_records=[]
    for i,n in enumerate(nodes):
        choices=n.get('choices',[]);path=n.get('background') or media.get('scene_background')
        if path not in bg_ids:bg_ids[path]=lib.background(packed(scene_bg(path),False))
        dest=targets[i];node_records.append(struct.pack('<7I',bg_ids[path],len(lib.runs),len(dest),*(dest+[0]*(4-len(dest)))))
        for selected in range(max(1,len(choices))):
            im=scene_bg(path) if i==start and selected==0 else None;d=Pen(lib,im);d.rectangle((14,164,465,260),fill=colors['paper']);d.rectangle((14,164,19,260),fill=colors['accent']);text(d,(30,175),n['speaker'],colors['ink'],small,395)
            for j,line in enumerate(lines(n['text'],417)):text(d,(30,199+20*j),line,colors['ink'],width=417)
            for j,c in enumerate(choices):
                y=153-29*(len(choices)-j);d.rectangle((226,y,465,y+24),fill=colors['accent' if j==selected else 'paper']);text(d,(236,y+5),c['label'],colors['ink'],small,223)
            if not targets[i]:text(d,(438,239),'终',colors['ink'],small)
            lib.run(d.commands)
            if im:im.save(preview/'scene.png')
    gallery=cfg.get('gallery',[]) if any(-4 in g for g in operations) else [];art=[]
    if any(-4 in g for g in operations):require(1<=len(gallery)<=99,'鉴赏需要 1 至 99 个条目')
    for e in gallery:
        fields(e,'label image');path=e.get('image') or media.get('scene_background');require(e['label'].strip(),'鉴赏标题为空')
        if path not in bg_ids:bg_ids[path]=lib.background(packed(scene_bg(path),False))
        art.append(bg_ids[path])
    from atri_ui import prepare_native
    prepare_native(ROOT,user,headers,font,menu,art,gallery)
    cats=[];actions=[]
    for g in operations:cats.append((0,len(actions),len(g)));actions+=g
    for key,name,size in [('icon','ICON0.PNG',(144,80)),('xmb_background','PIC1.PNG',(480,272))]:
        im=picture(media[key],size) if media.get(key) else ImageOps.fit(bg,size)
        if not media.get(key):
            d=ImageDraw.Draw(im);text(d,(7,8),cfg['title'],colors['ink'],small if key=='icon' else big,size[0]-14)
        im.save(out/'PSP_GAME'/name)
    if media.get('bgm'):
        dest=user/'MUSIC.PCM';subprocess.run(['ffmpeg','-v','error','-nostdin','-y','-i',str(asset(media['bgm'])),'-vn','-ar','44100','-ac','2','-f','s16le',str(dest)],check=True);require(dest.stat().st_size>0,'空音乐')
    if media.get('xmb_music'):
        from xmb_audio import validate
        data=asset(media['xmb_music']).read_bytes();validate(data);(out/'PSP_GAME/SND0.AT3').write_bytes(data)
    def numbers(values):return ','.join(map(str,values)) or '0'
    def structs(values):return ','.join('{'+numbers(v)+'}' for v in values) or '{0,0,1}'
    (user/'NODES.BIN').write_bytes(b''.join(node_records));(user/'RUNS.BIN').write_bytes(b''.join(lib.runs))
    (headers/'catalog.h').write_text(f'#define START_NODE {start}\n#define MENU_COUNT {len(menu)}\n#define MENU_DIVISOR {max(1,len(menu))}\n#define HAS_MUSIC {int(bool(media.get("bgm")))}\nstatic const struct {{int frame,first,count;}} groups[]={{ {structs(cats)} }};\nstatic const int actions[]={{ {numbers(actions)} }};\n',encoding='ascii')
    disc=cfg['disc_id'];(out/'UMD_DATA.BIN').write_bytes(f'{disc[:4]}-{disc[4:]}|0000000000000000|0001|G|00000000|00000000|00000000'.encode('ascii'))
    import hashlib
    signature=int.from_bytes(hashlib.sha256(json.dumps(story,ensure_ascii=False,sort_keys=True).encode()).digest()[:4],'little')
    with (headers/'catalog.h').open('a',encoding='ascii') as h:
        h.write(f'#define DISC_ID "{disc}"\n#define STORY_SIG {signature}u\n#define NODE_COUNT {len(nodes)}\n#define ART_COUNT {len(art)}\nstatic const int art_ids[]={{ {numbers(art)} }};\n')
    return len(lib.runs)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('out',type=Path);p.add_argument('headers',type=Path);p.add_argument('preview',type=Path);a=p.parse_args();read=lambda n:json.loads((ROOT/'project'/n).read_text(encoding='utf-8-sig'));print('Frames:',prepare(read('game.json'),read('story.json'),a.out,a.headers,a.preview))
