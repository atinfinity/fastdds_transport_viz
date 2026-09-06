# fastdds_transport_viz

[English](README.md) | 日本語

> 英語版が正です。この文書は 2026-09-06 時点の英語版に対応しています。

[![CI](https://github.com/atinfinity/fastdds_transport_viz/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/atinfinity/fastdds_transport_viz/actions/workflows/ci.yml)

ドキュメントサイト: <https://atinfinity.github.io/fastdds_transport_viz/ja/>

**ROS 2 の各トピックが Fast DDS のどの transport で通信しているか** — UDPv4、UDPv6、TCP、
共有メモリ (SHM)、zero-copy の data-sharing — を、**その理由とともに**表示します。

対象: ROS 2 Jazzy (Fast DDS 2.14) と Kilted / Rolling (Fast DDS 3.x)、`rmw_fastrtps_cpp`。

```
$ ros2 transport list -v --stats --topic '^/(chatter|bounded)$'
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT         RATE    REASON
/bounded  std_msgs/msg/Int32   1     1     DATA_SHARING x1   80 B/s  same-host-guid,datasharing-qos-enabled-both,datasharing-domain-ids-match,datasharing-confirmed-no-data-submessages
    /bounded_pub@57e20ce67dfe(156) -> /bounded_sub@57e20ce67dfe(159)  DATA_SHARING  80 B/s  measured=SHM 2pkt 248 B  same-host-guid,datasharing-qos-enabled-both,datasharing-domain-ids-match,datasharing-confirmed-no-data-submessages
/chatter  std_msgs/msg/String  1     2     UDPv4 x1, SHM x1  23 B/s  same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator,measured-udpv4-traffic,both-shm-locators,measured-shm-traffic
    /talker@57e20ce67dfe(157) -> /listener_udp@57e20ce67dfe(158)  UDPv4  23 B/s  measured=UDPv4 10pkt 1.31 kB  same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator,measured-udpv4-traffic
    /talker@57e20ce67dfe(157) -> /listener@57e20ce67dfe(160)      SHM    23 B/s  measured=SHM 10pkt 1.31 kB    same-host-guid,datasharing-disabled-writer,both-shm-locators,measured-shm-traffic

statistics: 562 samples from 6 participant(s)

shared memory: /dev/shm 339 MB used of 16.7 GB (16.3 GB free) | Fast DDS 4.06 MB in 6 segment(s), 14 port(s), 2 data-sharing histories (1 unmatched)
```

同じキャプチャの端末での表示 (`--color auto`、stdout が端末なら既定で有効):

![colored table](docs/images/example-table.svg)

- **予測** は Fast DDS の discovery データ (各エンドポイントが広告する locator (通信先アドレス) と QoS)
  から求めます。観測対象のノードには何も要求しません。
- `--stats` を付けると、Fast DDS の statistics モジュールから **実測** を取り、実際にパケットを
  運んだ transport を表示します。
- すべての判定に理由コードが付きます。`--explain` で説明を表示できます。

## できること

- **discovery データだけで、ペアごとの transport を予測。** writer → reader の各ペアに
  transport (`UDPv4`、`UDPv6`、`TCPv4`/`TCPv6`、`SHM`、`DATA_SHARING`) と機械可読な理由コードを
  付けます。観測対象のノードに変更は不要です。
- **`--stats` で実測。** Fast DDS の statistics モジュールから、locator ごとに実際に流れた
  パケット数とバイト数、payload レート (`RATE`)、ホスト名とプロセス id、zero-copy data-sharing の
  証明を取り、予測と食い違う実測は警告します。
- **複数のフロントエンド。** 色付きの表、`--watch` (変化を強調するライブ表示)、スキーマ付きの
  `--json`、`ros2 transport` コマンド、web viewer (グラフと表、`transport_viz_web` によるライブ更新)。
- **絞り込み。** `--topic` / `--node` の正規表現フィルタ、使われたコードの説明を出す `--explain`、
  全コードを一覧する `ros2 transport codes`。
- **環境の共有メモリ。** `/dev/shm` の容量、そこにある Fast DDS のセグメント・ポート・data-sharing
  履歴、残骸 (stale)、観測対象ノードがそれを共有しているかどうか。
- **検証済みの環境:** Jazzy (Fast DDS 2.14) と Kilted / Rolling (Fast DDS 3.x)、x86_64 と arm64、
  Discovery Server、`LARGE_DATA` (TCP)、`UDPv6`、`LOCALHOST` の discovery range、大きな SHM
  サンプル、zero-copy data-sharing、2 台の物理ホスト (x86_64 ↔ Jetson Orin NX、Wi-Fi 経由、両方向の
  予測と実測)。

## クイックスタート

```
docker compose build
docker compose run --rm dev bash
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

- [はじめに](docs/getting-started.ja.md) — ビルド (ネイティブ / Docker)、最初の実行、`--stats`、watch モード、web viewer、最初の確認事項
- [仕組み](docs/how-it-works.ja.md) — 判定ルール、理由コード、ホスト、実行場所、watch モード
- [実測 transport (`--stats`)](docs/statistics.ja.md) — statistics トピック、有効化、10 インスタンスの落とし穴
- [Data-sharing (zero-copy)](docs/data-sharing.ja.md) — ROS 2 トピックが既定で `SHM` になる理由と data-sharing の有効化
- [Web viewer](docs/web-viewer.ja.md) — `--json` 出力のグラフ/表表示、ライブモード (`transport_viz_web`)、JSON スキーマ
- [Architecture](docs/architecture.md) (英語) — コンポーネント、1 回の実行の流れ、データモデル、Fast DDS 2.14/3.x の互換層、拡張ポイント
- [開発・検証・テスト](docs/development.md) (英語) — Docker 環境、パッケージ構成、検証ノード、マルチコンテナのシナリオ、テスト、検証結果、ロードマップ

## 制限事項

- **`rmw_fastrtps_cpp` 専用。** CycloneDDS、Connext、`rmw_fastrtps_dynamic_cpp` のノードは対象外です。
  ROS ノードでない Fast DDS participant は `--all` でのみ表示されます。
- **Linux 専用。** macOS には `/dev/shm` が無く、Docker Desktop からホスト上のノードは観測できません。
- **ノードと同じ場所で実行する必要があります。** 同じドメイン、同じ環境変数と XML プロファイル、
  同じネットワーク/IPC 名前空間。`ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` では何も見えません。
- **予測はモデルです。** 判定は Fast DDS の選択規則を写したもので、`--stats` で確認するまで
  `likely` (`?` 付き) のままの状況があります。実測には観測対象ノードの *起動前* に
  `FASTDDS_STATISTICS` を設定する必要があり、10 を超える locator と通信するノードには同梱の
  プロファイルも必要です。
- **statistics は participant 単位** (ROS ノードごとに 1 つ) なので、同じ 2 ノード間の複数トピックは
  1 つの測定値を共有します。
- **帯域や遅延の測定ツールではありません。** `RATE` は Fast DDS 自身の `PUBLICATION_THROUGHPUT` の
  値で、遅延は表示しません。
- **DDS Security (SROS2) は未対応** で未検証です。ツールの participant にはセキュリティ設定が無いので、
  secure enclave 内の participant は発見できません。
- **ツール自身の痕跡。** ツールはドメインに自身の participant を 2 つ追加します (出力からは除外)。
- **検出できないケース。** ホスト id が同じで IPC 名前空間だけが別のノードは `shm-not-visible` に
  なりません。

## ライセンス

Apache-2.0
