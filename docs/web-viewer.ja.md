# Web viewer

> 英語版が正です。この文書は 2026-09-12 時点の英語版に対応しています。

`web/index.html` は `transport_viz --json` の文書をグラフとして描画します。ホストが列、ROS ノードが
箱、writer → reader の各ペアが transport ごとに色分けされた矢印です。静的ページ (素の HTML/JS と
同梱の d3) なので、ビルドもサーバーも不要で、`file://` からオフラインで動きます。

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

ページは最初に `web/sample/sample.json` を表示します。talker/listener ノードと、statistics を
有効にした bounded 検証ノードの実際のキャプチャです。

## グラフの読み方

| 要素 | 意味 |
|---|---|
| 列 | ホスト (`local`、`host:<id>`、または statistics から得たホスト名) |
| 箱 | ROS ノード (statistics があれば名前の下に `process id`)。赤い `+N unmatched` は相手のいないトピック |
| 矢印 | 2 ノード間の同じ transport・同じ確信度の writer → reader ペアを束ねたもの。ラベルはペア数 |
| 色 | UDPv4 青 · UDPv6 水色 · TCP 紫 · SHM 緑 · DATA_SHARING 橙 · NONE 灰 (凡例はツールバー) |
| 破線 | 確信度 `likely` |
| 赤い縁 | 警告が 1 つ以上ある (例: `measured-transport-mismatch`) |

矢印をクリックすると側面パネルにそのペアの一覧が出ます。transport、確信度、実測トラフィック、
理由コードとその説明・対処 (文書の `reason_code_descriptions` / `reason_code_remedies` から)、両エンドポイントの locator、QoS
(reliability、durability、data-sharing、設定されていれば deadline、liveliness、ownership、
partition)、data-sharing 履歴のサイズです。ノードをクリックすると publisher、subscription、相手のいない
トピックが出ます。

ヘッダの 2 行目は `transport_viz` が動いた環境の共有メモリの要約です (文書の `shm`
オブジェクト。[how-it-works.md](how-it-works.md#環境の共有メモリ) 参照): `/dev/shm` の容量、
Fast DDS が置いているもの、stale なファイル、`shm-*` の警告。

**Table** タブはペアごとに 1 行を表示します (見出しをクリックでソート)。statistics があれば
writer の payload レートと観測中に運ばれたバイト数も出ます。

![table view](images/web-viewer-table.jpg)

フィルタ (トピックの正規表現、ノードの正規表現、transport のチェックボックス、`/parameter_events`
と `/rosout` を隠す「hide ROS internal topics」) はグラフ、表、矢印のパネルに適用されます (ノードの
パネルはそのノードの全トピックを常に表示)。ノードの
フィルタは `--node` と同じ意味論です。writer か reader が一致するノードに属するペアを残し、グラフ
には一致したノード (強調表示。表示中のペアが無くても残る) と残ったペアの相手ノードを描き、それ以外
は隠します。不正な正規表現は赤枠で表示され、何も絞り込みません。

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
跳びません)。ヘッダにはライブ状態と最終更新時刻が出ます。**Pause** でフレームの適用を止め、
**Resume** で再開します。`/latest.json` は常に最新の文書を返します (`?src=/latest.json` で使えます。両方あるときは
`?live=1` が優先)。

![live mode](images/web-viewer-live.jpg)

サーバー自身のオプション以外の引数はすべて `transport_viz` に転送されます (`--` そのものも
転送され、バイナリに拒否されます)。

| オプション | 意味 |
|---|---|
| `--bind ADDR` | 待ち受けアドレス。既定 `127.0.0.1`。別のマシンから見るなら `0.0.0.0` (例: ノート PC からロボットを見る) |
| `--port N` | 既定 `8765`。`0` で空きポートを選ぶ |
| `--transport-viz PATH` | 実行するバイナリ (既定: スクリプトの隣、次に `$PATH`) |
| `--verbose` | リクエストと受信した文書をログに出す |
| それ以外 | 転送: `--stats`、`--interval S`、`--domain N`、`--all`、`--topic REGEX`、`--timeout S` |

`transport_viz` が終了するとサーバーは `status` イベントを送り (「live: transport_viz exited …」と
表示) 停止します。終了コードは `transport_viz` が失敗していれば 1、そうでなければ 0 です。Docker 環境では `docker compose run --rm --service-ports dev` が
ポート 8765 を公開するので、コンテナ内の `transport_viz_web --bind 0.0.0.0` にホストのブラウザから
届きます。

`transport_viz --watch --json` 自体は 1 行に 1 つのコンパクトな文書 (JSON Lines) を出力するので、
他のプログラムからも同じストリームを読めます。その文書の `changes` オブジェクトは
`transport_viz diff --json before.json after.json` が出すものと同じです
([how-it-works.ja.md](how-it-works.ja.md#2-つのスナップショットの比較) 参照)。viewer はまだそれを
どちらの場合も強調表示します ([2 つの文書の比較](#2-つの文書の比較))。

## JSON スキーマ

`schema/transport_viz.schema.json` (JSON Schema 2020-12) が viewer の依存する契約です。必須キーと
列挙値を列挙し、未知のキーは許容するので、ツールは `schema_version` を上げずにフィールドを追加
できます。互換性の無い変更では番号を上げます。サンプル文書とライブの `--json` 出力は `colcon test`
でスキーマ検証されます (`test_json_schema` と `test_json_schema_live.py`、`python3-jsonschema` 使用)。
