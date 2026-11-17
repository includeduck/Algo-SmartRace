"""Assemble the ten report chapters using existing measured figures and GUI captures.
Requires Python 3, reportlab and Pillow. Run from the repository root.
"""
from pathlib import Path
from xml.sax.saxutils import escape
import re
import textwrap

from PIL import Image as PillowImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, PageBreak,
    LongTable, TableStyle, Image, Preformatted, KeepTogether
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "report/24I-3071_24I-3000_BSE-5B_Report.pdf"
FONT_DIR = Path("C:/Windows/Fonts")
for name, filename in [("Report", "arial.ttf"), ("Report-Bold", "arialbd.ttf"),
                       ("Report-Italic", "ariali.ttf"), ("Report-BoldItalic", "arialbi.ttf")]:
    pdfmetrics.registerFont(TTFont(name, str(FONT_DIR / filename)))
pdfmetrics.registerFontFamily("Report", normal="Report", bold="Report-Bold",
                              italic="Report-Italic", boldItalic="Report-BoldItalic")
styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name="Body", fontName="Report", fontSize=9.5, leading=14,
                          spaceAfter=7, textColor=colors.HexColor("#203046")))
styles.add(ParagraphStyle(name="ReportTitle", parent=styles["Body"], fontName="Report-Bold",
                          fontSize=27, leading=34, spaceAfter=24, alignment=TA_CENTER))
for name, size, leading in [("Section", 19, 25), ("Subsection", 13, 18), ("SmallHeading", 11, 16)]:
    styles.add(ParagraphStyle(name=name, parent=styles["Body"], fontName="Report-Bold",
                              fontSize=size, leading=leading, spaceBefore=14, spaceAfter=9,
                              keepWithNext=True, textColor=colors.HexColor("#123D58")))
styles.add(ParagraphStyle(name="Cell", parent=styles["Body"], fontSize=7.2, leading=10, spaceAfter=0))
styles.add(ParagraphStyle(name="Caption", parent=styles["Body"], fontSize=8.5, leading=12,
                          textColor=colors.HexColor("#53657A"), spaceBefore=7, spaceAfter=15))
styles.add(ParagraphStyle(name="CodeText", fontName="Courier", fontSize=7.1, leading=9.3,
                          spaceBefore=6, spaceAfter=8, backColor=colors.HexColor("#F0F3F6")))
WIDTH = 483.3

def inline(value):
    value = escape(value).replace("&amp;#39;", "'")
    codes = []
    def protect(match):
        codes.append('<font name="Courier">' + match.group(1) + "</font>")
        return "CODETOKEN%dEND" % (len(codes) - 1)
    value = re.sub(r"`([^`]+)`", protect, value)
    value = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", value)
    value = re.sub(r"(?<![\w*])\*([^*\s](?:[^*]*[^*\s])?)\*(?![\w*])", r"<i>\1</i>", value)
    for index, code in enumerate(codes):
        value = value.replace("CODETOKEN%dEND" % index, code)
    value = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1 (\2)", value)
    return value

