"""Source Han Sans Bold with explicit mainland Chinese glyph selection."""
from io import BytesIO
import unicodedata
from fontTools.ttLib import TTFont
from PIL import ImageFont

FONT_FILE = 'sourcehansans-bold.otf'
# Unambiguous orthographic variants only; never apply wholesale NFKC to prose.
COMMON_FORMS = str.maketrans({'裏':'里', '裡':'里', '爲':'为', '麽':'么', '綫':'线'})


def common_text(text):
    result=[]
    for c in text.translate(COMMON_FORMS):
        cp=ord(c)
        if 0xF900 <= cp <= 0xFAFF or 0x2F800 <= cp <= 0x2FA1F:
            c=unicodedata.normalize('NFC', c)
        result.append(c)
    return ''.join(result)


def chinese_bold(root, size=14, chars=()):
    font=TTFont(root/'fonts'/FONT_FILE)
    table=font['GSUB'].table
    script=next(r.Script for r in table.ScriptList.ScriptRecord if r.ScriptTag=='hani')
    language=next(r.LangSys for r in script.LangSysRecord if r.LangSysTag=='ZHS ')
    mappings=[]
    for index in language.FeatureIndex:
        feature=table.FeatureList.FeatureRecord[index]
        if feature.FeatureTag!='locl':continue
        for lookup_id in feature.Feature.LookupListIndex:
            lookup=table.LookupList.Lookup[lookup_id]
            mapping={}
            for sub in lookup.SubTable:
                if lookup.LookupType==7:sub=sub.ExtSubTable
                if not hasattr(sub,'mapping'):raise ValueError('Unsupported Chinese glyph substitution')
                mapping.update(sub.mapping)
            mappings.append(mapping)
    if not mappings:raise ValueError('Font has no simplified Chinese glyph selection')
    for cmap in font['cmap'].tables:
        if not cmap.isUnicode():continue
        for cp,glyph in list(cmap.cmap.items()):
            for mapping in mappings:glyph=mapping.get(glyph,glyph)
            cmap.cmap[cp]=glyph
    missing=sorted({ord(c) for c in chars if ord(c)>=32}-font.getBestCmap().keys())
    if missing:raise ValueError(f'Missing Bold glyphs: {[hex(c) for c in missing]}')
    data=BytesIO();font.save(data);font.close();data.seek(0)
    return ImageFont.truetype(data,size,layout_engine=ImageFont.Layout.BASIC)
