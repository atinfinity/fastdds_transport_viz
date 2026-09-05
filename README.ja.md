# fastdds_transport_viz

[English](README.md) | 日本語

> 英語版が正です。この文書は 2026-09-05 時点の英語版に対応しています。

[![CI](https://github.com/atinfinity/fastdds_transport_viz/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/atinfinity/fastdds_transport_viz/actions/workflows/ci.yml)

ドキュメントサイト: <https://atinfinity.github.io/fastdds_transport_viz/ja/>

**ROS 2 の各トピックが Fast DDS のどの transport で通信しているか** — UDPv4、UDPv6、TCP、
共有メモリ (SHM)、zero-copy の data-sharing — を、**その理由とともに**表示します。

対象: ROS 2 Jazzy (Fast DDS 2.14) と Kilted / Rolling (Fast DDS 3.x)、`rmw_fastrtps_cpp`。

```
$ ros2 transport list -v --stats
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT         REASON
/chatter  std_msgs/msg/String  1     2     SHM x1, UDPv4 x1  same-host-guid,...
    /talker@myhost(136) -> /listener@myhost(135)      SHM    measured=SHM 47pkt    same-host-guid,datasharing-disabled-writer,both-shm-locators,measured-shm-traffic
    /talker@myhost(136) -> /listener_udp@myhost(134)  UDPv4  measured=UDPv4 49pkt  same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator,measured-udpv4-traffic

statistics: 63 samples from 3 participant(s)
shared memory: /dev/shm 2.19 MB used of 16.7 GB (16.7 GB free) | Fast DDS 2.19 MB in 3 segment(s), 6 port(s), 0 data-sharing histories
```

端末での実際の表示 (`--color auto`、stdout が端末なら既定で有効):

![colored table](docs/images/example-table.svg)

- **予測** は Fast DDS の discovery データ (各エンドポイントが広告する locator (通信先アドレス) と QoS)
  から求めます。観測対象のノードには何も要求しません。
- `--stats` を付けると、Fast DDS の statistics モジュールから **実測** を取り、実際にパケットを
  運んだ transport を表示します。
- すべての判定に理由コードが付きます。`--explain` で説明を表示できます。

## クイックスタート

```
docker compose build
docker compose run --rm dev
colcon build --symlink-install && source install/setup.bash

ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --explain
```

ツールは観測したいノードと同じ環境 (環境変数、XML プロファイル、ネットワーク/IPC 名前空間)
で実行してください。

## 使い方

```
ros2 transport list [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--node REGEX]
                    [--all] [-v] [--explain] [--stats] [--json] [--color auto|always|never]
                    [--watch [--interval S]]
ros2 transport codes
```

`ros2 transport` は薄い ros2cli 拡張 (パッケージ `ros2transport`) で、`fastdds_transport_viz` の
`transport_viz` バイナリを実行します。バイナリは
`ros2 run fastdds_transport_viz transport_viz` として直接実行することもでき、オプションは同じで
`--list-codes` が加わります。

| オプション | 効果 |
|---|---|
| `-v` | 各トピックの下に writer → reader のペアを展開する |
| `--explain` | 使われている理由コードの凡例を末尾に付ける |
| `--stats` | 実測の transport と `RATE` 列 (トピック/writer ごとの payload バイト数/秒) も表示する (観測対象ノードに `FASTDDS_STATISTICS` が必要。[docs/statistics.ja.md](docs/statistics.ja.md)) |
| `--json` | 機械可読な出力 (`schema_version: 1`、`schema/` 参照)。[web viewer](docs/web-viewer.ja.md) で開ける |
| `--topic REGEX` | 名前が一致するトピックだけ表示する |
| `--node REGEX` | 完全修飾ノード名が一致するノードが関わるペアだけ表示する (そのノードの未接続エンドポイントも残る) |
| `--all` | サービス/アクションと ROS 以外の DDS トピックも含める |
| `--watch` | `--interval` 秒ごとに再描画し、追加/変更/削除されたペアを強調する。キー `q p v e a` (`--json` 時は `changes` オブジェクト付きの JSON Lines) |
| `--color` | transport と警告の ANSI 色 (`auto` = 端末のときだけ) |

## ドキュメント

- [仕組み](docs/how-it-works.ja.md) — 判定ルール、理由コード、ホスト、実行場所、watch モード
- [実測 transport (`--stats`)](docs/statistics.ja.md) — statistics トピック、有効化、10 インスタンスの落とし穴
- [Data-sharing (zero-copy)](docs/data-sharing.ja.md) — ROS 2 トピックが既定で `SHM` になる理由と data-sharing の有効化
- [Web viewer](docs/web-viewer.ja.md) — `--json` 出力のグラフ/表表示、ライブモード (`transport_viz_web`)、JSON スキーマ
- [開発・検証・テスト](docs/development.md) (英語) — Docker 環境、パッケージ構成、検証ノード、マルチコンテナのシナリオ、テスト、検証結果、ロードマップ

## ライセンス

Apache-2.0
