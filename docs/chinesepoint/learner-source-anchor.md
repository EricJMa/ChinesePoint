# Learner source anchor

A saved word records where it was read as a `TextAnchor`
(`src/chinesepoint/cjk/CjkLearnerModel.h`) and exports it under `source`:

| Field | Meaning |
| --- | --- |
| `spine_index` | EPUB spine item (chapter file) |
| `visible_codepoint_offset` | Zero-based Unicode codepoints from the start of that file's `<body>` text |
| `codepoint_length` | Length of the selected word |
| `fingerprint` | FNV-1a hash of the selected word's UTF-8 |

The offset uses the same visible-text count as the per-page offset LUT in
`section.bin`, so it does not depend on font, size, margins or orientation.

## Exact and estimated parts

Each `PageLine` stores the exact visible offset of its first word
(`section.bin` version 50). Table cells store their own offset rather than the
row's. Word selection then estimates each later word on the line with
`followingWordOffset`: words are assumed contiguous and separated by one space
unless both sides are CJK.

The estimate is exact for single-spaced text and contiguous CJK. It drifts
where the source has text that layout does not keep as words:

- collapsed whitespace (runs of spaces, newlines and indentation in the
  XHTML) makes later words on the line read early by the extra count;
- an English word directly followed by Hanzi, with no space, reads one late;
- synthetic list markers and right-to-left lines are not modelled.

Because wrapping decides which word starts a line, the same occurrence can be
recorded at slightly different offsets under different layouts. The tests in
`test/chapter_html_slim_parser` pin this: a word after two collapsed spaces is
exact when it starts a line and two codepoints early mid-line.

## Resolving an occurrence later

Treat the offset as "near here", not as an identity. A future jump-back should
open `spine_index`, search outward from `visible_codepoint_offset` for the
saved word (`codepoint_length`, `fingerprint`), and prefer the occurrence whose
surrounding text matches the saved sentence. Repeated words are common; the
fingerprint identifies the word, not the occurrence.
