"""Build QQEasyTool 1.08 bilingual PDF manuals from the adjacent JSON content."""
from pathlib import Path
from xml.sax.saxutils import escape
import math,json
from reportlab.platypus import SimpleDocTemplate,Paragraph,Spacer,PageBreak,KeepTogether,Flowable
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from pypdf import PdfReader
ROOT=Path(__file__).parent
pdfmetrics.registerFont(TTFont('CJK',r'C:\Windows\Fonts\msyh.ttc',subfontIndex=0))
pdfmetrics.registerFont(TTFont('CJKB',r'C:\Windows\Fonts\msyhbd.ttc',subfontIndex=0))
INK=colors.HexColor('#192D35');TEAL=colors.HexColor('#126D71');MUTED=colors.HexColor('#52656D');PALE=colors.HexColor('#EAF3F3')
W,H=595.276,841.89
class Diagram(Flowable):
    def __init__(self,zh,kind):Flowable.__init__(self);self.width=499;self.height=105;self.zh=zh;self.kind=kind
    def draw(self):
        c=self.canv;c.setFillColor(PALE);c.roundRect(0,0,self.width,self.height,7,fill=1,stroke=0)
        c.setFillColor(MUTED);c.setFont('CJK' if self.zh else 'Helvetica',8)
        c.drawString(13,90,'操作示意 / 非界面截图' if self.zh else 'Operation diagram / not a screenshot')
        if self.kind=='flow':
            labels=['载入','分析','检查','处理','保存'] if self.zh else ['Load','Analyze','Review','Process','Save']
            for i,label in enumerate(labels):
                x=12+i*98;c.setFillColor(TEAL);c.roundRect(x,30,82,35,5,fill=1,stroke=0);c.setFillColor(colors.white);c.setFont('CJK' if self.zh else 'Helvetica-Bold',11);c.drawCentredString(x+41,43,label)
                if i<4:c.setFillColor(MUTED);c.drawString(x+87,43,'>')
        else:
            c.setFillColor(INK);c.roundRect(12,14,475,64,4,fill=1,stroke=0)
            for x,w,color,label in [(35,85,'#3F9682','Breath'),(220,60,'#B78852','Noise'),(340,105,'#5279A5','Others')]:
                c.setFillColor(colors.HexColor(color));c.rect(x,15,w,62,fill=1,stroke=0);c.setFillColor(colors.white);c.setFont('Helvetica',8);c.drawCentredString(x+w/2,62,label)
            c.setStrokeColor(colors.HexColor('#E0E8E8'));c.setLineWidth(.65);p=c.beginPath()
            for i in range(470):
                x=15+i;y=38+math.sin(i*1.8)*math.sin(i*.08)*15
                (p.moveTo if i==0 else p.lineTo)(x,y)
            c.drawPath(p)
def build(lang,pages):
    zh=lang=='ZH-CN';font='CJK' if zh else 'Helvetica';bold='CJKB' if zh else 'Helvetica-Bold'
    st={
      'title':ParagraphStyle('title',fontName=bold,fontSize=25,leading=32,textColor=INK,spaceAfter=10),
      'deck':ParagraphStyle('deck',fontName=font,fontSize=10.2,leading=15,textColor=MUTED,spaceAfter=18),
      'head':ParagraphStyle('head',fontName=bold,fontSize=12.2,leading=18,textColor=TEAL,spaceBefore=12,spaceAfter=6),
      'body':ParagraphStyle('body',fontName=font,fontSize=9.9,leading=16,textColor=INK,spaceAfter=7,wordWrap='CJK' if zh else None)}
    target=ROOT/f'QQEasyTool-1.08-User-Manual-{lang}.pdf'
    def chrome(c,doc):
        c.setTitle('QQEasyTool 1.08 - '+('用户说明书' if zh else 'User Manual'));c.setAuthor('Qing Audio / Ziqing Gu')
        c.setStrokeColor(TEAL);c.setLineWidth(2);c.line(48,H-41,W-48,H-41)
        c.setFont('Helvetica-Bold',9);c.setFillColor(TEAL);c.drawString(48,H-31,'QING AUDIO  /  QQEasyTool')
        c.setFont('Helvetica',8);c.setFillColor(MUTED);c.drawRightString(W-48,H-31,'1.08 STABLE')
        c.setLineWidth(.4);c.setStrokeColor(colors.HexColor('#CCD9DA'));c.line(48,42,W-48,42)
        c.setFont(font,8);c.drawString(48,28,'QQEasyTool 1.08 | 2026-10-27');c.drawRightString(W-48,28,f'{doc.page} / 8')
    doc=SimpleDocTemplate(str(target),pagesize=(W,H),leftMargin=48,rightMargin=48,topMargin=66,bottomMargin=58)
    story=[]
    for i,(title,deck,sections) in enumerate(pages):
        story += [Paragraph(escape(title),st['title']),Paragraph(escape(deck),st['deck'])]
        if i in (0,4):story+=[Diagram(zh,'flow' if i==0 else 'wave'),Spacer(1,6)]
        for head,paras in sections:
            ps=[Paragraph(escape(p),st['body']) for p in paras]
            story.append(KeepTogether([Paragraph(escape(head),st['head']),ps[0]]));story.extend(ps[1:])
        if i<len(pages)-1:story.append(PageBreak())
    doc.build(story,onFirstPage=chrome,onLaterPages=chrome)
    reader=PdfReader(target);assert len(reader.pages)==8,(lang,len(reader.pages))
    texts=[p.extract_text() for p in reader.pages];assert all(len(t)>150 for t in texts)
    assert 'Others.wav' in texts[6] and '64' in texts[6]
    return {'path':str(target),'pages':len(reader.pages),'bytes':target.stat().st_size,'language':lang}
if __name__=='__main__':
    data={lang:json.loads((ROOT/f'manual-{lang}-1.08.json').read_text(encoding='utf-8')) for lang in ['ZH-CN','EN']}
    print(json.dumps([build(lang,pages) for lang,pages in data.items()],ensure_ascii=False,indent=2))
