# 実測 transport (`--stats`)

> 英語版が正です。この文書は 2026-09-06 時点の英語版に対応しています。

discovery のデータは「こうなる*はず*」を教えてくれます。`--stats` を付けると、ツールは
[Fast DDS statistics モジュール](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
のトピックも購読し、「実際に*こうなった*」を表示します。

| トピック | 用途 |
|---|---|
| `_fastdds_statistics_rtps_sent` | 各 participant が各宛先 locator に送った RTPS パケット数/バイト数。reader が広告した locator と突き合わせ、実際にパケットを運んだ locator の種類を得ます (`measured=SHM 47pkt`)。予測と食い違えば `!measured-transport-mismatch` を付けます。 |
| `_fastdds_statistics_history2history_latency` | writer のサンプルが特定の reader に届いたことの証明。RTPS の痕跡を残さない zero-copy data-sharing の確認に使います。 |
| `_fastdds_statistics_physical_data` | participant ごとのホスト名、ユーザー、プロセス id。`local` / `host:<id>` の代わりに表示します。 |
| `_fastdds_statistics_publication_throughput` | writer ごとの payload バイト数/秒。`RATE` 列 (トピックは writer の合算) と JSON (ペアの `measured.throughput_bytes_per_s`、トピックの `topics[].throughput_bytes_per_s`) に出ます。transport に依らないので zero-copy の data-sharing も定量化できます。 |
| `_fastdds_statistics_data_count` | 各 writer が transport 経由で送った DATA/DATA_FRAG サブメッセージ数。zero-copy 配送では増えないので、増えるかどうかで data-sharing が本当に使われたかが決まります ([data-sharing.ja.md](data-sharing.ja.md#確信度) を参照)。 |

## 観測対象ノードで statistics を有効にする

コードの変更は不要です。Fast DDS は participant 作成時に環境変数を読みます。

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC"
```

*writer* がこれ無しで起動されたペアには警告 `stats-not-enabled-on-writer` が付きます (statistics の
無い reader は警告されません)。

## カウンタが表すもの

`RTPS_SENT` のカウンタは writer の participant の起動からの累積です。ツールは観測中ずっと statistics の
reader を読み続け、最初と最後のサンプルの *差分* を `packets` / `bytes` として表示します
(`measured=SHM 148pkt 7.63 MB`)。累積値は JSON の `packets_total` / `bytes_total` に残ります。
`measured` の transport の種類は報告されたすべてのパケットから決めるので、以前は流れていたが観測中は
静かだったペアは、実測 transport を失わずに `measured=SHM (idle)` と表示されます。このセルの他の値:
`n/a` (writer の participant が statistics を出していない)、`none` (statistics はあるが reader のどの
locator にもパケットが無い)、`none(delivered)` (同じ状況で `HISTORY_LATENCY` が配送を証明している)。

## 粒度

statistics は *participant* 単位 (ROS ノードごとに 1 つ) なので、測定値は writer のノード →
reader のノードのリンクに対するものです。個々のペアを区別するのは discovery による予測の方です。
`--stats` はカウンタが溜まるように `--timeout` の間ずっと観測します (既定 5 秒。静穏期間による
早期終了は無効)。トラフィックの無いトピックには `!no-traffic-observed` が付きます。`HISTORY_LATENCY`
が配送を証明しているのに `RTPS_SENT` に reader のどの locator の項目も無い場合は、代わりに
`!delivered-without-measured-traffic` が付きます。サンプルは届いたが statistics がパケットを
帰属させなかったということです (遅いマシンで 2 MB のサンプルを既定の 512 KB セグメントの SHM で
流したときに見られました。SHM transport descriptor の `segment_size` を大きくすると改善します)。

## 落とし穴: 10 インスタンスの上限

Fast DDS 2.14 は statistics の DataWriter を既定のリソース上限 (10 インスタンス) で作ります。
`RTPS_SENT` は宛先 locator ごとにキーが付くので、10 を超える locator と通信するノード (相手が
数個あれば足ります。相手ごとに metatraffic、ユーザーデータ、SHM の locator があるため) は、
超過分を黙って報告しなくなります。ツールはこれを `!stats-writer-instance-limit-suspected` で
示します。

同梱のプロファイルで観測対象ノードの上限を外してください。Fast DDS は `FASTDDS_STATISTICS` に渡した
別名と同じ名前の `data_writer` プロファイルを適用します。ファイルにはキー付きの各トピックの分が
あります (`PHYSICAL_DATA` はインスタンスが 1 つなので不要)。

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC"
```

Fast DDS はプロファイルファイルを 1 つしか読みません。data-sharing を `--stats` で観測するときは、
このファイルと `datasharing_auto.xml` を結合した `datasharing_auto_stats.xml` を使います。
(ツール自身の statistics reader は最初からインスタンス数無制限です。)

## 実装メモ

- `RTPS_SENT` の送信元は *participant* の GUID で、`byte_count` は単純な累積バイト数です
  (`byte_magnitude_order` は `floor(log10(byte_count))` にすぎません)。
- statistics トピックの型サポート生成コードは同梱しています (Apache-2.0)。ROS ディストリビューション
  はコンパイル済みの型を Fast DDS ライブラリに含めていますが、ヘッダも `fastddsgen` も配布して
  いないためです。`src/fastdds_transport_viz/third_party/fastdds_statistics_types/` (Fast DDS
  2.14.6、Jazzy) と `.../fastdds_statistics_types_v3/` (Fast DDS 3.2.4、Kilted / Rolling) があり、
  CMake が Fast DDS のメジャーバージョンで選びます。別の Fast DDS を対象にするときは該当ディレクトリ
  を差し替えてください。
