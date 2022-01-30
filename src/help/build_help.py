#!/usr/bin/python3

import argparse
import re
import html
import json
import io
from urllib.parse import urljoin

# Words in more than this amount of pages are ignored for indexing
INDEX_MAX_PAGE_RATIO = 0.75
# Combine roots with matched endings
INDEX_PLURAL_ENDINGS = [
    ["", "s"],
    ["y", "ies"],
]
TEMPLATE="""
<html>
<head>
<title><!--TITLE--></title>
<link rel="stylesheet" href="css/helpapp.css">
<style media="print">#menu { display: none }</style>
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
<div id="page-content" class="frame">
<iframe id="help-page"></iframe>
</div>
</div>
</body>
</html>
"""

TITLE_FACTOR=10
KEYWORD_FACTOR=10
HEADER_FACTOR=10

def main():
    parser = argparse.ArgumentParser(description="Compile dynamic help file")
    parser.add_argument("contents_file", help="File describe help file contents")
    parser.add_argument("help_file", help="Generated help file")
    parser.add_argument("-b","--base-dir",help="Base directory for help files")
    parser.add_argument(
        "-t", "--title", default="SNAP help", help="Title of generated help file"
    )
    parser.add_argument(
        "-c", "--css-stylesheet", action="append", help="Included stylesheets"
    )
    parser.add_argument(
        "-s", "--script-file", action="append", help="Included javascript"
    )
    parser.add_argument(
        "-i", "--index-file", default="js/wordindex.js", help="Output javascript word index"
    )
    args = parser.parse_args()
    stylesheets = args.css_stylesheet or []
    scripts = args.script_file or []
    stylesheets.append("css/helpapp.css")
    scripts.insert(0, "js/jquery.min.js")
    scripts.append("js/helpapp.js")
    basedir=args.base_dir + "/" if args.base_dir else ""
    helpfile=basedir+args.help_file
    indexfile = args.index_file

    levels, urls = loadContents(args.contents_file)
    wordindex = buildIndex(basedir,urls)
    scripts.append('defer:'+indexfile)
    writeHelpPage(helpfile, levels, stylesheets, scripts, args.title)
    writeWordIndex(basedir+indexfile, wordindex)


def writeHelpPage(help_file, levels, stylesheets, scripts, title ):

    indexpage = levels[1][0]
    with io.StringIO() as w:
        for item in levels[1]:
            writeContentsItem(w, item)
        contenthtml=w.getvalue()

    with io.StringIO() as w:
        for css in stylesheets:
            w.write(f'<link rel="stylesheet" href="{css}">\n')
        stylesheethtml=w.getvalue()

    with io.StringIO() as w:
        for script in scripts:
            defer=""
            if script.startswith('defer:'):
                defer='defer'
                script=script[6:]
            w.write(f'<script src="{script}" {defer}></script>\n')
        scripthtml=w.getvalue()

    pagehtml=TEMPLATE
    pagehtml=pagehtml.replace('<!--TITLE-->',html.escape(title))
    pagehtml=pagehtml.replace('<!--CONTENTS-->',contenthtml)
    pagehtml=pagehtml.replace('<!--STYLESHEETS-->',stylesheethtml)
    pagehtml=pagehtml.replace('<!--SCRIPTS-->',scripthtml)

    with open(help_file, "w") as th:
        th.write(pagehtml)


def loadContents(contentsFile):
    levels = [None, []]
    level0 = 1
    urls = []
    with open(contentsFile) as cf:
        for l in cf:
            l = l.strip()
            if l == "":
                continue
            parts = l.split("\t")
            if len(parts) == 2:
                parts.append("")
            if len(parts) != 3:
                print(f'Warning: ignoring "{l}"')
                continue
            if parts[0] not in "1 2 3 4 5 6 7 8".split():
                print(f"Error: Invalid help level: {l}")
                continue
            if parts[2]:
                urls.append(parts[2])
            level = int(parts[0])
            parts.append([])
            if level > level0 + 1:
                print(f"Error: Level {level} after {level0}: {l}")
                continue
            if level == level0 + 1:
                if level not in levels:
                    levels.append(None)
                levels[level] = levels[level0][-1][3]
            levels[level].append(parts)
            level0 = level
    return levels, urls


def writeContentsItem(th, item):
    level, label, href, subitems = item
    th.write(f'<div class="contents-level level-{level}">\n')
    if href:
        th.write(f'<div class="contents-item"><a href="{href}">{html.escape(label)}</a></div>\n')
    else:
        th.write(f'<div class="contents-item">{label}</div>\n')
    for subitem in subitems:
        writeContentsItem(th, subitem)
    th.write("</div>\n")


