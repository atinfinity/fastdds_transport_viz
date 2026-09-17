# 実測 transport (`--stats`)

> 英語版が正です。この文書は 2026-09-17 時点の英語版に対応しています。

discovery のデータは「こうなる*はず*」を教えてくれます。`--stats` を付けると、ツールは
[Fast DDS statistics モジュール](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
のトピックも購読し、「実際に*こうなった*」を表示します。

| トピック | 用途 |
|---|---|
| `_fastdds_statistics_rtps_sent` | 各 participant が各宛先 locator に送った RTPS パケット数/バイト数。reader が広告した locator と突き合わせ、実際にパケットを運んだ locator の種類 (`measured=SHM 47pkt`) と locator そのもの (`--locators`、JSON の `measured.locators[]`) を得ます。種類が予測と食い違えば `!measured-transport-mismatch`、種類は合っていても予測が選んだ locator に何も流れていなければ `!measured-locator-mismatch` を付けます。 |
| `_fastdds_statistics_history2history_latency` (LATENCY 列: write-to-notification 遅延の平均と最大、JSON の `measured.latency_s` とトピックの `latency_s`。ホスト間ではクロックのずれを含む) | writer のサンプルが特定の reader に届いたことの証明。RTPS の痕跡を残さない zero-copy data-sharing の確認に使います。 |
| `_fastdds_statistics_physical_data` | participant ごとのホスト名、ユーザー、プロセス id。`local` / `host:<id>` の代わりに表示します。 |
| `_fastdds_statistics_rtps_lost` | participant が取りこぼした RTPS パケット数 (シーケンス番号の欠落)。送信側 participant と、送信側が宛先にした自分の locator ごとに数えます。受信側 participant が publish するので、ペアの取りこぼしは reader の participant が writer の participant から reader の unicast locator 宛てに受け損ねたと報告した数です。`LOSS` 列の `lost` と警告 `rtps-packets-lost` になります (対象範囲は [RTPS_LOST](#rtps_lost) を参照)。 |
| `_fastdds_statistics_resent_datas`、`_fastdds_statistics_heartbeat_count`、`_fastdds_statistics_gap_count` | writer ごとの再送 DATA、HEARTBEAT、GAP の数。`resent` は `LOSS` 列のもう一方で、3 つとも JSON の `measured.reliability` に入ります。 |
| `_fastdds_statistics_acknack_count`、`_fastdds_statistics_nackfrag_count` | reader ごとの ACKNACK と NACKFRAG の数 (欠けたデータや断片を要求した回数)。JSON の `measured.reliability`。 |
| `_fastdds_statistics_data_count` | 各 writer が transport 経由で送った DATA/DATA_FRAG サブメッセージ数。zero-copy 配送では増えないので、増えるかどうかで data-sharing が本当に使われたかが決まります ([data-sharing.ja.md](data-sharing.ja.md#確信度) を参照)。 |

## レート列が無い理由と代わりの求め方

上の表に `_fastdds_statistics_publication_throughput` が無いのは意図的です。ツールはこのトピックを
購読せず、publish レートを表示しません ([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137))。この統計値はレートに見えますがレートでは
ありません。Fast DDS は `write()` ごとに 1 サンプルを publish し、その値は *そのサンプルの* payload を、
同じ writer の前回の `write()` からの間隔で割ったものです。writer が 1 回の書き込み間隔でどれだけ
速かったかを示すだけで、トピックが 1 秒あたりどれだけ運んでいるかではありません。バースト的に送って
黙る writer では両者は桁違いにずれます。後段で直すこともできません。ツールの statistics reader は
statistics reader はカウンタトピックについてインスタンスあたり 1 サンプルしか保持せず、50 ms ごとに
読み出すため、最新の値以外はツールが見る前に失われているからです。

`measured.throughput_bytes_per_s`、`topics[].throughput_bytes_per_s`、`stats.throughput` は、以前に
書かれた文書が検証を通り続けるように JSON に残し、それぞれ `null`、`null`、`{}` に固定しています。

JSON に実際に入っているのは累積カウンタと観測の長さなので、自分で計算するレートは定義がはっきり
しています。

| レート | 使う値 | 割る値 |
|---|---|---|
| writer の DATA サブメッセージ数/秒 | `stats.data_count[<writer の guid>].last` − `.first` | `observation_seconds` |
| participant がある locator に送った RTPS パケット数/バイト数の毎秒 | `stats.traffic[].packets` − `.packets_first`、`.bytes` − `.bytes_first` | `observation_seconds` |

`observation_seconds` はツールが観測した長さ (`--watch` では累積) で、`first` は 0 ではなくツールが
最初に見た値なので、差は観測期間中に起きたことそのものです。

これはワイヤ上のレートであって、アプリケーションの publish レートではありません。`DATA_COUNT` は
intraprocess や data-sharing の配送では増えず (だからこそ data-sharing の確認に使えます。
[data-sharing.ja.md](data-sharing.ja.md#確信度) を参照)、宛先 locator ごとに 1 回、さらに断片ごと・
再送ごとに数えます。`RTPS_SENT` はヘッダを含む RTPS パケット全体を数えます。ツール自身がレートを
表示する件は [#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143) で追っています。

## 観測対象ノードで statistics を有効にする

コードの変更は不要です。Fast DDS は participant 作成時に環境変数を読みます。

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

`qos-incompatible` と判定したペアは実測しません。それでも `HISTORY_LATENCY` が配送を証明した場合は
警告 `qos-incompatible-but-delivered` でマッチング規則の穴を知らせます。
`shm-ipc-namespace-split` のペアも、writer の SHM トラフィック (data-sharing のペアではハートビート) が
計測されても `NONE` のままで、配送が
証明されると `shm-ipc-namespace-split-but-delivered` が付きます。両端が SHM を広告するペアで
観測中に SHM 以外のパケットが流れると `shm-ipc-namespace-split-but-non-shm-traffic` が付きます
([IPC 名前空間の分断](how-it-works.ja.md#ipc-名前空間の分断) を参照)。
*writer* がこれ無しで起動されたペアには警告 `stats-not-enabled-on-writer` が付きます (statistics の
無い reader は警告されません)。ツールの statistics reader は UDP (または TCP) の locator だけを広告し
SHM の locator を広告しないので、別の IPC 名前空間にいる同一ホストの writer の statistics も届きます。
participant が SHM transport しか持たない writer はこれらの reader と共通の transport が無く、同じ警告に
なります。

## カウンタが表すもの

`RTPS_SENT` のカウンタは writer の participant の起動からの累積です。ツールは観測中ずっと statistics の
reader を読み続け、最初と最後のサンプルの *差分* を `packets` / `bytes` として表示します
(`measured=SHM 148pkt 7.63 MB`)。累積値は JSON の `packets_total` / `bytes_total` に残ります。
`measured` の transport の種類は報告されたすべてのパケットから決めるので、以前は流れていたが観測中は
静かだったペアは、実測 transport を失わずに `measured=SHM (idle)` と表示されます。このセルの他の値:
`n/a` (writer の participant が statistics を出していない)、`none` (statistics はあるが reader のどの
locator にもパケットが無い)、`none(delivered)` (同じ状況で `HISTORY_LATENCY` が配送を証明している)。
statistics が有効な participant とは、その participant 自身が publish した statistics のサンプルをツールが
受信したものです。この集合が JSON の `stats.participants_with_stats` で、フッタの
「statistics from N participant(s)」の N です。reader 側の `HISTORY_LATENCY` に出てくるリモートの writer
のように、他の participant のサンプルに名前が載っているだけの participant は数えません。

## 粒度

statistics は *participant* 単位 (ROS ノードごとに 1 つ) なので、測定値は writer のノード →
reader のノードのリンクに対するものです。個々のペアを区別するのは discovery による予測の方です。
`--stats` はカウンタが溜まるように `--timeout` の間ずっと観測します (既定 5 秒。静穏期間による
早期終了は無効)。トラフィックの無いトピックには `!no-traffic-observed` が付きます。`HISTORY_LATENCY`
が配送を証明しているのに `RTPS_SENT` に reader のどの locator の項目も無い場合は、代わりに
`!delivered-without-measured-traffic` が付きます。サンプルは届いたが statistics がパケットを
帰属させなかったということです (遅いマシンで 2 MB のサンプルを既定の 512 KB セグメントの SHM で
流したときに見られました。SHM transport descriptor の `segment_size` を大きくすると改善します)。

writer や reader 単位のカウンタ (`HISTORY_LATENCY`、`DATA_COUNT`、`RESENT_DATAS`、
`HEARTBEAT_COUNT`、`GAP_COUNT`、`ACKNACK_COUNT`、`NACKFRAG_COUNT`) には、
`<topic>/_buf_cpu` 上の native buffer のコンパニオン (Lyrical 以降の `rmw_fastrtps_cpp`。上限の無い
`uint8[]` フィールドを持つ型のサンプルを運ぶ) の値も含まれます。
[native buffer のコンパニオントピック](how-it-works.ja.md#native-buffer-のコンパニオントピック) を参照してください。

## RTPS_LOST

`RTPS_LOST` は、送信側 participant が RTPS パケットに付けるシーケンス番号の欠落を *受信側* participant が
見つけたときに publish します。Fast DDS は送信側 participant と宛先 locator ごとに番号を振るので、
項目 (JSON の `stats.lost[]`) は報告者 (`reporter_participant_guid_prefix`)、送信者
(`src_participant_guid_prefix`)、送信者が宛先にした報告者自身の locator (`dst_locator`) を持ちます。
ペアの `lost` は、reader の participant が writer の participant から reader の unicast locator 宛てに
受け損ねたと報告した数を、観測期間の最初のサンプルとの差分で表したものです。

- writer ではなく participant の組に属します。2 つの間のすべてのパケット (他のトピック、heartbeat、
  それらの locator 宛ての discovery トラフィック) が数えられ、同じ 2 つの participant 間のペアはすべて
  同じ数を示します。トピックの `lost_packets` は各項目を 1 回だけ数えます。
- マルチキャストの宛先は対象外です。Fast DDS は 1 回のマルチキャスト送信にソケットごとに番号を振る
  ことがあり、リモートの受信側はそれを取りこぼしとして報告してしまいます。
- 遅れて届いたパケットは数を減らします。観測の終わりに最初のサンプルを下回った場合は 0 と表示します。
- 番号を持つのは UDP と TCP のパケットだけです。SHM と data-sharing は取りこぼしを報告しません。
- 送信側は実行時の設定が不要で、statistics 付きでビルドされた Fast DDS であれば足ります。reader の
  participant には `FASTDDS_STATISTICS` の `RTPS_LOST_TOPIC` が必要で、無ければ取りこぼしは不明です。
  `LOSS` 列は `- lost`、JSON は `lost_packets: null` になり、他の信頼性カウンタはそのまま出ます。

## 落とし穴: 10 インスタンスの上限 {#instance-limit}

Fast DDS は 3.5 より前、statistics の DataWriter を既定のリソース上限 (10 インスタンス) で作ります
(Jazzy の 2.14。Humble の 2.6 のバイナリには statistics モジュールがありません)。
`RTPS_SENT` は宛先 locator ごとにキーが付くので、10 を超える locator と通信するノード (相手が
数個あれば足ります。相手ごとに metatraffic、ユーザーデータ、SHM の locator があるため) は、
超過分を黙って報告しなくなります。ツールはこれを `!stats-writer-instance-limit-suspected` で
示します。

Fast DDS 3.5 で既定の上限は無制限になりました。Lyrical と Rolling (3.6) では、インスタンス上限の
ためには下のプロファイルは不要です (別の理由で必要です。[次の落とし穴](#fast-dds-36)を
参照)。3.5 以降でビルドしたツールはこの警告を出しません。reader 宛ての
トラフィックが無いペアには、代わりに `delivered-without-measured-traffic` か `no-traffic-observed`
が付きます。判定はツールをビルドした Fast DDS で決まるので、Lyrical でビルドしたツールで Jazzy の
ノードを観測するときや、自分のプロファイルで `max_instances` を再び設定したときは、上限に気付けません。

同梱のプロファイルで観測対象ノードの上限を外してください。Fast DDS は `FASTDDS_STATISTICS` に渡した
別名と同じ名前の `data_writer` プロファイルを適用します。ファイルにはキー付きの各トピックの分が
あります (`PHYSICAL_DATA` はインスタンスが 1 つなので不要)。

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

Fast DDS 2.x は `FASTRTPS_DEFAULT_PROFILES_FILE` だけを、Fast DDS 3.x は
`FASTDDS_DEFAULT_PROFILES_FILE` だけを読みます。Lyrical と Rolling では rmw_fastrtps が
`FASTRTPS_DEFAULT_PROFILES_FILE` も読む (非推奨の警告付き) ので、上の行はサポート対象のどの
ディストリの ROS 2 ノードにも効きます。rmw を通さない Fast DDS 3.x のアプリケーションには
`FASTDDS_DEFAULT_PROFILES_FILE` が必要です。

Fast DDS はプロファイルファイルを 1 つしか読みません。data-sharing を `--stats` で観測するときは、
このファイルと `datasharing_auto.xml` を結合した `datasharing_auto_stats.xml` を使います。
(ツール自身の statistics reader は最初からインスタンス数無制限です。)

## 落とし穴: Fast DDS 3.6 でカウンタが停滞する {#fast-dds-36}

Fast DDS 3.6 (Lyrical、Rolling) では、カウンタの statistics DataWriter はほぼ周期 heartbeat
(既定 3 秒) でしか配送が進みません。サンプルはバーストと数秒以上の停滞を繰り返して reader に届き、
lost は 1 つも報告されず、`RTPS_SENT` のインスタンスが多いと 30 秒たっても大半が届きません。
その結果、20 プロセス・500 トピックの規模で、すべてのペアが `measured=none(delivered)` と
`delivered-without-measured-traffic` になります
([#152](https://github.com/atinfinity/fastdds_transport_viz/issues/152)。Jazzy の 2.14 では
起きません)。reader から heartbeat を要求することはできないので、ツール側では直せません。

同梱のプロファイルは、カウンタの writer (`HISTORY_LATENCY_TOPIC` 以外のすべてのプロファイル) の
`heartbeat_period` を、パッケージを Fast DDS 3.x でビルドしたときに 500 ms にして、連続した配送に
戻します。Fast DDS 2.x ではこの要素の綴りが `heartbeatPeriod` で、パースできないプロファイルは丸ごと
捨てられるため、インストールされる `statistics.xml` と `datasharing_auto_stats.xml` は CMake が
`config/*.xml.in` からビルド対象の Fast DDS 向けに生成します。観測対象ノードのマシンにインストール
されたファイルを使ってください。500 ms は100 ms、250 ms、500 ms、1 s のうち、
medium の規模で `--watch` のカバレッジを 1.0 に保てた最長の値です (1 s では 0.21)。どのディストリでも、
[前の節](#instance-limit)の 2 行で観測対象ノードにこのプロファイルを適用してください。

## reader の QoS

[#141](https://github.com/atinfinity/fastdds_transport_viz/issues/141) までは、ツールの 10 個の
statistics reader はすべて Fast DDS の既定値を使っていました。reliable、transient-local、keep-last 1、
そして目立たないところで**インスタンス数の上限 10** です。このインスタンス上限は、このページが
writer 側について警告しているものと同じで、reader にも等しく効きます。statistics のトピックはキー付き
なので、`(送信元, locator)` のインスタンスが 10 個を超えると、その participant のサンプルを reader は
一切受け取らなくなります。現在はインスタンス数を無制限にし、トピックごとに次のように使い分けます。

| トピック | 信頼性 | 永続性 | depth | 理由 |
|---|---|---|---|---|
| 8 つのカウンタトピック (`rtps_sent`、`rtps_lost`、`data_count`、`resent_datas`、`heartbeat_count`、`acknack_count`、`nackfrag_count`、`gap_count`) | reliable | transient-local | 1 | ツールが報告するのは `last - first` で、`first` は観測開始より前のサンプル。これを失うと 1 サンプルではなくエンティティ 1 つ分の実測値が失われる |
| `_fastdds_statistics_physical_data` | reliable | transient-local | 1 | participant ごとに 1 サンプル、discovery 時に 1 度だけ publish される。2 つ目は同じことしか言わない |
| `_fastdds_statistics_history2history_latency` | best-effort | volatile | 10 | 群を抜いて量が多く、運ぶ値は累積ではなく、`first` を一切見ない唯一のトピック |

累積カウンタにはインスタンスあたり 1 サンプルで足ります。ツールが必要とするのは最新の値だけで、
observer 自身のスレッドが到着から 50 ms 以内にそれを取り出すからです。10 サンプル保持する案は実測の
うえで棄却しました。実測できるペアは 1 つも増えないまま、CPU を 0.2 コア近く余分に使い、`--watch` の
フレーム p95 を倍にしたためです。

`max_samples` は無制限のままで、上限を掛けるのは `max_samples_per_instance` だけです。合計で上限を
掛けると、インスタンスが増えた時点でサンプルの受け取りを拒否し始め、その拒否は `samples_rejected`
として数えられます。上限で防ごうとしている損失そのものです。

`HISTORY_LATENCY` を reliable で受け取るのも割に合いません。20 プロセス・2400 ペアでの実測では、
acknack と再送によって coverage が #141 以前 (0.947) を下回る 0.746 まで落ち、`--watch` のフレーム
p95 は 20 秒に達しました。best-effort にすると、代わりにこのトピックの損失が見えるようになります
(後述の `stats.samples_lost_latency`)。これは悪化ではなく報告です。

reader の読み出しは observer が持つスレッドが 50 ms ごとに行います。両モードで、表示を一時停止して
いる間も動き続けます (以前のように `--interval` ごとではありません)。これらのトピックで損失を決める
のは 2 回の読み出しの間隔なので、depth よりも読み出しの頻度が効きます。

## 大規模なシステム

Docker の 8 CPU の VM で Jazzy (Fast DDS 2.14.6) を使い、すべてのノードで statistics を有効にし、
ツールをノードと同じ場所で動かして測りました (詳細は [development.md](development.md#scale-results))。

- **約 10 プロセス、500 ペアまで**は取りこぼしが無く、既定の 5 秒ですべてのペアが実測されます。
- **20 プロセス、2400 ペア**でも 5 秒ですべてのペアが実測され、ワンショットの表にも全ペアが出ます。
  [#141](https://github.com/atinfinity/fastdds_transport_viz/issues/141) より前は 4 分の 1 のペアが 5 秒後も未実測で、表に出るのは 1 ペアだけでした。
- **40 プロセス、5600 ペア**でも大半から全部のペアが実測されます。同じビルドの 3 回の実行で 5 秒
  時点の coverage は 0.62 / 0.97 / 1.0 でした。#141 より前は 3 回とも 0.0 です。このばらつきは
  ツールではなくホスト側の事情で、この規模では負荷だけで 8 コア中 6.7〜7.5 コアを使ってしまいます。
  この規模では `--watch --stats` の 1 フレームに依然として 1.6 秒かかります (`--stats` なしで 0.44 秒)。
  20 プロセスでは 0.15 秒で、[#135](https://github.com/atinfinity/fastdds_transport_viz/issues/135) 以降は 250 ms の予算内です。

ツールは statistics をすべて UDP で受け取り、Fast DDS はそれを 1 本の受信スレッドで処理します。
Nav2 (4 participant、1195 ペア) の隣でも、このスレッドだけで 1 コアを使い切ります。損失を決めるのは
reader からどれだけ頻繁に読み出すかで、そのための 50 ms の読み出しが上記のスレッドです。それを
超えると、送信側の keep-last の履歴が届く前のサンプルを上書きします。

ツールは落としたサンプルを報告します。JSON 文書の `stats.samples_lost` が届かなかった statistics
サンプルの数で、表の `statistics:` 行にも同じ数が出ます。その損失で実測まで失われたとき (後述) は、
文書に警告コード `stats-samples-lost` が付き、ワンショット実行では stderr に 1 行出ます:

```
warning: 682142 of 690671 statistics samples were lost (the tool could not keep up) and 37 of 2400 pairs with proven deliveries show no measured packet - enable statistics on fewer nodes, or keep FASTDDS_STATISTICS to the aliases you need (e.g. RTPS_SENT_TOPIC;RTPS_LOST_TOPIC)
```

`--watch` ではこの行は出しません。値はフレームごとに増えるだけで、`statistics:` のフッタが既に
表示しているからです。カウンタは実行全体の累積なので、損失の無いフレームが来ても、それ以前に
取り逃した実測値が戻るわけではありません。

サンプルを落とすことと実測を失うことは別です。40 プロセスの実行では 1040228 個のうち 925130 個を
落としながら、すべての `/scale` ペアを実測できました。statistics のカウンタは累積値で、ツールは
観測窓の `last - first` を報告するので、受け取れた 2 つのサンプルの間にある届かなかったサンプルは
何も変えません。実測が失われるのは、あるペアの locator の `RTPS_SENT` サンプルがまったく届かず、
引く相手が無いときだけです。ただしインスタンスだけを見てもそれは分かりません。サンプルが 1 つで
差が 0 というのは、ツールの起動後に何も送られていない locator の姿でもあり、20 プロセスの実行では
サンプルを 1 つも落としていないのに 1 割のインスタンスがそうでした。両者を区別できるのは配送の
証拠です。[#147](https://github.com/atinfinity/fastdds_transport_viz/issues/147) 以降、文書は `HISTORY_LATENCY` が配送を証明しているペアの数
(`stats.pairs_delivered`) と、そのうち実測パケットが無いペアの数
(`stats.pairs_delivered_unmeasured`) を持ち、警告は両方がそろったときだけ出ます。つまり、
カウンタのサンプルが失われた (または拒否された) こと**と**、そうしたペアが 1 つ以上あることです。
損失だけなら数として報告されるだけで警告にはなりません。損失の無い未実測ペアには、従来どおり
ペア単位の `delivered-without-measured-traffic` が付きます。data-sharing のペア、
`qos-incompatible` や IPC 分断のペア、`stats-writer-instance-limit-suspected` のペアはどちらの
数にも入らず、配送の証拠が無いペアは `no-traffic-observed` が説明するあいまいさのままです。
数はフレームごとに数え直すので、`--watch` では実測が戻れば警告も消えます。

`stats.samples_lost_latency` は別に数えます。`stats.samples_lost` の**内数**であって並ぶ数では
なく、これを警告の対象にするものはありません。`HISTORY_LATENCY` は設計として best-effort で
受け取る ([Reader QoS](#reader-qos)) ため、その reader は最も賑やかなトピックのシーケンスの
抜けをすべて報告します。40 プロセスでは 60 秒の実行で 170 万〜190 万に達します。これらは独立した
観測値で、ツールはそれを平均と最大に畳むので、落ちても出ている数値が粗くなるだけです。表では
別項目として出し (`..., 42 sample(s) lost, 1000 latency sample(s) lost`)、web ビューアも同様で、
`stats-samples-lost` の警告はこれを数えません。

この警告が出ているときは、ペアの `no-traffic-observed` が「実測できなかった」の意味にもなり得ます。
カウンタは 10 個の statistics reader で共有しているため、損失を特定のペアに帰属させられません。
見る範囲を絞ってください:
- 見たいノードでだけ statistics を有効にする;
- `FASTDDS_STATISTICS` を必要な別名 (例: `RTPS_SENT_TOPIC;RTPS_LOST_TOPIC`) に絞る。

`--timeout` を延ばしても損失は減りません (より多く集めるだけです)。ただし各インスタンスが 2 回
サンプリングされる機会は増えます。上の coverage はまさにそれを比べたもので、5 秒時点の実測と
30 秒時点の実測の比です。

`stats.samples_lost_at_start` は別に数えられ、警告にはなりません。reader はマッチした時点で、
writer の keep-last 履歴が既に捨てていたサンプルをすべて「失われた」と通知されますが、これは
ツールが追いつけているかとは無関係です。しかもこの通知は遅れて届きます。writer は捨てた分を
マッチ時ではなく次以降の heartbeat で知らせるためで、静穏な 5 ノード系での実測では、マッチから
jazzy で最大 1.2 秒、lyrical で最大 3.9 秒あとに届きました。新しい statistics writer は必ずこの
バーストを伴うので、「直近 5 秒以内に新規マッチがある間」の損失を late-join と数えます。1 度も
マッチしていない間も同じ扱いです（猶予時間の起点となるマッチがまだ無いため）。途中から起動した
ノードが言い訳にできるのは 5 秒分だけで、その後も続く損失は計上されます。

## 実装メモ

- ツールは participant を作る前に自分の環境から `FASTDDS_STATISTICS` を取り除きます。設定された
  ままだと Fast DDS 2.14 はツールの statistics reader を載せた participant にも statistics writer を
  追加し、reader が acknack を送る途中の `on_rtps_sent()` 内でデッドロックすることがあります。
  ツール自身の statistics は不要です (自身のエンドポイントは除外されます)。
- `RTPS_SENT` の送信元は *participant* の GUID で、`byte_count` は単純な累積バイト数です
  (`byte_magnitude_order` は `floor(log10(byte_count))` にすぎません)。
- Fast DDS はツールと同じホストにいる participant の locator を `127.0.0.1` に変換して見せます
  (localhost 変換) が、別ホストの writer の `RTPS_SENT` はその participant の実アドレス宛ての
  トラフィックを報告します。そのため重ね合わせでは、loopback の reader locator を同じポートの
  ツールのホストの任意のアドレスと同一視します。これが無いと、reader がツールと同じホストにいる
  ホスト間ペアは `delivered-without-measured-traffic` になっていました。
- statistics トピックの型サポート生成コードは同梱しています (Apache-2.0)。ROS ディストリビューション
  はコンパイル済みの型を Fast DDS ライブラリに含めていますが、ヘッダも `fastddsgen` も配布して
  いないためです。`src/fastdds_transport_viz/third_party/fastdds_statistics_types/` (Fast DDS
  2.14.6、Jazzy) と `.../fastdds_statistics_types_v3/` (Fast DDS 3.2.4 から生成、Lyrical / Rolling で使用) があり、
  CMake が Fast DDS のメジャーバージョンで選びます。別の Fast DDS を対象にするときは該当ディレクトリ
  を差し替えてください。
