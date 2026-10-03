#!/usr/bin/env python3
"""Build an offline technical reading fixture; no downloads or device writes."""
import argparse
import json
from pathlib import Path
import struct
from xml.sax.saxutils import escape
import zipfile

POEM = ["床前明月光，", "疑是地上霜。", "举头望明月，", "低头思故乡。"]
SOURCE = "https://zh.wikisource.org/w/index.php?title=靜夜思&oldid=8747585"
# Authored smoke-test glosses, not a replacement for a complete Chinese dictionary.
WORDS = {
    "床": "chuang2 / bed (technical fixture gloss)",
    "前": "qian2 / in front; before (technical fixture gloss)",
    "明月": "ming2 yue4 / bright moon (technical fixture gloss)",
    "月光": "yue4 guang1 / moonlight (technical fixture gloss)",
    "月": "yue4 / moon (technical fixture gloss)",
    "光": "guang1 / light (technical fixture gloss)",
    "疑": "yi2 / suspect; wonder (technical fixture gloss)",
    "是": "shi4 / be (technical fixture gloss)",
    "地上": "di4 shang4 / on the ground (technical fixture gloss)",
    "霜": "shuang1 / frost (technical fixture gloss)",
    "举头": "ju3 tou2 / raise one's head (technical fixture gloss)",
    "望": "wang4 / gaze toward (technical fixture gloss)",
    "低头": "di1 tou2 / lower one's head (technical fixture gloss)",
    "思": "si1 / think of (technical fixture gloss)",
    "故乡": "gu4 xiang1 / hometown (technical fixture gloss)",
}


def write_epub(path, title, language, body, author):
    container = '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
    opf = f'''<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">urn:chinesepoint:fixture:{path.stem}</dc:identifier><dc:title>{escape(title)}</dc:title><dc:creator>{escape(author)}</dc:creator><dc:language>{language}</dc:language><dc:source>{escape(SOURCE)}</dc:source></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest><spine toc="ncx"><itemref idref="chapter"/></spine></package>'''
    ncx = f'<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head/><docTitle><text>{escape(title)}</text></docTitle><navMap><navPoint id="chapter" playOrder="1"><navLabel><text>{escape(title)}</text></navLabel><content src="chapter.xhtml"/></navPoint></navMap></ncx>'
    chapter = f'<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="{language}"><head><title>{escape(title)}</title></head><body>{body}</body></html>'
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w") as z:
        for name, data in [("mimetype", "application/epub+zip"), ("META-INF/container.xml", container), ("OEBPS/content.opf", opf), ("OEBPS/toc.ncx", ncx), ("OEBPS/chapter.xhtml", chapter)]:
            info = zipfile.ZipInfo(name, (2026, 10, 3, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED if name == "mimetype" else zipfile.ZIP_DEFLATED
            z.writestr(info, data.encode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="isolated simulator SD root")
    args = parser.parse_args()
    root = args.destination
    body = '<h1>静夜思</h1><p>李白</p>' + ''.join(f'<p>{line}</p>' for line in POEM)
    write_epub(root / "books/01-jing-ye-si.epub", "静夜思 · ChinesePoint fixture", "zh", body, "李白")
    english = '<h1>English reading check</h1><p>This is an original technical sample for ChinesePoint. Ordinary English paragraphs should still render with the selected font.</p><p>A quiet page, a clear sentence, and a book that remembers where you stopped.</p>'
    write_epub(root / "books/02-english-check.epub", "English reading check", "en", english, "ChinesePoint local test fixture")
    directory = root / "dictionaries/zh/Fixture"
    directory.mkdir(parents=True, exist_ok=True)
    definitions, index = bytearray(), bytearray()
    for word in sorted(WORDS, key=lambda value: value.encode("utf-8")):
        text = WORDS[word].encode("utf-8")
        index.extend(word.encode("utf-8") + b'\0' + struct.pack('>II', len(definitions), len(text)))
        definitions.extend(text)
    (directory / "fixture.dict").write_bytes(definitions)
    (directory / "fixture.idx").write_bytes(index)
    (directory / "fixture.ifo").write_text(f"StarDict's dict ifo file\nversion=2.4.2\nwordcount={len(WORDS)}\nidxfilesize={len(index)}\nbookname=ChinesePoint technical fixture\nsametypesequence=m\n", encoding="utf-8")
    settings = root / ".crosspoint"
    settings.mkdir(parents=True, exist_ok=True)
    (settings / "settings.json").write_text(json.dumps({"fontSize": 16, "sdFontFamilyName": "CPFixture", "dictionaryName": "Fixture", "autoSleepTimeout": 0}) + '\n')
    (settings / "state.json").write_text(json.dumps({"openEpubPath": "/books/01-jing-ye-si.epub", "lastSleepFromReader": True, "showBootScreen": False}) + '\n')
    glyphs = ''.join(POEM) + '静夜思李白一·…'
    intervals = 'ascii,' + ','.join(f'(0x{cp:X}-0x{cp:X})' for cp in sorted({ord(c) for c in glyphs}))
    (root / "font-intervals.txt").write_text(intervals + '\n')
    (root / "fixture-provenance.json").write_text(json.dumps({"poem": "静夜思", "author": "李白", "edition": "唐诗三百首 variant, simplified characters", "source": SOURCE, "poemLicense": "Public domain", "dictionary": "15 authored technical fixture glosses; CC0-1.0", "englishSample": "Original technical test text; CC0-1.0", "font": "Noto Sans SC Sans2.004, converted subset; SIL OFL 1.1", "purpose": "Rendering, word selection, lookup, save, review and export smoke tests. Not a graded reader or validated curriculum."}, ensure_ascii=False, indent=2) + '\n')
    print(f"Built Chinese/English EPUBs and {len(WORDS)}-entry dictionary at {root}")


if __name__ == "__main__":
    main()
