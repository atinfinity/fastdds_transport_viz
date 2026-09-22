# 実測 transport (`--stats`)

> 英語版が正です。この文書は 2026-09-22 時点の英語版に対応しています。

discovery のデータは「こうなる*はず*」を教えてくれます。`--stats` を付けると、ツールは
[Fast DDS statistics モジュール](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
のトピックも購読し、「実際に*こうなった*」を表示します。

| トピック | 用途 |
|---|---|
| `_fastdds_statistics_rtps_sent` | 各 participant が各宛先 locator に送った RTPS パケット数/バイト数。reader が広告した locator と突き合わせ、実際にパケットを運んだ locator の種類 (`measured=SHM 47pkt`) と locator そのもの (`--locators`、JSON の `measured.locators[]`) を得ます。種類が予測と食い違えば `!measured-transport-mismatch`、種類は合っていても予測が選んだ locator に何も流れていなければ `!measured-locator-mismatch` を付けます。 |
| `_fastdds_statistics_history2history_latency` | writer → reader の各ペアの write-to-notification 遅延。`LATENCY` として表示し (観測期間中の平均と最大。JSON の `measured.latency_s`、トピックの `latency_s` は最も遅いペアの値)、届いたレート `HZ` (届いたサンプル 1 つにつき 1 サンプルをペアごとに数える。JSON の `measured.delivered_per_s`、[後述](#hz-列-1-秒あたりに届いたサンプル数)) にもなり、さらに存在するだけで、サンプルがその reader に届いたことの証明になります (RTPS の痕跡を残さない zero-copy data-sharing の確認に使います)。ホスト間ではクロックのずれを含みます。 |
| `_fastdds_statistics_physical_data` | participant ごとのホスト名、ユーザー、プロセス id。`local` / `host:<id>` の代わりに表示します。 |
| `_fastdds_statistics_rtps_lost` | participant が取りこぼした RTPS パケット数 (シーケンス番号の欠落)。送信側 participant と、送信側が宛先にした自分の locator ごとに数えます。受信側 participant が publish するので、ペアの取りこぼしは reader の participant が writer の participant から reader の unicast locator 宛てに受け損ねたと報告した数です。`LOSS` 列の `lost` と警告 `rtps-packets-lost` になります (対象範囲は [RTPS_LOST](#rtps_lost) を参照)。 |
| `_fastdds_statistics_resent_datas`、`_fastdds_statistics_heartbeat_count`、`_fastdds_statistics_gap_count` | writer ごとの再送 DATA、HEARTBEAT、GAP の数。`resent` は `LOSS` 列のもう一方で、3 つとも JSON の `measured.reliability` に入ります。 |
| `_fastdds_statistics_acknack_count`、`_fastdds_statistics_nackfrag_count` | reader ごとの ACKNACK と NACKFRAG の数 (欠けたデータや断片を要求した回数)。JSON の `measured.reliability`。 |
| `_fastdds_statistics_data_count` | 各 writer が transport 経由で送った DATA/DATA_FRAG サブメッセージ数。zero-copy 配送では増えないので、増えるかどうかで data-sharing が本当に使われたかが決まります ([data-sharing.ja.md](data-sharing.ja.md#確信度) を参照)。 |

## HZ 列: 1 秒あたりに届いたサンプル数

`HZ` は writer のサンプルが reader に届いたレートで、ツールがペアの `HISTORY_LATENCY` サンプルを
数えて求めます ([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143))。Fast DDS は reader の履歴に受け入れたサンプル 1 つにつき
1 サンプルを、どの配送経路でも (SHM や UDP の RTPS、同一 participant 内の intraprocess、zero-copy
の data-sharing) publish するので、ツールはそれを writer → reader のペアごとに数えます。サンプルが
`n` 個で、その source timestamp の幅が `t` なら、レートは `(n − 1) / t` です。2 サンプル未満では
レートは無く、セルは空のままです (`--stats` 無しでも空)。この列はペア行だけにあります。トピックに
1 つのレートは無く、writer のレートはその reader 行ごとに同じ数が出るもので、合計ではありません。
100 以上は整数、それ未満は小数 1 桁で表示します (`120`、`9.9`)。JSON では
`measured.delivered_per_s` (無ければ `null`)、`delivered_per_s_lower_bound`、
`delivered_per_s_window_s` です。

窓は一回きりの実行では観測全体 (`delivered_per_s_window_s` = `observation_seconds`)、`--watch` では
source timestamp の直近 5 秒で、フレームには開始からの平均ではなく今のレートが出ます。

これは届いたレートであって publish レートではありません。reader の履歴が拒んだサンプルや、
マッチする相手が無くて writer が送らなかったサンプルは含まれません。数えられる上限は、ツール自身の
`HISTORY_LATENCY` reader が 2 回の読み出しの間に保持できる量 (インスタンスあたり 100 を 50 ms ごと、
[reader の QoS](#reader-の-qos) を参照) で決まり、ペアあたり 1000 サンプル/秒を 0.1 % 以内で
数えます。2 プロセス間の SHM、intraprocess、data-sharing の各ペアで 10、100、1000 Hz を Jazzy と
Lyrical で検証しました (`scripts/integration_test.sh rate_stats`、許容 ±3 %、
[verification-log.md](verification-log.md#verification-results) を参照)。このトピックのサンプルがツールに届く
途中で失われたときは、レートは下限になり `≥120` と表示します (`delivered_per_s_lower_bound: true`)。
ペアの `HISTORY_LATENCY` を publish する statistics writer は reader 側の participant のもので、
その participant の全ペアのサンプルを 1 本のシーケンスで番号付けするため、抜けは participant には
帰属できてもペアには帰属できません。そこで、その participant に reader を持つすべてのペアに `≥` が
付きます。ツールの reader の中で読み出し前に上書きされたサンプルも同じ抜けを残すだけで、
`stats.samples_lost_latency` には数えられません。best-effort の reader での上書きを Fast DDS は
lost sample として報告しないからです。

`_fastdds_statistics_publication_throughput` は引き続き購読しません ([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137))。この統計値は
レートに見えますがレートではありません。Fast DDS は `write()` ごとに 1 サンプルを publish し、その値は
*そのサンプルの* payload を、同じ writer の前回の `write()` からの間隔で割ったもので、writer が 1 回の
書き込み間隔でどれだけ速かったかを示すだけで、トピックが 1 秒あたりにどれだけ運ぶかは決して示しません。`measured.throughput_bytes_per_s`、
`topics[].throughput_bytes_per_s`、`stats.throughput` は、以前に書かれた文書が検証を通り続けるように
JSON に残し、それぞれ `null`、`null`、`{}` に固定しています。

JSON には累積カウンタと観測の長さも入っているので、ワイヤ上のレートも定義がはっきりしています。

| レート | 使う値 | 割る値 |
|---|---|---|
| writer の DATA サブメッセージ数/秒 | `stats.data_count[<writer の guid>].last` − `.first` | `observation_seconds` |
| participant がある locator に送った RTPS パケット数/バイト数の毎秒 | `stats.traffic[].packets` − `.packets_first`、`.bytes` − `.bytes_first` | `observation_seconds` |

`observation_seconds` はツールが観測した長さ (`--watch` では累積) で、`first` は 0 ではなくツールが
最初に見た値なので、差は観測期間中に起きたことそのものです。`DATA_COUNT` は intraprocess や
data-sharing の配送では増えず (だからこそ data-sharing の確認に使えます。
[data-sharing.ja.md](data-sharing.ja.md#確信度) を参照)、宛先 locator ごとに 1 回、さらに断片ごと・
再送ごとに数えます。`RTPS_SENT` はヘッダを含む RTPS パケット全体を数えます。

## 観測対象ノードで statistics を有効にする

コードの変更は不要です。Fast DDS は participant 作成時に環境変数を読みます。

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

`qos-incompatible` と判定したペアは実測しません。それでも `HISTORY_LATENCY` が配送を証明した場合は
警告 `qos-incompatible-but-delivered` でマッチング規則の穴を知らせます。
`type-name-mismatch` と判定したペアも同じ扱いで、こちらは
`type-name-mismatch-but-delivered` です ([#85](https://github.com/atinfinity/fastdds_transport_viz/issues/85))。
`type-information-mismatch` も同様で、`type-information-mismatch-but-delivered` になります
([#213](https://github.com/atinfinity/fastdds_transport_viz/issues/213))。
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
静かだったペアは、実測 transport を失わずに `measured=SHM (idle)` と表示されます。その観測中に
`HISTORY_LATENCY` が配送を証明していれば、ペアは idle ではなく `RTPS_SENT` のサンプルが届かなかった
のであり、代わりに `measured=SHM (unmeasured, delivered)` と `!delivered-without-measured-traffic`
が付きます ([#149](https://github.com/atinfinity/fastdds_transport_viz/issues/149))。このセルの他の値:
`n/a` (writer の participant が statistics を出していない)、`none` (statistics はあるが reader のどの
locator にもパケットが無い)、`none(delivered)` (同じ状況で `HISTORY_LATENCY` が配送を証明している)。
同一プロセス内で配送されるペアは、`none(delivered)` や `<kind> (unmeasured, delivered)` の代わりに
`none(intra-process)` または `<kind> (intra-process)` (例: `SHM (intra-process)`) と表示します。パケットを取りこぼしたのではなく、そもそも 1 つも送られていない
からです ([プロセス内のペア](#プロセス内のペア) を参照)。
statistics が有効な participant とは、その participant 自身が publish した statistics のサンプルをツールが
受信したものです。この集合が JSON の `stats.participants_with_stats` で、フッタの
`statistics: <S> samples from <N> participant(s)` の `<N>` です。reader 側の `HISTORY_LATENCY` に出てくるリモートの writer
のように、他の participant のサンプルに名前が載っているだけの participant は数えません。

## 粒度

statistics は *participant* 単位 (ROS ノードごとに 1 つ) なので、測定値は writer のノード →
reader のノードのリンクに対するものです。個々のペアを区別するのは discovery による予測の方です。
`--stats` はカウンタが溜まるように少なくとも 5 秒観測し、その後は discovery が `--quiet` 秒
静穏で、ツールの reader がマッチした `RTPS_SENT` writer (statistics 付きの participant ごとに
1 つ) のすべてから最初のサンプルが届き、**かつ**発見済み reader が受信する locator 宛てに実測
パケットを持つ `RTPS_SENT` エントリの数が `--quiet` 秒 (少なくとも 3 秒) 増えなくなるまで、
または `--timeout` (`--stats` 付きの既定は 30 秒) まで、のどちらか早い方まで続けます。この規則が
働くのは、`--quiet` が 0 より大きく `--timeout` が最小の 5 秒の窓より長い一発実行だけです。
`--quiet 0` では実行は `--timeout` いっぱいまで続き、`--timeout` が 5 秒以下では規則を確かめる前に
実行が終わるので、どちらの場合も `stats.settled` は false、`discovery.stopped_on` は `timeout` に
なります。そもそも
1 つも実測しえない実行は、最小の観測窓が終わった時点で settle します
([プロセス内のペア](#プロセス内のペア))。数える
のは reader 宛てのエントリだけです
([#179](https://github.com/atinfinity/fastdds_transport_viz/issues/179))。multicast の
metatraffic 宛てやツール自身のポート宛てのエントリはどの participant でも数秒で動くため、それらを
数えていた実装では 47 エントリで 8 秒に settle し、ペアは 1 つも実測できていませんでした。reader
の宛先はその unicast ポート**と** reader が announce した multicast グループです
([#196](https://github.com/atinfinity/fastdds_transport_viz/issues/196))。これは実測の帰属で
使っている規則そのもので、unicast locator を持たずグループだけを announce する reader のペアは
そのグループのエントリでしか測れないため、unicast だけを数えていると全パケットを帰属できていても
settle せず `--timeout` まで待って警告を出していました。ここで metatraffic の除外は不要です。
`239.255.0.1:7400` は *participant* の locator で、endpoint が announce することはありません。
後の 2 つの条件は固定の窓では切れてしまっていたものです
([#168](https://github.com/atinfinity/fastdds_transport_viz/issues/168))。transient-local の
カウンタ writer は後から参加した reader に履歴をまとめて渡し、それはプロセスごとに順番に、間に
最大 2 秒ほどの間を置いて起こるので、100 ペアずつの 20 プロセスでは最後の writer からの最初の
サンプルが 16〜20 秒後に届き、エントリは約 25 秒まで増え続け、5 秒の観測では、ある実行ではペアの
一部しか実測できず、次の実行では 1 つも実測できませんでした。`--json` ではこの規則が `stats.writers_announced` (マッチした
`RTPS_SENT` writer 数)、`stats.writers_heard`、`stats.measured_instances` (reader 宛てに
実測できたエントリ数)、`stats.measurable_pairs` (両端が別プロセスにあるペア数)、
`stats.settled`、`stats.settled_at_s` (先に `--timeout` に達した場合は
`null` で、そのとき stderr に 1 行、まだ届いていない writer、または reader 宛てのエントリが 1 つも
実測できなかったことが出ます) に記録され、
`discovery.stopped_on` は `settled` になります。`--watch --stats` はこの規則を待ちません。最初の
フレームは discovery が静かになり 5 秒が経った時点で出て (`--timeout` が上限)、履歴の受け渡しで
届く分は後のフレームに載ります
([#177](https://github.com/atinfinity/fastdds_transport_viz/issues/177))。それでも数える対象は同じで、
`stats.measured_instances` は出力するスナップショットの reader 宛てかどうかで判定されます。これは
どのモードでも変わりません (`--quiet` や `--timeout` をどう指定した一発実行でも、`--watch` の各フレーム
でも、[#200](https://github.com/atinfinity/fastdds_transport_viz/issues/200))。トラフィックの無いトピックには
`!no-traffic-observed` が付きます。`HISTORY_LATENCY`
が配送を証明しているのに `RTPS_SENT` に reader のどの locator の項目も無い、または観測前の項目しか
無い (`measured=SHM (unmeasured, delivered)`) 場合は、代わりに
`!delivered-without-measured-traffic` が付きます。サンプルは届いたが statistics がパケットを
帰属させなかったということです (遅いマシンで 2 MB のサンプルを既定の 512 KB セグメントの SHM で
流したときに見られました。SHM transport descriptor の `segment_size` を大きくすると改善します)。

writer や reader 単位のカウンタ (`HISTORY_LATENCY`、`DATA_COUNT`、`RESENT_DATAS`、
`HEARTBEAT_COUNT`、`GAP_COUNT`、`ACKNACK_COUNT`、`NACKFRAG_COUNT`) には、
`<topic>/_buf_cpu` 上の native buffer のコンパニオン (Lyrical 以降の `rmw_fastrtps_cpp`。上限の無い
`uint8[]` フィールドを持つ型のサンプルを運ぶ) の値も含まれます。
[native buffer のコンパニオントピック](how-it-works.ja.md#native-buffer-のコンパニオントピック) を参照してください。

## プロセス内のペア

同一プロセスの writer と reader は participant の内部で配送され、サンプルが transport に載ることは
ありません。したがってこのペアで `RTPS_SENT` のカウンタが動くことはありません (ネットワークに渡した
DATA サブメッセージを数える `DATA_COUNT` も同様です)。一方で `HISTORY_LATENCY` は動きます。配送は証明され時間も測れるが、数えるべきパケットが無いという
ことです。ツールは GUID prefix からこのペアを `intra-process` と名付け
([プロセス内配送](how-it-works.ja.md#プロセス内配送-intra-process))、実測が期待される場所では
data-sharing のペアと同じ扱いにします。配送の証拠があれば `MEASURED` 列は `none(intra-process)`
または `<kind> (intra-process)` と表示し、ペアは `stats.pairs_delivered` から外れ、したがって
`stats.pairs_delivered_unmeasured`、`stats.pairs_delivered_absent`、`rtps-sent-absent`、
`stats_watch_coverage` ([規模の検証](development.md#scale-verification)の `--watch` statistics
coverage の予算) の分母からも外れます。ペア単位の警告も付きません。
配送の証拠が無ければ、何も実測されなかった他のペアと同じく `no-traffic-observed` が付きます。
実測パケットは予測に優先します。観測中に writer の participant が reader の locator 宛てにパケットを
送っていれば (独自の `<prefix>`、プロセス内配送を無効にしてビルドした Fast DDS、他所の reader と
共有する multicast グループ)、`intra-process` の理由を外し、カウンタが示すとおりにペアを報告します。

settle 規則は同じ事実を実行ごとに 1 度だけ `stats.measurable_pairs` として数えます。これは、その時点
までに発見したエンドポイントのうち両端が*別プロセス*にあるペアの数で、ツール自身の participant は
除きます。この数は QoS の適合性を見ずにトピックごとに writer × reader を数える過大評価ですが、
「何かが実測されうる」の下限であるためにはそれが必要です。`measurable_pairs` が 0 なら規則は待つ
ものが無く、最小の 5 秒の窓で実行が終わり `discovery.stopped_on` は `settled` になります。
[#201](https://github.com/atinfinity/fastdds_transport_viz/issues/201) 以前は、プロセス内配送だけの
システムでは `--stats` のたびに `--timeout` を使い切り、そのうえで reader 宛ての `RTPS_SENT` を 1 つも
実測できなかったと警告し、効きようのない対処を提示していました。publisher しか無いシステム (送る先の
reader が無い) も同様です。タイムアウト時の警告のもう半分、announce された writer からまだ届いていない、
は今までどおり出ます。

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
- マルチキャストの宛先は対象外です。その理由は
  [#130](https://github.com/atinfinity/fastdds_transport_viz/issues/130) で計測しました。Fast DDS は
  シーケンス番号をトランスポートのソケットごとの `send()` の中で、宛先 locator 単位のカウンタから
  書き込みます。1 回のマルチキャスト送信は any アドレスのソケットとインタフェースごとのソケットの
  両方から出るため、インタフェースが N 個ある送信側は、受信側が 1 回しか受け取らないメッセージに
  N+1 個の番号を使い、受信側は届かなかった N 個を取りこぼしとして報告します。実測の
  `RTPS_LOST` / `RTPS_SENT` は、Fast DDS 2.14.6 と 3.6.2 でそれぞれ 12 回観測して、インタフェース
  1 個で 0.91-1.03、2 個で 1.63-2.00、3 個で 2.29-2.98 でした。送信側と受信側が同じネットワーク
  名前空間にいてすべての複製が届く場合は、どの観測も 0.00 でした。
  ネットワーク上で失われているものはありません。
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
捨てられるため、インストールされる `statistics.xml`、`datasharing_auto_stats.xml`、
`multicast_user_stats.xml` (このファイルに multicast locator 上のユーザー endpoint を結合したもの。
[#130](https://github.com/atinfinity/fastdds_transport_viz/issues/130) の
[multicast stamping experiment](development.md#multicast-stamping-experiment) 用) は CMake が
`config/*.xml.in` からビルド対象の Fast DDS 向けに生成します (writer のプロファイル自体は
`config/statistics_writers.xml.in` の 1 か所にだけあります)。観測対象ノードのマシンにインストール
されたファイルを使ってください。500 ms は 100 ms、250 ms、500 ms、1 s のうち、
[規模の検証](development.md#scale-verification)の medium の規模で `--watch` のカバレッジを 1.0 に保てた最長の値です (1 s では 0.21)。どのディストリでも、
[前の節](#instance-limit)の 2 行で観測対象ノードにこのプロファイルを適用してください。

## 同梱プロファイルが writer に対して変えているもの {#writer-qos}

statistics のエイリアス名を持つプロファイルは writer の QoS を**丸ごと**置き換えます。Fast DDS は
自前で組み立てる QoS (statistics モジュールの `DataWriterQos.cpp`、2.14 と 3.6 で同じ) とマージしません。
Fast DDS が組み立てるのは reliable、transient-local、keep-last 10、
`FastDDSStatisticsFlowControllerDefault` 上の asynchronous (専用の送信スレッドなので、非同期 writer の
ユーザーデータの後ろに statistics が並ぶことはない)、そしてプロパティ `fastdds.push_mode=false`、
つまり *pull mode* です。pull mode では reliable な remote reader は heartbeat に ACKNACK で応えるまで
何も送られず、サンプルは heartbeat の周期で流れます。

同梱プロファイルはこのプロパティ以外を保っています
([#154](https://github.com/atinfinity/fastdds_transport_viz/issues/154))。writer は statistics 用の
flow controller を名指しし、**push mode** で動いて、書かれたサンプルをその場で送ります。`HISTORY_LATENCY`
だけは keep-last 10 ではなく **100** です
([#170](https://github.com/atinfinity/fastdds_transport_viz/issues/170))。届いたメッセージごとに
1 サンプル出すので、10 だと 1000 Hz で送信スレッドが 10 ms 遅れただけで未送信のサンプルが上書きされ、
ツールはレートを下限として報告することになります。100 はツール自身の reader の深さで、周期的な
カウンタは 10 のままです。pull mode は
medium の規模 (20 プロセス、500 トピック) で計測して不採用にしました。heartbeat が既定の 3 s のままの
Jazzy (2.x には `heartbeat_period` を付けない、上の節を参照) では、ツールの `--watch` は 60 s の間に
`HISTORY_LATENCY` の証拠を 1 つも見ず、1 回の実行あたり 2000〜3300 のカウンタサンプルが lost と
報告されました。reader が要求する前に keep-last 10 がインスタンスのサンプルを上書きするためです。
500 ms heartbeat の Lyrical では pull mode でも全ペアを計測でき、ツールの CPU は 3 分の 1 減りました
(0.7 コアが 0.49 に) が、バージョンで分岐を増やすより両ディストリで同じ配信機構を取りました。
数値は [verification-log.md](verification-log.md#verification-results) にあります。

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
| `_fastdds_statistics_history2history_latency` | best-effort | volatile | 100 | 群を抜いて量が多く、運ぶ値は累積ではなく、`first` を一切見ない唯一のトピック。サンプルは `HZ` のためにペアごとに数えるので、2 回の読み出しの間に収まる量が数えられるレートの上限になる。10 ではペアあたり 200 サンプル/秒付近で飽和し、100 なら 1000/秒を数える ([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143)) |

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
ツールをノードと同じ場所で動かして測りました (詳細は [verification-log.md](verification-log.md#scale-results))。

- **約 10 プロセス、500 ペアまで**は取りこぼしが無く、5 秒ですべてのペアが実測され、既定の `--stats` ワンショットはその数秒後に終わります。
- **20 プロセス、2400 ペア**では既定の `--stats` ワンショットが 17〜23 秒で settle し、ペアの 95〜100 % が実測され、ワンショットの表には全ペアが出ます。5 秒の観測では 1 つも実測できませんでした ([#168](https://github.com/atinfinity/fastdds_transport_viz/issues/168): statistics writer はツールの reader へ履歴をプロセスごとに順番に渡します)。
  [#141](https://github.com/atinfinity/fastdds_transport_viz/issues/141) より前は 4 分の 1 のペアが未実測で、表に出るのは 1 ペアだけでした。
- **40 プロセス、5600 ペア**でも大半から全部のペアが実測されます。同じビルドの 3 回の実行で 5 秒
  時点の coverage は 0.62 / 0.97 / 1.0 でした。#141 より前は 3 回とも 0.0 です。このばらつきは
  ツールではなくホスト側の事情で、この規模では負荷だけで 8 コア中 6.7〜7.5 コアを使ってしまいます。
  この規模では `--watch --stats` の 1 フレームに依然として 1.6 秒かかります (`--stats` なしで 0.44 秒)。
  20 プロセスでは 0.15 秒で、[#135](https://github.com/atinfinity/fastdds_transport_viz/issues/135) 以降は 250 ms の予算内です。

ツールは statistics をすべて UDP で受け取り、Fast DDS はそれを 1 本の受信スレッドで処理します。
Nav2 (4 participant、1195 ペア) の隣でも、このスレッドだけで 1 コアを使い切ります。損失を決めるのは
reader からどれだけ頻繁に読み出すかで、上述の 50 ms の読み出しはそのためのものです。それを
超えると、送信側の keep-last の履歴が届く前のサンプルを上書きします。

ツールは落としたサンプルを報告します。JSON 文書の `stats.samples_lost` が届かなかった statistics
サンプルの数、`stats.samples_rejected` がツールの reader が拒否したサンプルの数です。表の
`statistics:` フッタには `N sample(s) lost` が出て、N = `samples_lost` − `samples_lost_latency` +
`samples_rejected` です。つまり失われたか拒否されたカウンタのサンプルで、latency のサンプルは別項目
として出します (後述)。その損失で実測まで失われたとき (後述) は、
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
[プロセス内のペア](#プロセス内のペア)、`qos-incompatible` や IPC 分断のペア、
`stats-writer-instance-limit-suspected` のペアはどちらの数にも入らず、配送の証拠が無いペアは `no-traffic-observed` が説明するあいまいさのままです。
数はフレームごとに数え直すので、`--watch` では実測が戻れば警告も消えます。

未実測のペアがすべてツールの取りこぼしとは限りません。
[#152](https://github.com/atinfinity/fastdds_transport_viz/issues/152) 以降、文書は失われた
サンプルでは説明できない未実測ペアの数 (`stats.pairs_delivered_absent`) も持ちます。ツールが
`RTPS_SENT` のインスタンスを一度も見ていないペア (`packets_total` が 0) と、カウンタのサンプルを
1 つも失っていない実行の未実測ペアすべてです。これらには文書単位の別の警告 `rtps-sent-absent` が
付きます (ワンショット実行の stderr 1 行、`statistics:` フッター、web ビューア)。
`stats-samples-lost` は残りだけを数えるので、1 回の実行で両方が出ることもあります。主な原因は
statistics の writer 自身の既定で、これはどの Fast DDS バージョンでも同じです。これらの writer は
pull モード (`fastdds.push_mode` が false) で生成されるため、周期ハートビート (既定 3 秒) でしか
配送せず、カウンタは数秒の停滞をはさんでまとめて届き、損失は報告されません。最も顕著なのは
Fast DDS 3.6 (ROS 2 Lyrical) で、ワンショット実行では何も実測できませんでした
([#152](https://github.com/atinfinity/fastdds_transport_viz/issues/152))。ツールの reader 側では
変えられませんが、writer のプロファイルでは変えられます。観測対象のノードを、このパッケージが
インストールする `config/statistics.xml` (ROS 2 ノードはどのディストリでも
`FASTRTPS_DEFAULT_PROFILES_FILE`、rmw を通さない Fast DDS 3.x のアプリケーションは
`FASTDDS_DEFAULT_PROFILES_FILE`。[10 インスタンスの上限](#instance-limit)を参照) 付きで起動してください。これらの writer を push モードにし、
ハートビート周期を短くします。警告は観測した事実で判定し、Fast DDS のバージョンでは分岐しません。
また、そもそも実測しえないペア (data-sharing とプロセス内) はこの数に入りません
([#201](https://github.com/atinfinity/fastdds_transport_viz/issues/201))。

`stats.samples_lost_latency` は別に数えます。`stats.samples_lost` の**内数**であって並ぶ数では
なく、これを警告の対象にするものはありません。`HISTORY_LATENCY` は設計として best-effort で
受け取る ([Reader QoS](#reader-の-qos)) ため、その reader は最も賑やかなトピックのシーケンスの
抜けをすべて報告します。40 プロセスでは 60 秒の実行で 170 万〜190 万に達します。これらは独立した
観測値で、ツールはそれを平均と最大に畳むので、落ちても出ている数値が粗くなるだけで、加えて落とした
participant に reader を持つすべてのペアの `HZ` が下限 (`≥`、[前述](#hz-列-1-秒あたりに届いたサンプル数)) になります。表では
別項目として出し (`..., 42 sample(s) lost, 1000 latency sample(s) lost`)、web ビューアも同様で、
`stats-samples-lost` の警告はこれを数えません。

この警告が出ているときは、ペアの `no-traffic-observed` が「実測できなかった」の意味にもなり得ます。
カウンタは 10 個の statistics reader で共有しているため、損失を特定のペアに帰属させられません。
見る範囲を絞ってください:
- 見たいノードでだけ statistics を有効にする;
- `FASTDDS_STATISTICS` を必要な別名 (例: `RTPS_SENT_TOPIC;RTPS_LOST_TOPIC`) に絞る。

`--timeout` を延ばしても損失は減りません (より多く集めるだけです)。ただし各インスタンスが 2 回
サンプリングされる機会は増えます。上の coverage はまさにそれを比べたもので、5 秒時点 (40 プロセス) または
既定のワンショット (20 プロセス) の実測と 30 秒時点の実測の比です。

`stats.samples_lost_at_start` は別に数えられ、警告にはなりません。reader はマッチした時点で、
writer の keep-last 履歴が既に捨てていたサンプルをすべて「失われた」と通知されますが、これは
ツールが追いつけているかとは無関係です。しかもこの通知は遅れて届きます。writer は捨てた分を
マッチ時ではなく次以降の heartbeat で知らせるためで、静穏な 5 ノード系での実測では、マッチから
Jazzy で最大 1.2 秒、Lyrical で最大 3.9 秒あとに届きました。新しい statistics writer は必ずこの
バーストを伴うので、「直近 5 秒以内に新規マッチがある間」の損失を late-join と数えます。1 度も
マッチしていない間も同じ扱いです (猶予時間の起点となるマッチがまだ無いため)。途中から起動した
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
  2.14.6、Jazzy)、`.../fastdds_statistics_types_v26/` (Fast DDS 2.6.12、Humble、2.10 未満で使用。
  2.6 は fastcdr 1.0 の API でシリアライズするため)、`.../fastdds_statistics_types_v3/` (Fast DDS
  3.2.4 から生成、Lyrical / Rolling で使用) があり、CMake が Fast DDS のバージョンで選びます。別の Fast DDS を対象にするときは該当ディレクトリ
  を差し替えてください。
