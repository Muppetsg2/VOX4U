#!/usr/bin/env python3
# Copyright (c) 2025-2026 Muppetsg2
# Licensed under the MIT License.
"""
Collects every translatable text of the VOX4U sources into Localization/VOX4U.pot and brings the
translations in Localization/<culture>.po up to date.

    python Tools/UpdateLocalization.py           # write VOX4U.pot, update every <culture>.po
    python Tools/UpdateLocalization.py --check   # only report; exit code 1 if something is out of date

Collected:
  * LOCTEXT / NSLOCTEXT                       -> "<namespace>,<key>"
  * DisplayName / ToolTip of visible UPROPERTYs -> "UObjectDisplayNames,<Class>:<Property>",
                                                   "UObjectToolTips,<Class>:<Property>"
  * Category of visible UPROPERTYs           -> "UObjectCategory,<Category>"
  * DisplayName / ToolTip of UENUM values     -> "UObjectDisplayNames,<Enum>.<Value>", "UObjectToolTips,..."
  * DisplayName / ToolTip of UFUNCTIONs        -> "UObjectDisplayNames,<Class>:<Function>" (CallInEditor buttons)

These are the keys Unreal itself looks up, so the plugin (Source/VOX4UEditor/Private/VoxLocalization.cpp)
can load the .po files directly. Visible properties and enum values must have an explicit DisplayName and
ToolTip: Unreal only uses a translation whose English text matches the code exactly.

When the English text of a message changes, its old translation is kept but marked "fuzzy" (not used)
until a translator checks it.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIRS = [os.path.join(ROOT, "Source", "VOX4U"), os.path.join(ROOT, "Source", "VOX4UEditor")]
LOCALIZATION_DIR = os.path.join(ROOT, "Localization")
TEMPLATE = os.path.join(LOCALIZATION_DIR, "VOX4U.pot")

VISIBLE = re.compile(r"\b(EditAnywhere|VisibleAnywhere|EditDefaultsOnly|VisibleDefaultsOnly|EditInstanceOnly|VisibleInstanceOnly)\b")
CPP_STRING = r'"((?:[^"\\]|\\.)*)"'


def cpp_unescape(text):
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t", "\\": "\\", '"': '"'}.get(m.group(1), m.group(0)), text)


def po_escape(text):
    return text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t")


def po_unescape(text):
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"'}.get(m.group(1), m.group(0)), text)


def balanced(text, open_index):
    """Index just after the parenthesis that closes the one at open_index."""
    depth = 0
    in_string = False
    i = open_index
    while i < len(text):
        c = text[i]
        if in_string:
            if c == "\\":
                i += 1
            elif c == '"':
                in_string = False
        elif c == '"':
            in_string = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError("unbalanced parenthesis")


def meta_value(args, name):
    m = re.search(r"\b" + name + r"\s*=\s*" + CPP_STRING, args)
    return cpp_unescape(m.group(1)) if m else None


class Collector:
    def __init__(self):
        self.messages = {}  # context -> [source, references]
        self.problems = []

    def add(self, context, source, reference):
        if context in self.messages:
            if self.messages[context][0] != source:
                self.problems.append(f"{reference}: {context} has two different texts")
            self.messages[context][1].append(reference)
        else:
            self.messages[context] = [source, [reference]]

    def scan(self, path):
        with open(path, encoding="utf-8-sig") as f:
            text = f.read()
        relative = os.path.relpath(path, ROOT).replace(os.sep, "/")

        def line_of(index):
            return text.count("\n", 0, index) + 1

        # LOCTEXT / NSLOCTEXT, with the namespace in force at that point of the file
        namespaces = [(m.start(), m.group(1)) for m in re.finditer(r'#define\s+LOCTEXT_NAMESPACE\s+' + CPP_STRING, text)]
        for m in re.finditer(r"\bLOCTEXT\(\s*" + CPP_STRING + r"\s*,\s*" + CPP_STRING + r"\s*\)", text):
            namespace = None
            for start, name in namespaces:
                if start < m.start():
                    namespace = name
            if namespace is None:
                self.problems.append(f"{relative}:{line_of(m.start())}: LOCTEXT without LOCTEXT_NAMESPACE")
                continue
            self.add(f"{namespace},{cpp_unescape(m.group(1))}", cpp_unescape(m.group(2)), f"{relative}:{line_of(m.start())}")
        for m in re.finditer(r"\bNSLOCTEXT\(\s*" + CPP_STRING + r"\s*,\s*" + CPP_STRING + r"\s*,\s*" + CPP_STRING + r"\s*\)", text):
            self.add(f"{cpp_unescape(m.group(1))},{cpp_unescape(m.group(2))}", cpp_unescape(m.group(3)), f"{relative}:{line_of(m.start())}")

        # UPROPERTY
        owners = [(m.start(), m.group(1)[1:]) for m in re.finditer(r"^\s*(?:class|struct)\s+(?:\w+_API\s+)?([UAF]\w+)\s*(?::|$|\{)", text, re.M)]
        for m in re.finditer(r"\bUPROPERTY\s*\(", text):
            if text[:m.start()].rstrip().endswith("#define"):
                continue
            end = balanced(text, m.end() - 1)
            args = text[m.end():end - 1]
            if not VISIBLE.search(args):
                continue
            declaration = text[end:text.index(";", end)]
            name = re.search(r"(\w+)\s*(?::\s*\d+)?\s*$", declaration).group(1)
            owner = None
            for start, owner_name in owners:
                if start < m.start():
                    owner = owner_name
            reference = f"{relative}:{line_of(m.start())}"
            key = f"{owner}:{name}"
            display = meta_value(args, "DisplayName")
            tooltip = meta_value(args, "ToolTip")
            category = meta_value(args, "Category")
            if display is None or tooltip is None:
                self.problems.append(f"{reference}: {key} needs an explicit DisplayName and ToolTip")
            if display is not None:
                self.add(f"UObjectDisplayNames,{key}", display, reference)
            if tooltip is not None:
                self.add(f"UObjectToolTips,{key}", tooltip, reference)
            if category:
                self.add(f"UObjectCategory,{category}", category, reference)

        # UFUNCTION shown in the editor (CallInEditor buttons) or with an explicit display name
        for m in re.finditer(r"\bUFUNCTION\s*\(", text):
            end = balanced(text, m.end() - 1)
            args = text[m.end():end - 1]
            declaration = text[end:text.index("(", end)]
            name = re.search(r"(\w+)\s*$", declaration).group(1)
            owner = None
            for start, owner_name in owners:
                if start < m.start():
                    owner = owner_name
            reference = f"{relative}:{line_of(m.start())}"
            key = f"{owner}:{name}"
            display = meta_value(args, "DisplayName")
            tooltip = meta_value(args, "ToolTip")
            if re.search(r"\bCallInEditor\b", args) and (display is None or tooltip is None):
                self.problems.append(f"{reference}: {key} is shown in the editor and needs an explicit DisplayName and ToolTip")
            if display is not None:
                self.add(f"UObjectDisplayNames,{key}", display, reference)
            if tooltip is not None:
                self.add(f"UObjectToolTips,{key}", tooltip, reference)

        # UENUM
        for m in re.finditer(r"\bUENUM\s*\(", text):
            enum = re.compile(r"enum\s+class\s+(\w+)[^{]*\{(.*?)\}", re.S).search(text, m.end())
            if not enum:
                continue
            for value in re.finditer(r"^\s*(\w+)\s*(?:=\s*[^,\n]+?)?\s*(UMETA\s*\()?", enum.group(2), re.M):
                reference = f"{relative}:{line_of(enum.start(2) + value.start())}"
                key = f"{enum.group(1)}.{value.group(1)}"
                if not value.group(2):
                    self.problems.append(f"{reference}: {key} needs UMETA(DisplayName, ToolTip)")
                    continue
                open_index = enum.start(2) + value.end() - 1
                args = text[open_index + 1:balanced(text, open_index) - 1]
                if meta_value(args, "Hidden") is not None or re.search(r"\bHidden\b", args):
                    continue
                display = meta_value(args, "DisplayName")
                tooltip = meta_value(args, "ToolTip")
                if display is None or tooltip is None:
                    self.problems.append(f"{reference}: {key} needs an explicit DisplayName and ToolTip")
                if display is not None:
                    self.add(f"UObjectDisplayNames,{key}", display, reference)
                if tooltip is not None:
                    self.add(f"UObjectToolTips,{key}", tooltip, reference)


def parse_po(path):
    """Header text and messages: context -> (source, translation, fuzzy)."""
    header, messages = [], {}
    with open(path, encoding="utf-8-sig") as f:
        lines = f.read().splitlines()
    entry, field, in_header = {}, None, True

    def flush():
        nonlocal entry, field
        if "context" in entry:
            messages[entry["context"]] = (entry.get("source", ""), entry.get("translation", ""), entry.get("fuzzy", False))
        entry, field = {}, None

    for line in lines + [""]:
        stripped = line.strip()
        if in_header:
            header.append(line)
            if stripped == "":
                in_header = False
            continue
        if not stripped:
            flush()
            continue
        if field == "translation" and (stripped.startswith("#") or stripped.startswith("msgctxt") or stripped.startswith("msgid ")):
            flush()
        if stripped.startswith("#,"):
            entry["fuzzy"] = entry.get("fuzzy", False) or "fuzzy" in stripped
        elif stripped.startswith("#"):
            pass
        elif stripped.startswith("msgctxt"):
            entry["context"], field = po_unescape(stripped[stripped.index('"') + 1:-1]), "context"
        elif stripped.startswith("msgid"):
            entry["source"], field = po_unescape(stripped[stripped.index('"') + 1:-1]), "source"
        elif stripped.startswith("msgstr"):
            entry["translation"], field = po_unescape(stripped[stripped.index('"') + 1:-1]), "translation"
        elif stripped.startswith('"') and field:
            entry[field] = entry.get(field, "") + po_unescape(stripped[1:-1])
    return header, messages


def format_po(header_lines, messages, translations=None):
    out = list(header_lines)
    if out and out[-1].strip():
        out.append("")
    for context, (source, references) in messages:
        translation, fuzzy = "", False
        if translations is not None and context in translations:
            old_source, old_translation, old_fuzzy = translations[context]
            translation = old_translation
            fuzzy = bool(old_translation) and (old_fuzzy or old_source != source)
        out.append(f"#. Key: {context.split(',', 1)[1]}")
        out.extend(f"#: {reference}" for reference in references)
        if fuzzy:
            out.append("#, fuzzy")
        out.append(f'msgctxt "{po_escape(context)}"')
        out.append(f'msgid "{po_escape(source)}"')
        out.append(f'msgstr "{po_escape(translation)}"')
        out.append("")
    return "\n".join(out)


TEMPLATE_HEADER = [
    "# VOX4U user interface texts.",
    "# Copy this file to <culture>.po (pl.po, es.po, de.po, pt-BR.po, ...) in this folder and fill in the msgstr lines.",
    "# Generated by Tools/UpdateLocalization.py - do not edit by hand.",
    'msgid ""',
    'msgstr ""',
    '"Project-Id-Version: VOX4U\\n"',
    '"MIME-Version: 1.0\\n"',
    '"Content-Type: text/plain; charset=UTF-8\\n"',
    '"Content-Transfer-Encoding: 8bit\\n"',
    "",
]


def write_if_changed(path, text, check):
    old = None
    if os.path.exists(path):
        with open(path, encoding="utf-8-sig") as f:
            old = f.read()
    if old == text:
        return False
    if not check:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    return True


def main():
    check = "--check" in sys.argv[1:]
    collector = Collector()
    for directory in SOURCE_DIRS:
        for folder, _, files in sorted(os.walk(directory)):
            for name in sorted(files):
                if name.endswith((".h", ".cpp")):
                    collector.scan(os.path.join(folder, name))

    for problem in collector.problems:
        print("warning:", problem)

    messages = list(collector.messages.items())
    os.makedirs(LOCALIZATION_DIR, exist_ok=True)
    changed = []
    if write_if_changed(TEMPLATE, format_po(TEMPLATE_HEADER, messages), check):
        changed.append(TEMPLATE)

    for name in sorted(os.listdir(LOCALIZATION_DIR)):
        if not name.endswith(".po"):
            continue
        path = os.path.join(LOCALIZATION_DIR, name)
        header, translations = parse_po(path)
        if write_if_changed(path, format_po(header, messages, translations), check):
            changed.append(path)
        missing = [context for context, _ in messages if not translations.get(context, ("", "", False))[1]]
        fuzzy = [context for context, (source, _) in messages if context in translations
                 and translations[context][1] and (translations[context][2] or translations[context][0] != source)]
        print(f"{name}: {len(messages) - len(missing) - len(fuzzy)}/{len(messages)} translated"
              + (f", {len(fuzzy)} to review (fuzzy)" if fuzzy else "") + (f", {len(missing)} missing" if missing else ""))

    print(f"{len(messages)} messages")
    for path in changed:
        print(("out of date: " if check else "updated: ") + os.path.relpath(path, ROOT))
    return 1 if (check and changed) or collector.problems else 0


if __name__ == "__main__":
    sys.exit(main())
