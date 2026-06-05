from lxml import etree


def main():
    filename = "Examples/EsiFiles/XML/Official Beckhoff/Beckhoff EL1xxx.xml"
    # Load file with lxml:
    with open(filename, "rb") as f:
        tree = etree.parse(f)
    print(tree)

    descriptions = tree.find("Descriptions")
    d = descriptions.find("Devices")
    print(d)
    devices = d.getchildren()
    for device in devices:
        processDevice(device)
    # Find device elements:
    # for device in


def processDevice(device):
    print("Checking device")
    print(device)
    type = device.find("Type")
    print(
        "Name:",
        type.text,
        "Product code",
        type.attrib["ProductCode"],
        type.text,
        "Revision number",
        type.attrib["RevisionNo"],
    )


if __name__ == "__main__":
    main()
