# Web viewer

> 英語版が正です。この文書は 2026-09-22 時点の英語版に対応しています。

`web/index.html` は `transport_viz --json` の文書をグラフとして描画します。ホストが列、ROS ノードが
箱、writer → reader の各ペアが transport ごとに色分けされた矢印です。静的ページ (素の HTML/JS と
同梱の d3) なので、ビルドもサーバーも不要で、`file://` からオフラインで動きます。(`model.js` が文書の
モデルと書式、`scene.js` がレイアウトと矢印の形状、`replay.js` が録画の読み込みとそのタイムライン、
`app.js` が描画を受け持ちます。)

![graph view](images/web-viewer-graph.jpg)

## 開き方

```
transport_viz --json --stats > snapshot.json
open web/index.html            # macOS。あるいはファイルをダブルクリック
```

文書の読み込み方は 3 通りです。

- **Open JSON…** ボタン (ファイル選択)
- ページ上のどこかにファイルをドラッグ & ドロップ
- `index.html?src=<URL>` で文書を取得 (ページを HTTP で配信しているときだけ。例えば `web/` で
  `python3 -m http.server`。ブラウザは `file://` からの `fetch` を禁止しています)

同じ 3 通りで録画 (複数の文書を並べた JSON Lines ファイル、`.jsonl`。
[録画と再生](#録画と再生) 参照) も開けます。

ページは最初に `web/sample/sample.json` を表示します。talker/listener ノードと、statistics を
有効にした bounded 検証ノードの実際のキャプチャです。別の文書を開いた後は、ヘッダの
**Load sample** でこれに戻れます。

## グラフの読み方

| 要素 | 意味 |
|---|---|
| 列 | ホスト (`local`、`host:<id>`、または statistics から得たホスト名) |
| 箱 | ROS ノード (statistics があれば名前の下に `pid <n>`)。赤い `+N unmatched` は相手のいないトピック |
| 矢印 | 2 ノード間の同じ transport・同じ確信度の writer → reader ペアを束ねたもの。ラベルはペアが 1 つなら `<topic> · <TRANSPORT>` (確信度が `likely` なら transport の後に `?`)、N 個のペアを束ねたものなら `<N> topics · <TRANSPORT>` |
| 丸い箱 | Discovery Server (`SERVER` / `BACKUP` の participant。アナウンスされた名前、なければ `Discovery Server`、下に最初の locator)。そのホストの列に出ます ([#86](https://github.com/atinfinity/fastdds_transport_viz/issues/86)) |
| `CLIENT` タグ | participant が `CLIENT` / `SUPER_CLIENT` とアナウンスしたノード。文書がどのサーバーか判定できた場合はラベルのない灰色の点線がサーバーへ伸びます ([how-it-works.ja.md](how-it-works.ja.md#ノードと同じ場所で実行する) 参照)。transport の凡例の外で、束ねられません |
| 色 | UDPv4 青 · UDPv6 水色 · TCP 紫 · SHM 緑 · DATA_SHARING 橙 · NONE 灰 (凡例はツールバー) |
| 破線 | 確信度 `likely` |
| 赤い縁 | 警告が 1 つ以上ある (例: `measured-transport-mismatch`) |

矢印をクリックすると側面パネルにそのペアの一覧が出ます。transport、確信度、実測トラフィック、
理由コードとその説明・対処 (文書の `reason_code_descriptions` / `reason_code_remedies` から)、locator、QoS
(reliability、durability、data-sharing、設定されていれば deadline、liveliness、ownership、
partition)、`data-sharing` 行 (writer の履歴のサイズと、エンドポイントの data-sharing セグメントが
ツールの `/dev/shm` にあるか、[#163](https://github.com/atinfinity/fastdds_transport_viz/issues/163))、そして文書に `participants` があればエンドポイントごとの
`shm` 行 (その participant のツールの IPC 名前空間からの SHM 可視性と、アナウンスされた SHM
ポートのロック状態、[#125](https://github.com/atinfinity/fastdds_transport_viz/issues/125))、エンドポイントが ROS 2 の type hash をアナウンスしていれば
その先頭 8 文字を出す `type hash` 行 (マウスを乗せると全体が出ます、[#85](https://github.com/atinfinity/fastdds_transport_viz/issues/85)) です。ノードをクリックすると publisher、subscription、相手のいない
トピックが出ます。クライアントならさらにアナウンスした discovery プロトコル、participant の
prefix と metatraffic locator、そのサーバーが、サーバーならそれに紐づくクライアントの一覧が
出ます。

ヘッダの 1 行目にはドメイン、観測時刻、件数が出て、最後に statistics の要約が付きます。文書が
持っているサンプル数と、ツールが取り逃したぶんがあれば `N lost` です。その損失で実測まで失われた
ときは `!stats-samples-lost` の印が続きます (印にマウスを乗せると説明と対処が出ます)。この印が
あるときは、実際には通信していてもペアが `(unmeasured, delivered)` と表示されることがあります。印は文書の
`stats.warnings` に従うので、印の無い `N lost` は害の無かった損失です ([statistics.ja.md](statistics.ja.md#大規模なシステム) 参照)。

ヘッダの 2 行目は `transport_viz` が動いた環境の共有メモリの要約です (文書の `shm`
オブジェクト。[how-it-works.ja.md](how-it-works.ja.md#環境の共有メモリ) 参照): `/dev/shm` の容量、
Fast DDS が置いているもの、stale なファイル、`shm-*` の警告。末尾には、通常の discovery でなかった
場合のツール自身の参加の仕方 (`observed as SUPER_CLIENT of UDPv4 …` や Easy Mode のアドレス) が
付きます。

**Table** タブは CLI の `--verbose` と同じ形です: トピックごとの見出し行の下にペア行が並びます。
見出し行にはドキュメントのトピック集計 (`topics[]`) が出ます: writer / reader の数、ペアが使う
トランスポート、最も遅いペアの遅延、欠落 (RTPS_LOST はトピックごとに 1 回だけ数え、再送は合計)、
unmatched の理由です。ペア行を足し合わせることはないので、フィルタでペアを隠しても見出しの数字は
変わりません。`Hz` はペアごとにしかないので見出しでは空欄です。見出しをクリックするとそのトピックの
ペアを畳んだり開いたりできます。ツールバーのボタン (Table タブでだけ表示) で一括にでき、表示は
開いているトピックが 1 つでもあれば **Collapse all**、すべて畳まれていれば **Expand all** です。
列見出しをクリックするとトピックは集計値で、トピック内のペアは各自の値でソートされます
(数値は数値として、値のないものは最後)。statistics があればペア行に観測中に運ばれたパケット数と
バイト数、遅延、1 秒あたりに届いたサンプル数 (`Hz`、ホバーで窓の長さ)、欠落も出ます。

`--all` で取得したサービスとアクションは、グループ全体で 1 つの見出し行になります。
`SERVICE` / `ACTION` のバッジが付き、`rq/` / `rr/` トピックではなくサービス名・アクション名で
表示され、その下にメンバーのペア行が並びます
([#84](https://github.com/atinfinity/fastdds_transport_viz/issues/84))。writer / reader の
セルは向きごとのメンバーペア数 (完全なサービスは `1`/`1`、完全なアクションは `3`/`5`)、型は
メンバーの型から末尾を取り除いたものです。サービスなら `_Request` / `_Response` を取り除いた
サービスの型、アクションなら `_SendGoal_Request`、`_GetResult_Response`、`_FeedbackMessage` などを
取り除いたアクションの型 (`example_interfaces/action/Fibonacci`) です。試すための capture として
`sample/services.json` があります (`index.html?src=sample/services.json`)。グラフは変えて
いません。エッジは矢印であり矢印には向きがあるので、メンバーごとのままです。

![table view](images/web-viewer-table.jpg)

フィルタ (トピックの正規表現、ノードの正規表現、transport のチェックボックス、`/parameter_events`、
`/rosout` と、すべての endpoint が親トピックに畳み込まれた (`buffer_parent_guid`) native buffer の
コンパニオントピックを隠す「hide ROS internal topics」) はグラフ、表、矢印のパネルに適用されます (ノードの
パネルはそのノードの全トピックを常に表示)。ノードの
フィルタは `--node` と同じ意味論です。writer か reader が一致するノードに属するペアを残し、グラフ
には一致したノード (強調表示。表示中のペアが無くても残る) と残ったペアの相手ノードを描き、それ以外
は隠します。不正な正規表現は赤枠で表示され、何も絞り込みません。
正規表現の欄への入力は、打鍵が 100 ms 止まった時点、または Enter か欄からフォーカスが外れた時点で
適用されます。

## 2 つの文書の比較

viewer は `transport_viz diff` の比較 ([how-it-works.ja.md](how-it-works.ja.md#2-つのスナップショットの比較)
参照) をブラウザ内で行います。方法は 3 つ:

- **Compare with…** で 2 つ目の文書を読み込むと、表示中の文書 (before) とそれ (after) を比較し、
  after の文書を表示します。**Open JSON…** は従来どおり文書を置き換え、比較も終わります。
- `index.html?src=before.json&diff=after.json` は両方を取得します (`?src=` と同じく HTTP で配信
  している場合)。`&key=guid` で GUID キーになります。
- `changes` オブジェクトを既に持つ文書、つまり `transport_viz diff --json` の出力や `--watch --json`
  のフレームは、そのまま強調表示されます。

ヘッダには before の文書 (`vs 2026-09-13T09:00:00Z by node key`)、ツールバーには CLI と同じ
`changes:` の要約、印の付いたペアとそのノードだけを残す **changes only**、そしてここで比較した場合は
**key** (既定の `node` か `guid`。それぞれが何を乗り越えるかは
[how-it-works.ja.md](how-it-works.ja.md#2-つのスナップショットの比較)) が出ます。比較そのものは
`web/model.js` の `diffDocuments()` で、C++ の関数の移植です。同じ fixture (`web/sample/diff_before.json`、
`diff_after.json`、`diff.json`) に対してバイナリの出力と一致することをテストしているので、両者は
食い違いません。

| 要素 | 意味 |
|---|---|
| `+` 緑 | ペアが現れた (表の行、矢印のハロとラベル) |
| `~` 橙 | transport、確信度、実測 transport、選ばれた locator、実測 locator、警告のいずれかが変わった。表には `before → after` の transport、ペアのカードには変わった項目が出る |
| `-` 灰、点線、斜体 | ペアが消えた。表の末尾に以前の transport 付きの薄い行 (before の文書がある場合)、両端のノードがまだあれば点線の矢印 |

ライブモードでは、どの印も変化から 3 フレーム残ってから消えます (CLI の `--watch` と同じ)。

## ライブモード

`transport_viz_web` (`web/serve.py` からインストール。Python 標準ライブラリのみ) は
`transport_viz --watch --json` をサブプロセスとして実行し、viewer と、新しい文書ごとの
Server-Sent Events ストリームを配信します。

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1
# transport_viz_web: listening on http://127.0.0.1:8765/  (serving .../share/fastdds_transport_viz/web)
```

表示された URL を開きます。`/` は `index.html?live=1` にリダイレクトされ、`/events` に接続して
文書ごとに再描画します。選択状態、フィルタ、ズームは保たれます (レイアウトは決定的なので位置が
跳びません)。ヘッダにはライブ状態と最終更新時刻が出ます。受信したフレームはページに残ります
([ライブ履歴](#ライブ履歴))。`/latest.json` は常に最新の文書を返します (`?src=/latest.json` で使えます。両方あるときは
`?live=1` が優先)。

接続が切れたとき (サーバーの停止、ネットワークの切断、ラップトップのスリープなど) は、
直前の文書を表示したままヘッダが「live: connection lost, reconnecting…」になり、ブラウザは
1 秒ごとに再接続を試みます (ストリームが `retry: 1000` で指定します。ブラウザ既定は 3 秒)。
サーバーは新しい接続ごとに最新の文書を送るので、再びサーバーが待ち受けていれば次の
`--interval` を待たずにこの表示は消えます。

### ライブ履歴

viewer は受信したフレームをすべて保持します
([#218](https://github.com/atinfinity/fastdds_transport_viz/issues/218))。1 分前に UDPv4 に
切り替わったペアもあとから見られます。2 フレーム目からは[再生のタイムライン](#録画と再生)が
ツールバーの下に出て最新フレームに追従し、選択したペアのカードのチャートは保持中のフレーム
全体を描き、フレームごとに伸びます。

- タイムライン上の移動 (`◀` / `▶`、スライダー、`◀ change` / `change ▶`、チャートのクリック) は
  そのフレームで止まります。フレームは届き続けてタイムラインに加わりますが、画面はそのままで、
  ヘッダは `live: viewing #k of N (newest …)` になります。**Pause** も表示中のフレームで同じように
  止まります。
- `live ▶|`、End キー、**Resume** で最新フレームに戻り、再び追従します。
- 最新フレームの変化は CLI と同じく 3 フレームの間マークされます。過去のフレームは再生と同じく
  そのフレーム自身の `changes` だけを表示します。
- **Save recording** は保持中のフレームを `transport_viz-<最初の observed_at>.jsonl` として
  ダウンロードします。`--record` と同じ JSON Lines なので、あとで開いたり人に渡したりできます。
- **match by** を変えると保持中のフレームを別のキーで読み直します。その間に届いたフレームは
  後ろに追加されます。

履歴はページの中にあります。各フレームのテキストを Blob として持ち、表示するときに解析し直します。
ページを開いた時点から始まり、閉じるか再読み込みすると消えます。それより前の時間や、ブラウザを
開いていない間のためには、サーバーを [`--record`](#録画と再生) 付きで動かしてください。
`?history=<MB>` で上限を決めます (既定 512。`0` で履歴を持たず、その場合 **Pause** は
**Resume** まで最新フレームの反映を止めます)。`/` は `index.html?live=1` にリダイレクトされ
クエリは捨てられるので、パラメータはそのアドレスに付けてください:
`http://127.0.0.1:8765/index.html?live=1&history=1024`。上限を超えると最も古い 1 割のフレームをまとめて
捨て、タイムラインに `history: 512 MB, oldest dropped` と出ます。表示中の過去フレームが捨てられた
ときは、一時停止のまま最も古い保持フレームに移り、`the frame on screen was dropped` と出ます。
2400 ペアの `medium` 文書を `--interval 1` で流すと (ストリーム上で 1 フレーム約 5.7 MB)、既定の
上限で 90 フレーム (1 分半) を保持します。1 フレームにかかる時間は追従中 17 ms、一時停止中 12 ms、
最も古い 9 フレームの破棄はそれを引き起こしたフレームと合わせて 14 ms でした。JavaScript ヒープは
38-49 MB のままで (フレームのテキストはヒープではなくブラウザの Blob ストアにあります)、**match by**
による 90 フレームの読み直しは 0.8 秒でした ([verification-log.md](verification-log.md#scale-results))。
もっと遡りたいときは上限を上げ、小さなマシンでは下げてください。

`document` イベントにはそれぞれ `id:` (サーバーのストリーム内での番号) が付きます。サーバーは
クライアントに最新の文書だけを送るので、遅れたブラウザ (忙しいタブ、遅い回線) は一部を受け取り
ません。タイムラインはその数を示します (`3 frames skipped by the stream`)。再接続で送り直される
文書はすでに持っている id なので二重には追加しません。id が小さくなったらサーバーが再起動した
ということで、履歴はそのまま続き、タイムラインに `stream restarted` が加わります。
`transport_viz` が終了しても保持中のフレームは残り、スクラブできます。

![live mode](images/web-viewer-live.jpg)

サーバー自身のオプション以外の引数はすべて `transport_viz` に転送されます (`--` そのものも
転送され、バイナリに拒否されます)。

| オプション | 意味 |
|---|---|
| `--bind ADDR` | 待ち受けアドレス。既定 `127.0.0.1`。別のマシンから見るなら `0.0.0.0` (例: ノート PC からロボットを見る) |
| `--port N` | 既定 `8765`。`0` で空きポートを選ぶ |
| `--transport-viz PATH` | 実行するバイナリ (既定: スクリプトの隣、次に `$PATH`) |
| `--verbose` | リクエストと受信した文書をログに出す |
| `--record FILE` | `transport_viz` が出力する行 (1 行に 1 文書) をすべて `FILE` にも書き、[再生](#録画と再生)に使えるようにする (`FILE` は上書き。書けないときは `transport_viz` を起動する前に終了する) |
| それ以外 | 転送: `--stats`、`--interval S`、`--domain N`、`--all`、`--topic REGEX`、`--timeout S` |

`transport_viz` が終了するとサーバーは `status` イベントを送り (「live: transport_viz exited …」と
表示) 停止します。終了コードは `transport_viz` が失敗していれば 1、そうでなければ 0 です。
`transport_viz_web` を止めると (Ctrl-C、またはこのプロセス自身への SIGTERM か SIGHUP)、起動した
`transport_viz` も一緒に止まり、サーバーは 0 で終了します。Docker 環境では `docker compose run --rm --service-ports dev` が
ポート 8765 を公開するので、コンテナ内の `transport_viz_web --bind 0.0.0.0` にホストのブラウザから
届きます。

`transport_viz --watch --json` 自体は 1 行に 1 つのコンパクトな文書 (JSON Lines) を出力するので、
他のプログラムからも同じストリームを読めます。その文書の `changes` オブジェクトは
`transport_viz diff --json before.json after.json` が出すものと同じです
([how-it-works.ja.md](how-it-works.ja.md#2-つのスナップショットの比較) 参照)。viewer は
どちらの場合もそれを強調表示します ([2 つの文書の比較](#2-つの文書の比較))。

### Prometheus メトリクス

`transport_viz_web` は `/metrics` も提供します。最新の文書を Prometheus のテキスト形式にしたもので、
スクレイプされた時点で組み立てます (起動時の行に URL が出ます:
`transport_viz_web: metrics: http://127.0.0.1:8765/metrics`)。Prometheus のスクレイプジョブを
向ければ、ロボットの他の指標と並べて Grafana で transport をグラフにできます:

```yaml
scrape_configs:
  - job_name: transport_viz
    static_configs:
      - targets: ['robot1:8765']   # transport_viz_web --bind 0.0.0.0
```

名前はすべて `transport_viz_` で始まります。ペアの系列はラベル `topic`、`writer_node`、
`reader_node`、`writer_host`、`reader_host`、`writer_guid`、`reader_guid` を持ちます
(同じノード間の 2 つのペアは GUID で区別されます):

| 系列 | 値 |
|---|---|
| `pair_transport{transport}` | 1。ペアの予測 transport |
| `pair_warning{code}` | ペアの警告コードごとに 1 |
| `pair_measured_transport{transport}` | パケットを運んだ transport ごとに 1 (`--stats`) |
| `pair_packets`、`pair_bytes` | 観測中に reader へ送られた RTPS パケット数 / バイト数 (`--stats`) |
| `pair_delivered_per_second` | `measured.delivered_per_s` (`--stats`) |
| `pair_latency_seconds{stat}` | `stat` = `mean`、`min`、`max`、`last` (`--stats`) |
| `pair_lost_packets`、`pair_resent_datas` | `measured.reliability` (`--stats`) |
| `shm_total_bytes`、`shm_used_bytes`、`shm_free_bytes`、`shm_fastdds_bytes`、`shm_segments`、`shm_ports`、`shm_stale_segments`、`shm_stale_ports` | `shm` オブジェクト。ラベル `host` はツール自身のホスト |
| `info{domain,schema_version}` | 1 |
| `up` | `transport_viz` の実行中は 1、終了後は 0 |
| `documents_total` | 受け取った文書の数 (counter) |
| `last_document_timestamp_seconds` | 最新の文書の `observed_at` |
| `stats_enabled` | `--stats` のとき 1 |

ペアの値はすべて最新の文書の gauge です。文書で null の値には系列がなく (実測のないペアは
`pair_transport` と `pair_warning` だけ)、文書から消えたペアは次のスクレイプで `/metrics`
からも消えます。スループットの系列はありません: `throughput_bytes_per_s` は常に null です
([statistics.ja.md](statistics.ja.md) 参照)。最初の文書が届く前は `up` と `documents_total`
だけを返します。ラベルの値は文書の値そのままなので、1 つの Prometheus に複数のロボットを
集めるときは Prometheus が付ける `instance` ラベルで区別します。大きなシステムでは応答も
大きくなります。medium スケール (`--stats` 付きで 2400 ペア) で約 24 000 系列、7.5 MB、
組み立てに約 50 ms です。スクレイプ間隔は `--interval` 以上にし、一部のトピックだけで足りるなら
`transport_viz_web --topic REGEX` で絞ってください。

## 録画と再生

一時的な問題 (ノードの再起動中に 10 秒だけ UDPv4 に落ちるペアなど) は、誰かが viewer を開く前に
消えてしまいます。ライブのストリームを録画しておき、あとで再生できます
([#82](https://github.com/atinfinity/fastdds_transport_viz/issues/82))。すでにライブモードで
開いていた viewer は自分でフレームを持っていて、保存もできます ([ライブ履歴](#ライブ履歴))。

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1 --record rec.jsonl
# サーバーなしなら:
ros2 run fastdds_transport_viz transport_viz --watch --json --stats --interval 1 > rec.jsonl
```

どちらも同じファイルを書きます。1 行に 1 つの `transport_viz --json` 文書 (*フレーム*) で、
各行は届いた時点で書き出して flush するので、Ctrl-C やクラッシュで途切れた録画も最後の完全な
行まで再生できます。形式は既存スキーマの JSON Lines で、文書に新しいものは何も加えません。

ファイルは普通の文書と同じく **Open JSON…**、ドラッグ & ドロップ、`index.html?src=rec.jsonl`
で開きます。`&frame=N` (1 始まり) を付けるとフレーム N から開きます。viewer は名前ではなく中身で
判断します。文書が 2 つ以上あるファイルは録画、1 つだけのファイルはその文書として表示します。
文書でない行 (リダイレクトに紛れた `[ros2run]` のメッセージ、末尾で切れた行) は読み飛ばし、
タイムラインに件数を出します (`2 lines skipped (not a document)`)。

ツールバーの下にタイムラインが出ます。

| 操作 | 動作 |
|---|---|
| スライダー | フレームを選ぶ。すべての `observed_at` が解釈でき、逆戻りしていなければその時刻の位置に、そうでなければ等間隔に並ぶ |
| スライダー上の目盛り | `changes` に追加・削除・変化したペアがあるフレーム |
| `◀` / `▶`、または ← / → キー | 前 / 次のフレーム |
| `◀ change` / `change ▶` | 目盛りのある前 / 次のフレーム |
| `i / N` と時刻 | 表示中のフレームとその `observed_at`。ページタイトルの末尾は `#i` になり、`?src=` で開いたページはアドレスに `&frame=i` を保つ |
| match by | フレーム間でペアを追う方法。`transport_viz diff` の `--key` と同じで、`node` (既定) はノードが新しい GUID で再起動しても同じペアとして追い、`guid` は別のペアとして始める |

各フレームはライブのフレームと同じように表示します。そのフレーム自身の `changes` が追加・削除・
変化させたペアに印が付き (フレームは直前のフレームとの差分を持っています)、フィルタ、選択、
ズームはそのまま、選択は次のフレームでも同じペアを (矢印なら writer と reader のノードを) 追います。
選択中のペアがそのフレームにないときは "not in this frame" と表示します。

選択したペアのカードには録画全体のチャートが付き、表示中のフレームに縦線が引かれます。チャートを
クリックすると、その位置にいちばん近いフレームに移ります。

- 帯: transport ごとに色分けし、ペアがないところは空白;
- `delivered/s`: 各フレームのペアの `delivered_per_s`;
- latency: 各フレーム自身の区間の平均 (文書は観測全体の平均を持つので、2 つのフレームの差から出す);
- lost packets: 区間ごとの損失。累積の `lost_packets` の差。

後ろ 3 つには `--stats` が必要で、`--stats` なしの録画は帯だけになります。矢印のカードには、
その矢印に束ねたすべてのペアの帯が付きます。

再生中の **Compare with…** は再生を終え、表示中のフレーム (`rec #k`) と選んだファイルを
比較します。単一の文書を期待するところ (**Compare with…** の after 文書、`?diff=`、比較のために
取得する `?src=`) では、録画は最後の文書として扱います。

ファイルは 8 MB ずつ読み、全体を保持しません。viewer が持つのは各フレームのバイト範囲と、
ペアとフレームごとのいくつかの数値だけです。最初のフレーム (または `&frame=` のフレーム) は
読み終えた時点で表示し、タイムラインの横に "loading x / y MB" を出します。フレームは表示する
ときにもう一度パースします。2400 ペアの `medium` スケール文書 60 フレーム (1 フレーム 5.5 MB、
332 MB) の録画では、最初のフレームが 0.6-0.8 秒で出て、1.4-1.6 秒で読み終わり、フレームの移動は
33 ms、JavaScript ヒープは約 45 MB でした ([verification-log.md](verification-log.md#scale-results))。
この規模で `--interval 1` なら 1 時間で約 20 GB になります。丸 1 日ではなく、問題の前後数分を
録画するか `--interval` を大きくしてください。

## 大きな文書

スケール検証の文書を Apple M3 上のヘッドレス Chrome で開き、
[#136](https://github.com/atinfinity/fastdds_transport_viz/issues/136) の後に測りました (詳細は
[verification-log.md](verification-log.md#scale-results))。

| 文書 | 初回描画 | トピックのフィルタ / ノードのフィルタ / その解除 | 選択 / 選択解除 |
|---|---|---|---|
| `medium`: 矢印 378 本、2400 ペア、9.1 MB | 33 ms | 133 / 133 / 133 ms | 33 / 34 ms |
| `large`: 矢印 1467 本、5600 ペア、16.8 MB | 53 ms | 120 / 123 / 146 ms | 35 / 33 ms |

フィルタの時間には、最後の打鍵から正規表現のフィルタが適用されるまでの 100 ms の待ちが含まれる
ので、処理そのものはその時間から 100 ms を引いたものです。フィルタを変えると表示中の矢印を描き
直します。絞り込みは軽く、`large` でフィルタを解除すると待ちの後に 46 ms かかります。クリックは
選択した矢印、ノード、行の見た目を変えて側面パネルを埋めるだけで何も描き直さないので、文書が
大きくなってもほとんど重くなりません。選択の時間のうち 33 ms は、60 Hz での 2 フレーム分という
測定の下限です。

## JSON スキーマ

`schema/transport_viz.schema.json` (JSON Schema 2020-12) が viewer の依存する契約です。必須キーと
列挙値を列挙し、未知のキーは許容するので、ツールは `schema_version` を上げずにフィールドを追加
できます。互換性の無い変更では番号を上げます。サンプル文書とライブの `--json` 出力は `colcon test`
でスキーマ検証されます (`test_json_schema` と `test_json_schema_live.py`、`python3-jsonschema` 使用)。
