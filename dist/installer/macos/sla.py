"""Writes the software license agreement of a disk image, which the Finder shows before it mounts
the image, as the resources that hdiutil udifrez -xml reads.

    sla.py <license> <output>

The agreement is offered in English and in Simplified Chinese. The Finder shows the language of
the system if the agreement has it, or else the default language, English. Each language has its
own labels of the buttons and explanation, and both show the license in English, the language of
the license. The license must be ASCII text, which both legacy encodings of the resources include.
"""

import plistlib
import struct
import sys

# The resource TEXT holds at most this many bytes.
MAX_TEXT_SIZE = 32767

# The languages of the agreement: the name of the resources, the region code of the classic Mac OS
# (verUS and verChina), the legacy encoding of the text and whether the script uses two bytes per
# character, and the strings of STR#: the name of the language, the labels of the buttons and the
# explanation. The first language is the default.
LANGUAGES = [
    {
        "name": "English",
        "region": 0,
        "encoding": "mac_roman",
        "two_byte": False,
        "strings": [
            "English",
            "Agree",
            "Disagree",
            "Print",
            "Save...",
            "If you agree with the terms of this license, click \"Agree\" to open the disk image. "
            "If you do not agree, click \"Disagree\".",
        ],
    },
    {
        "name": "Simplified Chinese",
        "region": 52,
        "encoding": "gb2312",
        "two_byte": True,
        "strings": [
            "简体中文",
            "同意",
            "不同意",
            "打印",
            "存储...",
            "如果您同意本许可协议的条款，请点按「同意」以打开磁盘映像。如果您不同意，请点按「不同意」。",
        ],
    },
]


def pascal_string(text, encoding):
    data = text.encode(encoding)
    if len(data) > 255:
        sys.exit("The string %r has more than 255 bytes." % text)
    return bytes([len(data)]) + data


def resource(data, index):
    # The resources of each language have the ID 5000 plus the index of the language.
    language = LANGUAGES[index]
    return {"Attributes": "0x0000", "Data": data, "ID": str(5000 + index),
            "Name": language["name"]}


def main():
    license_path, output = sys.argv[1], sys.argv[2]
    with open(license_path, "rb") as file:
        text = file.read().decode("ascii")
    text = text.replace("\r\n", "\n").replace("\n", "\r")

    # LPic: the region code of the default language, the number of entries, and for each entry
    # the region code, the offset of its resource IDs from 5000 and whether its script uses two
    # bytes per character
    lpic = struct.pack(">hh", LANGUAGES[0]["region"], len(LANGUAGES))
    labels = []
    bodies = []
    for index, language in enumerate(LANGUAGES):
        lpic += struct.pack(">hhh", language["region"], index, int(language["two_byte"]))
        strings = language["strings"]
        labels.append(resource(struct.pack(">h", len(strings)) + b"".join(
            pascal_string(s, language["encoding"]) for s in strings), index))
        body = text.encode(language["encoding"])
        if len(body) > MAX_TEXT_SIZE:
            sys.exit("%s has %d bytes, more than the %d of the resource TEXT."
                     % (license_path, len(body), MAX_TEXT_SIZE))
        bodies.append(resource(body, index))

    with open(output, "wb") as file:
        plistlib.dump({
            "LPic": [resource(lpic, 0)],
            "STR#": labels,
            "TEXT": bodies,
        }, file)


if __name__ == "__main__":
    main()
