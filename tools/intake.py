"""Inventory inputs without loading scripts or copying originals into the disc."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'inputs/original'
TYPES = {'images':'.png .jpg .jpeg .bmp .webp', 'audio':'.wav .ogg .mp3 .flac .at3',
         'scripts':'.ast .txt .ks .json .csv .rpy .lua .xml', 'movies':'.mp4 .wmv .ogv .avi'}

def inventory():
    SOURCE.mkdir(parents=True,exist_ok=True)
    groups = {key:[] for key in (*TYPES,'other')}
    for p in sorted(SOURCE.rglob('*')):
        if not p.is_file() or p.name=='.gitkeep' or not p.resolve().is_relative_to(SOURCE.resolve()): continue
        kind=next((k for k,v in TYPES.items() if p.suffix.lower() in v.split()),'other')
        groups[kind].append({'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size})
    output=ROOT/'build/intake.json';output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(groups,indent=2,ensure_ascii=False),encoding='utf8')
    print('原文件目录：',SOURCE);print('分类统计：',{k:len(v) for k,v in groups.items()});print('候选清单：',output)
    if not any(groups.values()): print('请将解包后的文件连同目录结构放在上述目录，放好后告诉 agent。')

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf8');inventory()