class ReportDoc(BaseDocTemplate):
    def __init__(self):
        super().__init__(str(OUT), pagesize=(595.3, 841.9), leftMargin=56, rightMargin=56,
                         topMargin=60, bottomMargin=52, title="Smart Racing Championship",
                         author="Muhammad Wasay Tariq and Syed Muhammad Rayyan Hassan")
        self.addPageTemplates(PageTemplate(id="body", frames=[
            Frame(56, 52, WIDTH, 729.9, leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        ], onPage=self.chrome))
        self.heading_index = 0

    def beforeDocument(self):
        self.heading_index = 0

    def chrome(self, canvas, doc):
        canvas.saveState()
        canvas.setFillColor(colors.HexColor("#53657A"))
        canvas.setFont("Report", 8)
        canvas.drawString(56, 811, "SMART RACING CHAMPIONSHIP  |  DESIGN AND ANALYSIS OF ALGORITHMS")
        canvas.setStrokeColor(colors.HexColor("#CCD7DF"))
        canvas.line(56, 800, 539.3, 800)
        canvas.drawString(56, 29, "24I-3071 / 24I-3000  |  BSE-5B  |  Review edition, 17 November 2026")
        canvas.drawRightString(539.3, 29, str(doc.page))
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name in ("Section", "Subsection"):
            if flowable.getPlainText() in ("Contents", "The Algorithmic Racing Challenge"):
                return
            level = 0 if flowable.style.name == "Section" else 1
            self.heading_index += 1
            key = "section-%d" % self.heading_index
            self.canv.bookmarkPage(key)
            self.notify("TOCEntry", (level, flowable.getPlainText(), self.page, key))

def table(lines):
    rows = [[cell.strip() for cell in line.strip().strip("|").split("|")] for line in lines]
    rows = [row for row in rows if not all(re.match(r"^:?-+:?$", cell.replace(" ", "")) for cell in row)]
    count = max(map(len, rows))
    rows = [row + [""] * (count - len(row)) for row in rows]
    weights = [max(12, min(65, max(len(row[c]) for row in rows))) for c in range(count)]
    widths = [WIDTH * weight / sum(weights) for weight in weights]
    data = [[Paragraph(inline(cell), styles["Cell"]) for cell in row] for row in rows]
    result = LongTable(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    result.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#DCE8EF")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#BDCCD7")),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return result

def markdown(text, title=False):
    output = []
    lines = text.splitlines()
    pos = 0
    while pos < len(lines):
        line = lines[pos].strip()
        if not line or re.match(r"^---+$", line):
            pos += 1
            continue
        if line.startswith(("```", "~~~")):
            marker = line[:3]
            pos += 1
            code = []
            while pos < len(lines) and not lines[pos].strip().startswith(marker):
                code.extend(textwrap.wrap(lines[pos], width=94, replace_whitespace=False,
                                          drop_whitespace=False) or [""])
                pos += 1
            output.append(Preformatted("\n".join(code), styles["CodeText"]))
            pos += 1
            continue
        if line.startswith("|"):
            cells = []
            while pos < len(lines) and lines[pos].strip().startswith("|"):
                cells.append(lines[pos])
                pos += 1
            output.extend([table(cells), Spacer(1, 10)])
            continue
        picture = re.match(r"^!\[(.+)\]\((.+)\)$", line)
        if picture:
            # Markdown image, path relative to report/; drawn full width, up to most of a page.
            output.append(figure(ROOT / "report" / picture.group(2), picture.group(1), max_height=560))
            pos += 1
            continue
        heading = re.match(r"^(#{1,6})\s+(.+)$", line)
        if heading:
            level = len(heading.group(1))
            name = "ReportTitle" if title and level == 1 else ("Section" if level == 1 else ("Subsection" if level == 2 else "SmallHeading"))
            output.append(Paragraph(inline(heading.group(2)), styles[name]))
            pos += 1
            continue
        bullet = re.match(r"^(?:[-*]|\d+\.)\s+(.+)$", line)
        if bullet:
            output.append(Paragraph("&#8226; " + inline(bullet.group(1)), styles["Body"]))
            pos += 1
            continue
        paragraph = [line]
        pos += 1
        while pos < len(lines):
            next_line = lines[pos].strip()
            if not next_line or next_line.startswith(("#", "|", "```", "~~~")) or re.match(r"^(?:[-*]|\d+\.)\s+", next_line):
                break
            paragraph.append(next_line)
            pos += 1
        output.append(Paragraph(inline(" ".join(paragraph)), styles["Body"]))
    return output

def figure(path, caption, max_height=294):
    with PillowImage.open(path) as image:
        width, height = image.size
    ratio = min(WIDTH / width, max_height / height)
    return KeepTogether([Image(str(path), width=width*ratio, height=height*ratio),
                         Paragraph(inline(caption), styles["Caption"])])

def build():
    chapters = []
    for index in range(1, 11):
        matches = list((ROOT / "report").glob(f"{index:02d}_*.md"))
        if len(matches) != 1:
            raise RuntimeError(f"Expected exactly one chapter {index}, found {matches}")
        chapters.append(matches[0])
    story = markdown(chapters[0].read_text(encoding="utf-8"), title=True)
    story.append(PageBreak())
    story.append(Paragraph("Contents", styles["Section"]))
    toc = TableOfContents()
    toc.levelStyles = [
        ParagraphStyle("toc0", fontName="Report-Bold", fontSize=10, leading=17, spaceBefore=5),
        ParagraphStyle("toc1", fontName="Report", fontSize=9, leading=14, leftIndent=15),
    ]
    story.extend([toc, PageBreak()])
    for index, chapter in enumerate(chapters[1:], 2):
        story.extend(markdown(chapter.read_text(encoding="utf-8")))
        if index == 3:
            story.append(Paragraph("Application rendering gallery", styles["Subsection"]))
            story.append(Paragraph("These images are generated by the application-level GUI smoke test using real backend data. They are not a completed manual usability matrix.", styles["Body"]))
            for image in sorted((ROOT / "report/screenshots").glob("*.png")):
                story.append(figure(image, "Figure 3." + image.stem[:2] + " - " + image.stem[3:].replace("_", " ") + ". Actual application framebuffer."))
        if index == 8:
            story.append(Paragraph("Measured experiment charts", styles["Subsection"]))
            chart_names = ["exp1_time_vs_size", "exp1_normalised", "exp2_expanded_by_preset",
                           "exp2_grid_scaling", "exp3_time", "exp3_greedy_quality",
                           "exp4_heap_ops", "exp4_ns_per_op", "exp5_comparisons", "exp5_topk"]
            for number, name in enumerate(chart_names, 1):
                path = ROOT / "results/charts" / (name + ".png")
                if not path.is_file():
                    raise FileNotFoundError(path)
                story.append(figure(path, f"Figure 8.{number} - {name.replace('_', ' ')}. Team measurements from committed result CSVs; no GUI rendering time is included."))
        if index < 10:
            story.append(PageBreak())
    ReportDoc().multiBuild(story)
    print(OUT)
    print(f"PDF size: {OUT.stat().st_size} bytes")

if __name__ == "__main__":
    build()
