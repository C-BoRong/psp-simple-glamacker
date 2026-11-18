import json, struct, sys, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from prepare import prepare, asset

class TemplateTests(unittest.TestCase):
    def setUp(self):
        self.cfg=json.loads((ROOT/'project/game.json').read_text(encoding='utf8'));self.story=json.loads((ROOT/'project/story.json').read_text(encoding='utf8'))
    def test_bad_target_and_unknown_action(self):
        self.story['nodes'][0]['next']='missing'
        with self.assertRaisesRegex(ValueError,'未知目标'):prepare(self.cfg,self.story,None,None,None)
        self.story['nodes'][0]['next']='ticket';self.cfg['menu'][0]['items'][0]['action']='unknown_action'
        with self.assertRaisesRegex(ValueError,'未实现菜单动作'):prepare(self.cfg,self.story,None,None,None)
    def test_assets_confined_to_input(self):
        with self.assertRaises(ValueError):asset('inputs/original/../../runtime/main.c')
    def test_no_menu_has_no_overlay_or_audio(self):
        self.cfg['menu']=[]
        scratch=ROOT/'build';scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as tmp:
            p=Path(tmp);frames=prepare(self.cfg,self.story,p/'disc',p/'obj',p/'preview')
            self.assertEqual(frames,5);self.assertFalse((p/'preview/menu.png').exists());self.assertFalse((p/'disc/PSP_GAME/USRDIR/MUSIC.PCM').exists());self.assertIn('#define MENU_COUNT 0',(p/'obj/catalog.h').read_text())

    def test_long_story_shares_art_and_seeks_last_branch(self):
        self.cfg['menu']=[{'label':'章','items':[{'label':'跳','action':'jump','target':'512'}]}]
        nodes=[dict(id=str(i),speaker='甲',text='乙',next=str(i+1)) for i in range(1024)]
        nodes[-1]={'id':'1023','speaker':'甲','text':'乙','choices':[{'label':'甲','target':'0'},{'label':'乙','target':'512'}]}
        scratch=ROOT/'build';scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as tmp:
            p=Path(tmp);prepare(self.cfg,{'start':'0','nodes':nodes},p/'disc',p/'obj',p/'preview');u=p/'disc/PSP_GAME/USRDIR'
            self.assertEqual((u/'BACK.BIN').stat().st_size,480*272*2)
            self.assertLess((u/'GLYPHS.BIN').stat().st_size,32000)
            self.assertLess(sum(f.stat().st_size for f in u.iterdir() if f.name!='CHINESE.FNT'),500000)
            self.assertFalse(list(u.glob('F*.BIN')))
            records=(u/'NODES.BIN').read_bytes();self.assertEqual(len(records),28*1024)
            bg,first,count,a,b,_,_=struct.unpack_from('<7I',records,28*1023);self.assertEqual((bg,count,a,b),(0,2,0,512))
            runs=(u/'RUNS.BIN').read_bytes();commands=(u/'SCENES.BIN').read_bytes()
            for i in (first,first+1):
                off,n=struct.unpack_from('<II',runs,8*i);self.assertGreater(n,0);self.assertLessEqual(n,256);self.assertLessEqual(off+16*n,len(commands))
