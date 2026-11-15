#!/usr/bin/env python3
"""Convert a saved documentation HTML page to plain text for verbatim quoting.

Usage: python3 -I html2text.py <in.html> <out.txt> [--links]
Block elements become line breaks, inline elements are joined, scripts and
styles are dropped, entities are decoded. Superscripts and subscripts are
kept visible as ^{...} and _{...} so that footnote markers and exponents are
never glued to the text (2^{16}-1, not 216-1). With --links, every <a href> is
also printed to stdout (absolute or relative as in the page).
"""
import html.parser
import re
import sys

BLOCK = {"p", "div", "li", "ul", "ol", "dt", "dd", "dl", "h1", "h2", "h3", "h4", "h5",
         "h6", "pre", "table", "tr", "br", "section", "blockquote", "td", "th", "caption",
         "figcaption", "hr", "article", "header", "footer", "nav"}
SKIP = {"script", "style", "noscript"}


class P(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.out = []
        self.skip = 0
        self.pre = 0
        self.links = []

    def handle_starttag(self, tag, attrs):
        if tag in SKIP:
            self.skip += 1
        if tag == "pre":
            self.pre += 1
        if tag in BLOCK:
            self.out.append("\n")
        if tag in ("td", "th"):
            self.out.append(" | ")
        if tag in ("sup", "sub"):
            self.out.append("^{" if tag == "sup" else "_{")
        if tag == "a":
            for k, v in attrs:
                if k == "href" and v:
                    self.links.append(v)

    def handle_endtag(self, tag):
        if tag in ("sup", "sub"):
            self.out.append("}")
        if tag in SKIP and self.skip:
            self.skip -= 1
        if tag == "pre" and self.pre:
            self.pre -= 1
        if tag in BLOCK:
            self.out.append("\n")

    def handle_data(self, data):
        if self.skip:
            return
        if self.pre:
            self.out.append(data)
        else:
            self.out.append(re.sub(r"\s+", " ", data))


def main():
    src, dst = sys.argv[1], sys.argv[2]
    p = P()
    p.feed(open(src, encoding="utf-8", errors="replace").read())
    text = "".join(p.out)
    text = "\n".join(l.rstrip() for l in text.split("\n"))
    text = re.sub(r"\n{3,}", "\n\n", text)
    open(dst, "w", encoding="utf-8").write(text)
    if "--links" in sys.argv:
        seen = set()
        for l in p.links:
            if l not in seen:
                seen.add(l)
                print(l)


if __name__ == "__main__":
    main()
