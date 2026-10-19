#!/usr/bin/python3
"""Builds the help viewer page and the javascript index of its words, from a file of help contents."""

from __future__ import annotations

import argparse
import html
import io
import json
import re
from dataclasses import dataclass, field
from typing import Any, TextIO
from urllib.parse import urljoin

# Words in more than this amount of pages are ignored for indexing
INDEX_MAX_PAGE_RATIO = 0.75
# Combine roots with matched endings
INDEX_PLURAL_ENDINGS = [
    ["", "s"],
    ["y", "ies"],
]
TEMPLATE = """
<html>
<head>
<meta charset="utf-8">
<title><!--TITLE--></title>
<link rel="icon" href="favicon.ico">
<link rel="stylesheet" href="css/helpapp.css">
<style media="print">#menu, #resize-handle { display: none }</style>
<script src="js/jquery.min.js" ></script>
<!--SCRIPTS-->
<script src="js/helpapp.js" ></script>
</head>
</body>
<div class="container">
<div id="menu">
<div class="menu_header"><div id="show-contents-button" class="menu-button">Contents</div><div id="show-search-button" class="menu-button">Search</div></div><div class="menu_area"><div id="contents" class="contents">
<!--CONTENTS-->
</div>
<div id="search"></div></div>
</div>
<div id="resize-handle"></div>
<div id="page-content" class="frame">
<div id="breadcrumbs"></div>
<iframe id="help-page"></iframe>
</div>
</div>
</body>
</html>
"""

TITLE_FACTOR = 10
KEYWORD_FACTOR = 10
HEADER_FACTOR = 10


@dataclass(frozen=True)
class ContentsItem:
    """An entry of the help contents, with the entries that are below it."""

    level: str
    label: str
    href: str
    subitems: list[ContentsItem] = field(default_factory=list)


@dataclass
class WordEntry:
    """How often a word, or a group of equivalent words, is used in the help pages."""

    page_count: int = 0
    word_count: int = 0
    pages: list[list[int]] = field(default_factory=list)  # Each is [page number, word count in the page]

    def as_list(self) -> list[Any]:
        """The entry as the array that the javascript index holds."""
        return [self.page_count, self.word_count, self.pages]


@dataclass(frozen=True)
class HelpIndex:
    """The words of the help pages, with where they are used."""

    wordindex: dict[str, int]  # The number of the entry in index for each word
    index: list[WordEntry | None]  # None for a word that is in too many pages to be useful
    pages: list[dict[str, str]]


def main() -> None:
    """Writes the help page and the word index."""
    parser = argparse.ArgumentParser(description="Compile dynamic help file")
    parser.add_argument("contents_file", help="File describe help file contents")
    parser.add_argument("help_file", help="Generated help file")
    parser.add_argument("-b", "--base-dir", help="Base directory for help files")
    parser.add_argument("-t", "--title", default="SNAP help", help="Title of generated help file")
    parser.add_argument("-c", "--css-stylesheet", action="append", help="Included stylesheets")
    parser.add_argument("-s", "--script-file", action="append", help="Included javascript")
    parser.add_argument("-i", "--index-file", default="js/wordindex.js", help="Output javascript word index")
    args = parser.parse_args()
    stylesheets = args.css_stylesheet or []
    scripts = args.script_file or []
    stylesheets.append("css/helpapp.css")
    scripts.insert(0, "js/jquery.min.js")
    scripts.append("js/helpapp.js")
    basedir = args.base_dir + "/" if args.base_dir else ""
    helpfile = basedir + args.help_file
    indexfile = args.index_file

    levels, urls = load_contents(args.contents_file)
    help_index = build_index(basedir, urls)
    scripts.append("defer:" + indexfile)
    write_help_page(helpfile, levels, stylesheets, scripts, args.title)
    write_word_index(basedir + indexfile, help_index)


def write_help_page(
    help_file: str, levels: list[list[ContentsItem]], stylesheets: list[str], scripts: list[str], title: str
) -> None:
    """Writes the help page, with the top level of the contents and the stylesheets and scripts that it uses."""
    if not levels[1]:
        raise ValueError("The help contents have no entries")

    with io.StringIO() as w:
        for item in levels[1]:
            write_contents_item(w, item)
        contenthtml = w.getvalue()

    with io.StringIO() as w:
        for css in stylesheets:
            w.write(f'<link rel="stylesheet" href="{css}">\n')
        stylesheethtml = w.getvalue()

    with io.StringIO() as w:
        for script in scripts:
            defer = ""
            if script.startswith("defer:"):
                defer = "defer"
                script = script[6:]
            w.write(f'<script src="{script}" {defer}></script>\n')
        scripthtml = w.getvalue()

    pagehtml = TEMPLATE
    pagehtml = pagehtml.replace("<!--TITLE-->", html.escape(title))
    pagehtml = pagehtml.replace("<!--CONTENTS-->", contenthtml)
    pagehtml = pagehtml.replace("<!--STYLESHEETS-->", stylesheethtml)
    pagehtml = pagehtml.replace("<!--SCRIPTS-->", scripthtml)

    with open(help_file, "wb") as th:
        th.write(pagehtml.encode("utf8"))


