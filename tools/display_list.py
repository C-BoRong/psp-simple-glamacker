"""On-disc scene records. Commands are fixed-width; only one selection is loaded."""
import hashlib
import struct
from PIL import Image, ImageColor, ImageDraw


def color(value):
    if isinstance(value,str):value=ImageColor.getrgb(value)
    r,g,b=value[:3]
    return (r>>3)|((g>>2)<<5)|((b>>3)<<11)


class Library:
    def __init__(self,folder):
        self.folder=folder; self.glyphs={}; self.backgrounds={}; self.runs=[]; self.offset=0
        for name in ('GLYPHS','SCENES','BACK'):(folder/(name+'.BIN')).write_bytes(b'')

    def background(self,pixels):
        key=hashlib.sha256(pixels).digest()
        if key not in self.backgrounds:
            self.backgrounds[key]=len(self.backgrounds)
            with (self.folder/'BACK.BIN').open('ab') as f:f.write(pixels)
        return self.backgrounds[key]

    def glyph(self,font,ch):
        key=(font.size,ch)
        if key not in self.glyphs:
            idx=len(self.glyphs);self.glyphs[key]=idx
            mask=Image.new('L',(20,20));ImageDraw.Draw(mask).text((0,0),ch,font=font,fill=255,anchor='lt')
            with (self.folder/'GLYPHS.BIN').open('ab') as f:f.write(mask.tobytes())
        return self.glyphs[key]

    def run(self,commands):
        data=b''.join(commands);n=len(commands)
        if n>256:raise ValueError('Display list exceeds 256 commands; split the dialogue node')
        self.runs.append(struct.pack('<II',self.offset,n));self.offset+=len(data)
        with (self.folder/'SCENES.BIN').open('ab') as f:f.write(data)


class Pen:
    def __init__(self,lib,preview=None):
        self.lib=lib; self.commands=[];self.image=preview;self.preview=ImageDraw.Draw(preview) if preview is not None else None

    def rectangle(self,box,fill):
        x,y,r,b=box;a=fill[3] if isinstance(fill,tuple) and len(fill)==4 else 255
        self.commands.append(struct.pack('<6H',0,x,y,r-x+1,b-y+1,color(fill))+struct.pack('<I',a))
        if self.preview:
            if self.image.mode=='RGBA' and a<255:
                self.image.alpha_composite(Image.new('RGBA',(r-x+1,b-y+1),fill),(x,y))
            else:self.preview.rectangle(box,fill=fill)

    def textlength(self,s,font):return font.getlength(s)

    def text(self,pos,s,font,fill,anchor):
        if self.preview:self.preview.text(pos,s,font=font,fill=fill,anchor=anchor)
        x,y=pos
        for ch in s:
            self.commands.append(struct.pack('<6HI',1,round(x),y,20,20,color(fill),self.lib.glyph(font,ch)))
            x+=font.getlength(ch)
