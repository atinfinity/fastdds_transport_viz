# Web viewer

> 英語版が正です。この文書は 2026-09-05 時点の英語版に対応しています。

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
| 矢印 | 2 ノード間の同じ transport の writer → reader ペアを束ねたもの。トピック数付き |
| 色 | UDPv4 青 · UDPv6 水色 · TCP 紫 · SHM 緑 · DATA_SHARING 橙 · NONE 灰 (凡例はツールバー) |
| 破線 | 確信度 `likely` |
| 赤い縁 | 警告が 1 つ以上ある (例: `measured-transport-mismatch`) |

矢印をクリックすると側面パネルにそのペアの一覧が出ます。transport、確信度、実測トラフィック、
理由コードとその説明 (文書の `reason_code_descriptions` から)、両エンドポイントの locator と QoS
です。ノードをクリックすると publisher、subscription、相手のいないトピックが出ます。

**Table** タブはペアごとに 1 行を表示します (見出しをクリックでソート)。statistics があれば
writer の payload レートと観測中に運ばれたバイト数も出ます。

![table view](images/web-viewer-table.jpg)

フィルタ (トピックの正規表現、ノードの正規表現、transport のチェックボックス、`/parameter_events`
と `/rosout` を隠す「hide ROS internal topics」) はグラフ、表、パネルに適用されます。ノードの
フィルタは `--node` と同じ意味論です。writer か reader が一致するノードに属するペアを残し、グラフ
には一致したノード (強調表示。表示中のペアが無くても残る) と残ったペアの相手ノードを描き、それ以外
は隠します。不正な正規表現は赤枠で表示され、何も絞り込みません。

## ライブモード

`transport_viz_web` (`web/serve.py` からインストール。Python 標準ライブラリのみ) は
`transport_viz --watch --json` をサブプロセスとして実行し、viewer と、新しい文書ごとの
Server-Sent Events ストリームを配信します。

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1
# transport_viz_web: listening on http://127.0.0.1:8765/
```

表示された URL を開きます。`/` は `index.html?live=1` にリダイレクトされ、`/events` に接続して
文書ごとに再描画します。選択状態、フィルタ、ズームは保たれます (レイアウトは決定的なので位置が
跳びません)。ヘッダにはライブ状態と最終更新時刻が出ます。**Pause** でフレームの適用を止め、
**Resume** で再開します。`/latest.json` は常に最新の文書を返します (`?src=/latest.json` で使えます)。

![live mode](images/web-viewer-live.jpg)

二重ダッシュより前のオプションはサーバーのもので、それ以外はすべて `transport_viz` に転送されます。

| オプション | 意味 |
|---|---|
| `--bind ADDR` | 待ち受けアドレス。既定 `127.0.0.1`。別のマシンから見るなら `0.0.0.0` (例: ノート PC からロボットを見る) |
| `--port N` | 既定 `8765`。`0` で空きポートを選ぶ |
| `--transport-viz PATH` | 実行するバイナリ (既定: スクリプトの隣、次に `$PATH`) |
| `--verbose` | リクエストと受信した文書をログに出す |
| それ以外 | 転送: `--stats`、`--interval S`、`--domain N`、`--all`、`--topic REGEX`、`--timeout S` |

`transport_viz` が終了するとサーバーは `status` イベントを送り (「live: transport_viz exited …」と
表示)、0 以外のコードで停止します。Docker 環境では `docker compose run --rm --service-ports dev` が
ポート 8765 を公開するので、コンテナ内の `transport_viz_web --bind 0.0.0.0` にホストのブラウザから
届きます。

`transport_viz --watch --json` 自体は 1 行に 1 つのコンパクトな文書 (JSON Lines) を出力するので、
他のプログラムからも同じストリームを読めます。

## JSON スキーマ

`schema/transport_viz.schema.json` (JSON Schema 2020-12) が viewer の依存する契約です。必須キーと
列挙値を列挙し、未知のキーは許容するので、ツールは `schema_version` を上げずにフィールドを追加
できます。互換性の無い変更では番号を上げます。サンプル文書とライブの `--json` 出力は `colcon test`
でスキーマ検証されます (`test_json_schema` と `test_json_schema_live.py`、`python3-jsonschema` 使用)。
