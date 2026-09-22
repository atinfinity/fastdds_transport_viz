# Security policy

## Supported versions

Security fixes go to the latest release and to `main`. There is one `main` branch for every
ROS 2 distribution (Humble, Jazzy, Lyrical, Rolling), and older releases do not receive
backports: update to the latest release to get a fix.

## Reporting a vulnerability

Please do not open a public issue for a vulnerability. Report it privately through GitHub's
[private vulnerability reporting](https://github.com/atinfinity/fastdds_transport_viz/security/advisories/new)
("Report a vulnerability" on the repository's Security tab), with the affected version, the
ROS 2 distribution and a way to reproduce it.

- You will get an acknowledgement within **7 days**.
- A fix is released, or the vulnerability is published as a
  [GitHub Security Advisory](https://github.com/atinfinity/fastdds_transport_viz/security/advisories),
  within **90 days** of the report. If more time is needed, the reason and a new date are
  given in the report's thread.
- The advisory credits the reporter unless they ask not to be named.

## Scope

`transport_viz` observes a DDS domain and reads the Fast DDS shared-memory files of its own
host; `transport_viz_web` serves what it sees over HTTP, on `127.0.0.1` unless `--bind` says
otherwise, without authentication. Exposing `transport_viz_web` on a network you do not
trust shows your ROS graph to anyone who can reach it; that is the documented behavior, not
a vulnerability.