def buildIndex(basedir,urls):
    allwords = {}
    pagedata = []
    todo = list(urls)
    todo.sort()
    done=set()
    # allwords[word] is array
    # [0] = page count
    # [1] = total word count
    # [2] = array [...] of
    #    [ pageid, pagewordcount ]
    while todo:
        url = todo.pop(0)
        if url in done:
            continue
        done.add(url)
        title, text, refs = processPage(basedir,url)
        words = indexText(text)
        npage = len(pagedata)
        pagedata.append({"url": url, "title": title})
        for word in words:
            # Skip things like numbers - this will screw up #commands etc.. To be reviewed
            if not re.match("^[#a-z]", word):
                continue
            if word not in allwords:
                allwords[word] = [0, 0, []]
            allwords[word][0] += 1
            allwords[word][1] += words[word]
            allwords[word][2].append([npage, words[word]])
        for ref in refs:
            if re.match(r'^\w+\:\/',ref):
                print(f"Skipping non-relative url {ref}")
                continue
            ref = re.sub(r'[\#\?].*','',ref)
            if ref not in done and ref not in todo:
                print(f"Adding non-indexed url {ref}")
                todo.append(ref)

    # Form list rootword joining equivalent words (eg singular/plural versions)
    # rootword redirects from alphabetically highest to lowest word of equivalents 
    wordlist = list(sorted(allwords.keys()))
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

    # Merge entries for allword into a common index.  wordindex is a lookup
    # to the index.
    wordindex = {}
    index = []
    for word in wordlist:
        root = rootword.get(word)
        if root:
            while root in rootword:
                root = rootword[root]
            indexid = wordindex[root]
            wordindex[word] = indexid
            rootpagelist = index[indexid][2]
            pagelist = allwords[word][2]
            pagecount = index[indexid][0]
            for npage, nword in pagelist:
                for entry in rootpagelist:
                    if entry[0] == npage:
                        entry[1] += nword
                        nword = 0
                        break
                if nword:
                    rootpagelist.append([npage, nword])
                    index[indexid][0] += 1
            index[indexid][1] += allwords[word][1]
        else:
            wordindex[word] = len(index)
            index.append(allwords[word])

    pagecount = len(pagedata)
    for indexid, entry in enumerate(index):
        if entry[0] > pagecount * INDEX_MAX_PAGE_RATIO:
            index[indexid] = None
            words = ", ".join(
                (word for word in wordindex if wordindex[word] == indexid)
            )
            print(f"Removing common word ({entry[0]/pagecount:0.3f}): {words}")

    indexdata = {
        "wordindex": wordindex,
        "index": index,
        "pages": pagedata,
    }

    return indexdata


def writeWordIndex(indexfile, wordindex):
    words=[[key,val] for key,val in wordindex['wordindex'].items()]
    with open(indexfile, "w") as ixh:
        ixh.write(f'''
wordindex={{
    "words": new Map({json.dumps(words)}),
    "pages": {json.dumps(wordindex["pages"])},
    "index": {json.dumps(wordindex["index"])}
}};
''')
        ixh.write("installSearch();")


def processPage(basedir,url):
    with open(basedir+url) as urlh:
        page = urlh.read()
    
    # Just take refs from page.  Could get clever and get anchor text as 
    # as high value index words for referenced page.

    page=re.sub(r'\s+',' ',page,flags=re.S)

    refs=set()
    for refmatch in re.finditer(r"\<a\b[^\>]*\bhref\=\"([^\"]+)\"",page):
        href=refmatch.group(1)
        aurl = urljoin(url, href)
        refs.add(aurl)
    refs=list(sorted(refs))

    title=''
    if tmatch := re.search(r"\<title\>(.*?)\<\/title\>",page):
        title=' '+tmatch.group(1)

    page=page.lower()

    keywords=''
    for kmatch in re.finditer(r"\<meta\s+name\=\"keywords\"\s+content=\"([^\"]+)\"",page):
        keywords = keywords + ' ' + kmatch.group(1)

    # Now just look up to body.  Also discard after <h3>see also</h3> as this as assumed to
    # be text from another page.

    if bmatch := re.search(r"\<body[^\>]*\>(.*)\<\/body",page):
        page=bmatch.group(1)

    if smatch := re.match(r'(.*)\<h3>\s*see\s+also',page):
        page=smatch.group(1)

    # Capture header text

    headers=''
    for hmatch in re.finditer(r'\<(h[1-4])\>(.*?)\<\/\1',page):
        header=hmatch.group(2)
        header=re.sub(r'\<.[^\>]*\>',' ',header)
        headers=headers+' '+header

    # Remove any more tags in page
    page=re.sub(r'\<[^\>]*\>',' ',page)

    # Add titles etc multiple times to weight in index
    page=page+title.lower()*TITLE_FACTOR+headers*HEADER_FACTOR+keywords*KEYWORD_FACTOR
    page=html.unescape(page)
    return title, page, refs


def indexText(text):
    words = {}
    for m in re.finditer(r"\w+", text.lower()):
        word = m.group(0)
        if word not in words:
            words[word] = 1
        else:
            words[word] += 1
    return words


if __name__ == "__main__":
    main()
