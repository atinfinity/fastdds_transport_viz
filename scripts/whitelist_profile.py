#!/usr/bin/env python3
"""Merge an interface whitelist of this container's own addresses into a profiles file.

The third check of #130: with <interfaceWhiteList> Fast DDS opens one output socket per
whitelisted interface and a *unicast* message goes out on every one of them, each consuming
a statistics sequence number. The whitelist has to name concrete addresses, which only the
running container knows, so the profile is generated at startup:

    python3 whitelist_profile.py <statistics profiles file> <output file>

The statistics writer profiles of the input file are kept (Fast DDS reads a single
FASTRTPS_DEFAULT_PROFILES_FILE); a UDPv4 transport descriptor with the whitelist and a
default participant using it are added.
"""
import re
import socket
import subprocess
import sys


def own_addresses():
    """Every non-loopback IPv4 address of this container, in interface order."""
    out = subprocess.run(
        ['ip', '-o', '-4', 'addr', 'show'], check=True, capture_output=True, text=True).stdout
    found = []
    for line in out.splitlines():
        fields = line.split()
        if len(fields) > 3 and fields[1] != 'lo':
            address = fields[3].split('/')[0]
            try:
                socket.inet_aton(address)
            except OSError:
                continue
            if address not in found:
                found.append(address)
    return found


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    source, target = argv[1], argv[2]
    addresses = own_addresses()
    if not addresses:
        print('whitelist_profile: no non-loopback IPv4 address', file=sys.stderr)
        return 1
    with open(source, encoding='utf-8') as handle:
        text = handle.read()
    whitelist = '\n'.join(f'          <address>{a}</address>' for a in addresses)
    block = f"""  <transport_descriptors>
    <transport_descriptor>
      <transport_id>whitelisted_udp</transport_id>
      <type>UDPv4</type>
      <interfaceWhiteList>
{whitelist}
      </interfaceWhiteList>
    </transport_descriptor>
  </transport_descriptors>
  <profiles>
    <participant profile_name="whitelisted" is_default_profile="true">
      <rtps>
        <userTransports>
          <transport_id>whitelisted_udp</transport_id>
        </userTransports>
        <useBuiltinTransports>false</useBuiltinTransports>
      </rtps>
    </participant>"""
    merged, count = re.subn(r'  <profiles>', block, text, count=1)
    if count != 1:
        print(f'whitelist_profile: no <profiles> element in {source}', file=sys.stderr)
        return 1
    with open(target, 'w', encoding='utf-8') as handle:
        handle.write(merged)
    print(f'whitelist_profile: {target} whitelists {", ".join(addresses)}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