def load_contents(contents_file: str) -> tuple[list[list[ContentsItem]], list[str]]:
    """Reads the contents file, and returns the entries of each level, and the pages that it refers to.

    A line has the level, the label and, optionally, the page, separated by tabs. levels[n] holds the entries
    of the level n that are being added to, and levels[1] holds the top level entries. levels[0] is not used.
    """
    levels: list[list[ContentsItem]] = [[], []]
    level0 = 1
    urls = []
    with open(contents_file, encoding="utf-8") as cf:
        for line in cf:
            line = line.strip()
            if line == "":
                continue
            parts = line.split("\t")
            if len(parts) == 2:
                parts.append("")
            if len(parts) != 3:
                print(f'Warning: ignoring "{line}"')
                continue
            if parts[0] not in "1 2 3 4 5 6 7 8".split():
                print(f"Error: Invalid help level: {line}")
                continue
            if parts[2]:
                urls.append(parts[2])
            level = int(parts[0])
            if level > level0 + 1:
                print(f"Error: Level {level} after {level0}: {line}")
                continue
            if level == level0 + 1:
                if level == len(levels):
                    levels.append([])
                levels[level] = levels[level0][-1].subitems
            levels[level].append(ContentsItem(parts[0], parts[1], parts[2]))
            level0 = level
    return levels, urls


def write_contents_item(th: TextIO, item: ContentsItem) -> None:
    """Writes the entry of the contents, and the entries below it."""
    th.write(f'<div class="contents-level level-{item.level}">\n')
    if item.href:
        th.write(f'<div class="contents-item"><a href="{item.href}">{html.escape(item.label)}</a></div>\n')
    else:
        th.write(f'<div class="contents-item contents-group">{item.label}</div>\n')
    for subitem in item.subitems:
        write_contents_item(th, subitem)
    th.write("</div>\n")


def build_index(basedir: str, urls: list[str]) -> HelpIndex:
    """Indexes the words of the pages, and of the pages that they refer to."""
    allwords, pagedata = collect_words(basedir, urls)
    wordlist = sorted(allwords.keys())
    wordindex, index = merge_equivalent_words(allwords, wordlist, find_root_words(allwords, wordlist))
    return HelpIndex(wordindex, remove_common_words(wordindex, index, len(pagedata)), pagedata)


def collect_words(basedir: str, urls: list[str]) -> tuple[dict[str, WordEntry], list[dict[str, str]]]:
    """Returns the pages that use each word, and the url and title of each page.

    The pages are the urls, and the pages that they refer to by relative urls.
    """
    allwords: dict[str, WordEntry] = {}
    pagedata: list[dict[str, str]] = []
    todo = sorted(urls)
    done = set()
    while todo:
        url = todo.pop(0)
        if url in done:
            continue
        done.add(url)
        title, text, refs = process_page(basedir, url)
        words = index_text(text)
        npage = len(pagedata)
        pagedata.append({"url": url, "title": title})
        for word, count in words.items():
            # Skip things like numbers - this will screw up #commands etc.. To be reviewed
            if not re.match("^[#a-z]", word):
                continue
            if word not in allwords:
                allwords[word] = WordEntry()
            allwords[word].page_count += 1
            allwords[word].word_count += count
            allwords[word].pages.append([npage, count])
        for ref in refs:
            if re.match(r"^\w+\:\/", ref):
                print(f"Skipping non-relative url {ref}")
                continue
            ref = re.sub(r"[\#\?].*", "", ref)
            if ref not in done and ref not in todo:
                print(f"Adding non-indexed url {ref}")
                todo.append(ref)
    return allwords, pagedata


def find_root_words(allwords: dict[str, WordEntry], wordlist: list[str]) -> dict[str, str]:
    """Returns the words that are equivalent to another word, such as the singular and plural of a word.

    A word redirects from the alphabetically highest to the lowest of its equivalents.
    """
    rootword = {}
    for word in wordlist:
        for singular, plural in INDEX_PLURAL_ENDINGS:
            if word.endswith(singular):
                root = word if singular == "" else word[: -len(singular)]
                wordplural = root + plural
                if wordplural in allwords:
                    if word < wordplural:
                        rootword[wordplural] = word
                    else:
                        rootword[word] = wordplural
    return rootword


