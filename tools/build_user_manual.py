#!/usr/bin/env python3
"""Build the illustrated PDF from Docs/UserManual.md and real JUCE screenshots."""
from pathlib import Path
import re, subprocess, os
from xml.sax.saxutils import escape
from PIL import Image as PILImage
from reportlab.pdfgen import canvas
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.graphics.shapes import Drawing, Rect, String, Line, Polygon
from reportlab.graphics import renderPDF, renderSVG

ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'Docs'
OUT=ROOT/'output/pdf/OpenPreamp-User-Manual.pdf'
NAVY=colors.HexColor('#101a30'); RED=colors.HexColor('#a32323'); GREY=colors.HexColor('#526078')

def diagram(name, rows):
    d=Drawing(900,len(rows)*112+28)
    y=d.height-90
    for ri,(heading,labels) in enumerate(rows):
        d.add(String(12,y+67,heading,fontName='Helvetica-Bold',fontSize=16,fillColor=NAVY))
        n=len(labels); gap=18; w=(876-gap*(n-1))/n
        for i,label in enumerate(labels):
            x=12+i*(w+gap)
            d.add(Rect(x,y,w,53,rx=6,ry=6,fillColor=NAVY if ri%2==0 else RED,strokeColor=None))
            lines=label.split('|')
            for j,line in enumerate(lines):d.add(String(x+w/2,y+32-j*17,line,fontName='Helvetica',fontSize=14,textAnchor='middle',fillColor=colors.white))
            if i<n-1:
                end=x+w+gap-3;d.add(Line(x+w+3,y+26,end,y+26,strokeColor=GREY,strokeWidth=2));d.add(Polygon([end,y+26,end-5,y+30,end-5,y+22],fillColor=GREY,strokeColor=None))
        y-=112
    renderSVG.drawToFile(d,str(DOC/(name+'.svg')))
    temp=ROOT/'build-release/manual-qa';temp.mkdir(parents=True,exist_ok=True)
    renderPDF.drawToFile(d,str(temp/(name+'.pdf')))
    config=temp/'fonts.conf';config.write_text('<fontconfig><dir>/System/Library/Fonts</dir><dir>/System/Library/Fonts/Supplemental</dir><cachedir>'+str(temp/'font-cache')+'</cachedir></fontconfig>');env=dict(os.environ,FONTCONFIG_FILE=str(config));os.environ['FONTCONFIG_FILE']=str(config)
    subprocess.run(['pdftoppm','-r','144','-singlefile','-png',str(temp/(name+'.pdf')),str(DOC/name)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=env)

def assets():
    source=PILImage.open('/private/tmp/openpreamp-100-preview.png')
    for name,box in [('meters',(16,68,492,278)),('channels',(16,306,492,524)),('preamp',(628,540,984,704)),('cuts',(16,540,598,704))]:
        source.crop(box).save(DOC/f'OpenPreamp-1.0.0-{name}.png')
    for suffix in ['preview','ms','linked','rms','small']:
        path=Path(f'/private/tmp/openpreamp-100-{suffix}.png')
        if path.exists():(DOC/f'OpenPreamp-1.0.0-{suffix}.png').write_bytes(path.read_bytes())
    PILImage.open(DOC/'OpenPreamp-1.0.0-ms.png').crop((395,6,615,45)).save(DOC/'OpenPreamp-1.0.0-ms-switch.png')
    diagram('OpenPreamp-SignalFlow',[
        ('1  Session rate: input tools',['DAW L/R input','Optional M/S|encoder','Optional Side HP|mono maker','Shared HP / LP|+ input gain / PAD']),
        ('2  Choose the nonlinear path',['CIRCUIT ON|2x or HQ 4x','Upsampling|IIR or FIR','Full component|network solve','Downsample to|session rate']),
        ('   Alternative path',['CIRCUIT OFF','Light preamp|with ADAA','At session rate','To output tools']),
        ('3  Session rate: output tools',['Fixed latency|padding','Channel output|trims / meters','Optional M/S|decoder','DAW L/R output'])])
    diagram('OpenPreamp-Modeling',[
        ('Preparation',['Build network','Find DC|operating point','Calibrate reference|gain near 1 kHz']),
        ('Each circuit sample',['Input amplitude|to voltage','Solve currents and|node voltages','Output voltage|to amplitude']),
        ('State and nonlinearity',['Capacitor and|inductor memory','Transistors /|saturating cores','Newton iterations|from previous state'])])
    diagram('OpenPreamp-Topologies',[
        ('N-Type',['Input transformer|+ coupling','Three-transistor|feedback stage','Class-A|output driver','Output transformer|+ load / Zobel']),
        ('Brit',['Balanced input|+ RF networks','Matched pair /|instrumentation','Difference amp|+ DC servos','Balanced|line driver']),
        ('FSF',['Input transformer|+ gain ladder','Op-amp stages|+ coupling','Class-AB|output pair','Transformer|feedback / load']),
        ('A-Type',['Input|transformer','Amplifier with|T feedback','Coupling|network','Output transformer|+ load'])])

styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyManual',fontName='Helvetica',fontSize=10.1,leading=14.5,textColor=NAVY,spaceAfter=10))
styles.add(ParagraphStyle(name='TitleManual',fontName='Helvetica-Bold',fontSize=30,leading=36,textColor=NAVY,spaceAfter=20))
styles.add(ParagraphStyle(name='HeadingManual',fontName='Helvetica-Bold',fontSize=20,leading=25,textColor=NAVY,spaceAfter=16))
styles.add(ParagraphStyle(name='CellManual',fontName='Helvetica',fontSize=9,leading=12,textColor=NAVY))
styles.add(ParagraphStyle(name='CaptionManual',fontName='Helvetica-Oblique',fontSize=8.5,leading=12,textColor=GREY,alignment=TA_CENTER,spaceAfter=12))

def inline(text):
    text=escape(text)
    text=re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',text)
    return text

def furniture(c,doc):
    c.saveState();w,h=doc.pagesize
    c.setFillColor(NAVY);c.rect(0,h-34,w,34,fill=1,stroke=0)
    c.setFillColor(colors.white);c.setFont('Helvetica-Bold',10);c.drawString(43,h-22,'OpenPreamp 1.0.0  /  USER MANUAL')
    c.setStrokeColor(colors.HexColor('#d5dbe4'));c.line(43,35,w-43,35)
    c.setFillColor(GREY);c.setFont('Helvetica',8);c.drawString(43,23,'OpenGrid  /  Marcos Deida');c.drawRightString(w-43,23,str(doc.page));c.restoreState()

def build():
    assets();OUT.parent.mkdir(parents=True,exist_ok=True)
    doc=SimpleDocTemplate(str(OUT),pagesize=(595.28,841.89),rightMargin=43,leftMargin=43,topMargin=53,bottomMargin=48,title='OpenPreamp 1.0.0 User Manual',author='Marcos Deida / OpenGrid')
    story=[];lines=(DOC/'UserManual.md').read_text().splitlines();i=0
    while i<len(lines):
        line=lines[i].strip()
        if not line:i+=1;continue
        if line=='---':story.append(PageBreak());i+=1;continue
        if line.startswith('# '):story.append(Paragraph(inline(line[2:]),styles['TitleManual']));i+=1;continue
        if line.startswith('## '):story.append(Paragraph(inline(line[3:]),styles['HeadingManual']));i+=1;continue
        image=re.match(r'!\[(.*?)\]\((.*?)\)',line)
        if image:
            file=DOC/image.group(2);im=PILImage.open(file);w,h=im.size;maxh=335 if image.group(2).endswith('preview.png') else (340 if image.group(2).startswith(('OpenPreamp-Signal','OpenPreamp-Model','OpenPreamp-Topo')) else 220)
            scale=min(doc.width/w,maxh/h)
            story += [Image(str(file),width=w*scale,height=h*scale),Spacer(1,5),Paragraph(inline(image.group(1)),styles['CaptionManual'])];i+=1;continue
        if line.startswith('|'):
            rows=[]
            while i<len(lines) and lines[i].strip().startswith('|'):
                row=[v.strip() for v in lines[i].strip().strip('|').split('|')]
                if not all(re.match(r'^:?-+:?$',v) for v in row):rows.append(row)
                i+=1
            n=len(rows[0]);widths=([110,125,doc.width-235] if n==3 else [160,doc.width-160])
            table=Table([[Paragraph(inline(v),styles['CellManual']) for v in row] for row in rows],colWidths=widths,repeatRows=1,hAlign='LEFT')
            table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#e8edf4')),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),('LINEBELOW',(0,0),(-1,-1),.3,colors.HexColor('#d6dce5'))]))
            story += [table,Spacer(1,14)];continue
        paragraph=[line];i+=1
        while i<len(lines) and lines[i].strip() and not lines[i].startswith(('#','|','![')) and lines[i].strip()!='---':paragraph.append(lines[i].strip());i+=1
        story.append(Paragraph(inline(' '.join(paragraph)),styles['BodyManual']))
    doc.build(story,onFirstPage=furniture,onLaterPages=furniture)
    print(OUT)
if __name__=='__main__':build()
