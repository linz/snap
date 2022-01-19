#!/usr/bin/python3

import argparse
import re


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

contents=[]
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
        contents.append(parts)


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
    for item in contents:
        level,label,href=item
        if href:
            th.write(f"<div class=\"contents-item level-{level}\"><a href=\"{href}\">{label}</a></div>\n")
        else:
            th.write(f"<div class=\"contents-item level-{level}\">{label}</div>\n")

    th.write("</div>\n")
    th.write("</div>\n")
    th.write("<div id=\"page-content\" class=\"frame\">\n")
    th.write(f"<iframe id=\"help-page\" src=\"{contents[0][2]}\" title=\"{contents[0][1]}\"></iframe>\n")
    th.write("</div>\n")
    th.write("</div>\n")
    th.write("</body>\n")
    th.write("</html>\n")