def merge_equivalent_words(
    allwords: dict[str, WordEntry], wordlist: list[str], rootword: dict[str, str]
) -> tuple[dict[str, int], list[WordEntry]]:
    """Merges the entries of equivalent words into one entry.

    Returns the number of the merged entry of each word, and the merged entries.
    """
    wordindex: dict[str, int] = {}
    index: list[WordEntry] = []
    for word in wordlist:
        root = rootword.get(word)
        if root:
            while root in rootword:
                root = rootword[root]
            indexid = wordindex[root]
            wordindex[word] = indexid
            rootpagelist = index[indexid].pages
            for npage, nword in allwords[word].pages:
                for entry in rootpagelist:
                    if entry[0] == npage:
                        entry[1] += nword
                        nword = 0
                        break
                if nword:
                    rootpagelist.append([npage, nword])
                    index[indexid].page_count += 1
            index[indexid].word_count += allwords[word].word_count
        else:
            wordindex[word] = len(index)
            index.append(allwords[word])
    return wordindex, index


def remove_common_words(wordindex: dict[str, int], index: list[WordEntry], pagecount: int) -> list[WordEntry | None]:
    """Replaces the entries of the words that are in more of the pages than INDEX_MAX_PAGE_RATIO with None."""
    result: list[WordEntry | None] = list(index)
    for indexid, entry in enumerate(index):
        if entry.page_count > pagecount * INDEX_MAX_PAGE_RATIO:
            result[indexid] = None
            words = ", ".join(word for word, wordid in wordindex.items() if wordid == indexid)
            print(f"Removing common word ({entry.page_count / pagecount:0.3f}): {words}")
    return result


def write_word_index(indexfile: str, help_index: HelpIndex) -> None:
    """Writes the word index as javascript."""
    words = [[key, val] for key, val in help_index.wordindex.items()]
    index = [entry.as_list() if entry else None for entry in help_index.index]
    with open(indexfile, "wb") as ixh:
        ixh.write(
            f"""
wordindex={{
    "words": new Map({json.dumps(words)}),
    "pages": {json.dumps(help_index.pages)},
    "index": {json.dumps(index)}
}};
""".encode("utf8")
        )
        ixh.write(b"installSearch();\n")


def process_page(basedir: str, url: str) -> tuple[str, str, list[str]]:
    """Returns the title of the page, its text with the title, headers and keywords repeated, and its references."""
    with open(basedir + url, encoding="utf-8") as urlh:
        page = urlh.read()

    # Just take refs from page.  Could get clever and get anchor text as
    # as high value index words for referenced page.

    page = re.sub(r"\s+", " ", page, flags=re.S)

    ref_urls = set()
    for refmatch in re.finditer(r"\<a\b[^\>]*\bhref\=\"([^\"]+)\"", page):
        href = refmatch.group(1)
        aurl = urljoin(url, href)
        ref_urls.add(aurl)
    refs = sorted(ref_urls)

    title = ""
    tmatch = re.search(r"\<title\>(.*?)\<\/title\>", page)
    if tmatch:
        title = " " + tmatch.group(1)

    page = page.lower()

    keywords = ""
    for kmatch in re.finditer(r"\<meta\s+name\=\"keywords\"\s+content=\"([^\"]+)\"", page):
        keywords = keywords + " " + kmatch.group(1)

    # Now just look up to body.  Also discard after <h3>see also</h3> as this as assumed to
    # be text from another page.

    bmatch = re.search(r"\<body[^\>]*\>(.*)\<\/body", page)
    if bmatch:
        page = bmatch.group(1)

    smatch = re.match(r"(.*)\<h3>\s*see\s+also", page)
    if smatch:
        page = smatch.group(1)

    # Capture header text

    headers = ""
    for hmatch in re.finditer(r"\<(h[1-4])\>(.*?)\<\/\1", page):
        header = hmatch.group(2)
        header = re.sub(r"\<.[^\>]*\>", " ", header)
        headers = headers + " " + header

    # Remove any more tags in page
    page = re.sub(r"\<[^\>]*\>", " ", page)

    # Add titles etc multiple times to weight in index
    page = page + title.lower() * TITLE_FACTOR + headers * HEADER_FACTOR + keywords * KEYWORD_FACTOR
    page = html.unescape(page)
    return title, page, refs


def index_text(text: str) -> dict[str, int]:
    """Returns how many times each word is in the text, in the order that the words first appear."""
    words: dict[str, int] = {}
    for match in re.finditer(r"\w+", text.lower()):
        word = match.group(0)
        if word not in words:
            words[word] = 1
        else:
            words[word] += 1
    return words


if __name__ == "__main__":
    main()
