"""Writes the software license agreement of a disk image, which the Finder shows before it mounts
the image, as the resources that hdiutil udifrez -xml reads.

    sla.py <license> <output>

The agreement is in English, the language of the license. The license must be ASCII text, which
the resource TEXT holds as Mac OS Roman with carriage returns.
"""

import plistlib
import struct
import sys

# The resource TEXT holds at most this many bytes.
MAX_TEXT_SIZE = 32767


def pascal_string(text):
    data = text.encode("mac_roman")
    return bytes([len(data)]) + data


def resource(data):
    # Every resource of the agreement has the ID 5000 of the first language.
    return {"Attributes": "0x0000", "Data": data, "ID": "5000", "Name": "English"}


def main():
    license_path, output = sys.argv[1], sys.argv[2]
    with open(license_path, "rb") as file:
        text = file.read().decode("ascii")
    body = text.replace("\r\n", "\n").replace("\n", "\r").encode("mac_roman")
    if len(body) > MAX_TEXT_SIZE:
        sys.exit("%s has %d bytes, more than the %d of the resource TEXT."
                 % (license_path, len(body), MAX_TEXT_SIZE))

    # LPic: the default language, the number of entries, and for each entry the region code,
    # the offset of its resource IDs from 5000 and whether its script uses two bytes per character
    languages = struct.pack(">hhhhh", 0, 1, 0, 0, 0)

    # STR#: the name of the language, the labels of the buttons and the explanation
    strings = [
        "English",
        "Agree",
        "Disagree",
        "Print",
        "Save...",
        "If you agree with the terms of this license, click \"Agree\" to open the disk image. "
        "If you do not agree, click \"Disagree\".",
    ]
    labels = struct.pack(">h", len(strings)) + b"".join(pascal_string(s) for s in strings)

    with open(output, "wb") as file:
        plistlib.dump({
            "LPic": [resource(languages)],
            "STR#": [resource(labels)],
            "TEXT": [resource(body)],
        }, file)


if __name__ == "__main__":
    main()
