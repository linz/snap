#!/usr/bin/python3

import argparse
import re

def main():
    parser=argparse.ArgumentParser(description="Compile dynamic help file")
    parser.add_argument("contents_file",help="File describe help file contents")
    parser.add_argument("help_file",help="Generated help file")
    parser.add_argument("-t","--title",default="SNAP help",help="Title of generated help file")
    parser.add_argument("-c","--css-stylesheet",action="append",help="Included stylesheets")
    parser.add_argument("-s","--script-file",action="append",help="Included javascript")
    args=parser.parse_args()
    stylesheets=args.css_stylesheet or []
    scripts=args.script_file or []
    stylesheets.append("css/help.css")
    scripts.insert(0,"js/jquery.min.js")
    scripts.append("js/help.js")

    levels=[None,[]]
    level0=1
    with open(args.contents_file) as cf:
        for l in cf:
            l=l.strip()
            if l == "":
                continue
            parts=l.split("\t")
            if len(parts) == 2:
                parts.append("")
            if len(parts) != 3:
                print(f"Warning: ignoring \"{l}\"")
                continue
            if parts[0] not in '1 2 3 4 5 6 7 8'.split():
                print(f"Error: Invalid help level: {l}")
                continue
            level=int(parts[0])
            parts.append([])
            if level > level0+1:
                print(f"Error: Level {level} after {level0}: {l}")
                continue
            if level == level0+1:
                if level not in levels:
                    levels.append(None)
                levels[level]=levels[level0][-1][3]
            levels[level].append(parts)
            level0=level

    indexpage=levels[1][0]

    with open(args.help_file,"w") as th:
        th.write(f"""
        <html>
        <head>
        <title>{args.title}</title>
        """)
        for css in stylesheets:
            th.write(f"<link rel=\"stylesheet\" href=\"{css}\">\n")
        for script in scripts:
            th.write(f"<script src=\"{script}\"></script>\n")
        th.write("</head>\n")
        th.write("</body>\n")
        th.write("<div class=\"container\">\n")
        th.write("<div id=\"menu\">\n")
        th.write("<div id=\"contents\" class=\"contents\">\n")
        for item in levels[1]:
            writeContentsItem(th,item)
        th.write("</div>\n")
        th.write("</div>\n")
        th.write("<div id=\"page-content\" class=\"frame\">\n")
        th.write(f"<iframe id=\"help-page\" src=\"{indexpage[2]}\" title=\"{indexpage[1]}\"></iframe>\n")
        th.write("</div>\n")
        th.write("</div>\n")
        th.write("</body>\n")
        th.write("</html>\n")

def writeContentsItem(th,item):
    level,label,href,subitems=item
    th.write(f"<div class=\"contents-level level-{level}\">\n")
    if href:
        th.write(f"<div class=\"contents-item\"><a href=\"{href}\">{label}</a></div>\n")
    else:
        th.write(f"<div class=\"contents-item\">{label}</div>\n")
    for subitem in subitems:
        writeContentsItem(th,subitem)
    th.write("</div>\n")
    

if __name__=="__main__":
    main()