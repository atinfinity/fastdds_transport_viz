# 仕組み

> 英語版が正です。この文書は 2026-09-05 時点の英語版に対応しています。

このツールは Fast DDS 2.14 (ROS 2 Jazzy) と 3.x (Kilted、Rolling) の両方に対してビルドできます。
API の差分は `include/fastdds_transport_viz/fastdds_compat.hpp` に閉じ込めてあり、以下の判定ルールは
両方で同じです。

`ros2 topic info -v` では transport は分かりません。rmw 層は locator (通信先アドレス) の情報を
公開しないためです。そこで `transport_viz` は自前の Fast DDS `DomainParticipant` を作り、
エンドポイントの discovery を観測します。discovery にはリモートの各 writer / reader が
**広告している locator** (`UDPv4`、`SHM` など) と QoS が含まれます。その情報に、Fast DDS 2.14 が
writer → reader の各ペアで transport を選ぶときと同じルールを適用します。

## 判定ルール

1. **同じホストか?** Fast DDS は、2 つの participant の GUID プレフィックス先頭 4 バイトが等しい
   とき同じホスト上にあるとみなします。
2. 同じホストで、両エンドポイントが data-sharing (zero-copy) を広告し、domain id に共通部分がある
   → `DATA_SHARING` (確信度 `likely`。[data-sharing.ja.md](data-sharing.ja.md) を参照)。
3. 同じホストで、両方が SHM locator を広告している → `SHM`。このとき Fast DDS はその participant
   間のユーザーデータに共有メモリだけを使います。discovery は引き続き UDP で行われます。
4. それ以外は、reader が広告するネットワーク locator のうち writer も話せる最初の種類
   → `UDPv4` / `UDPv6` / `TCPv4` / `TCPv6`。
5. 共通の locator が無い → `NONE`。

publisher だけ、または subscription だけのトピックは `-` と理由 `no-matching-reader` /
`no-matching-writer` で表示されます。

`--topic REGEX` は名前が一致するトピックを残します。`--node REGEX` は writer か reader が
完全修飾ノード名 (`/ns/name`) の一致するノードに属するペアを、そのノードの未接続エンドポイントと
ともに残します。残ったペアの相手側は一致しなくても表示されます。2 つのフィルタは AND です。
不正な正規表現は起動時に拒否されます (終了コード 2)。

## 理由コード

すべての判定には機械可読な理由コード (`same-host-guid`、`reader-no-shm-locator` など) が付き、
必要に応じて `!` で始まる警告も付きます。`--explain` は現在の出力で使われているコードの凡例を
末尾に付け、`transport_viz --list-codes` は全コードを表示します。transport の後ろの `?` は
確信度が `certain` ではなく `likely` であることを意味します。

判定ロジックは `src/fastdds_transport_viz/src/decision.cpp` に DDS 依存の無い純粋関数として
実装され、`test/test_decision.cpp` でテストされています。

## ノード名とツール自身の痕跡

ROS のノード名は rclcpp のグラフ API (エンドポイント GID → ノード) で解決するため、ツールは
隠しノード `_transport_viz_<pid>` を登録します。ツール自身のエンドポイントは出力から除外されます。
discovery の観測は別の生の Fast DDS participant で行い、rmw 自身の discovery リスナには触れません。

## ノードと同じ場所で実行する

ツールは Fast DDS が読む環境をそのまま読み、変更はしません。観測したいノードと同じシェル環境で
実行してください。`FASTDDS_BUILTIN_TRANSPORTS`、`FASTRTPS_DEFAULT_PROFILES_FILE` (観測用
participant もノードと同様に既定の participant プロファイルをここから取ります)、
`ROS_DISCOVERY_SERVER`、`ROS_AUTOMATIC_DISCOVERY_RANGE`、`ROS_STATIC_PEERS` を揃え、ネットワークと
IPC の名前空間も同じにします (コンテナなら `network_mode` / `ipc`)。ツールからノードが見えない
環境では `ros2 topic list` でも見えません。マルチキャストの通らないネットワーク上のホストについては
[development.md](development.md#two-physical-hosts) (英語) を参照してください。

transport ごとの注意点 (いずれも launch テストかマルチコンテナのシナリオで確認済み。
[development.md](development.md#verification-results) を参照):

- `FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA` は SHM と並んで TCPv4 を広告します。同一ホストでは
  SHM が選ばれ (`both-shm-locators`)、ホスト間では `TCPv4` (`common-tcpv4-locator`) になり、
  `--stats` は TCP のトラフィックを測定します。`--stats` を使うときはツールも `LARGE_DATA` で
  起動してください。statistics のサンプルが TCP で流れるためです。
- `UDPv6` / `DEFAULTv6` には IPv6 アドレスを持つインターフェースが必要です (Docker の既定ブリッジには
  ありません)。discovery を聞くにはツールも UDPv6 を話す必要があります。
- `ROS_DISCOVERY_SERVER`: 通常のクライアントは自分に関係するエンドポイントしか教えてもらえない
  ため (Fast DDS 2.14 と 3.2。Rolling の 3.6 は全部中継します)、この変数が設定されているとツールは
  自分を `SUPER_CLIENT` にします (stderr にその旨を出します)。`ROS_SUPER_CLIENT` を明示していれば
  それを尊重します。サーバーは Jazzy では `fastdds discovery -i 0 -l <ip> -p <port>`、
  Kilted / Rolling では `fastdds discovery -l <ip> -p <port>` です。
- `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` はそのまま動きます (ノードはループバックの locator だけ
  を広告します)。`OFF` はすべての participant を自分自身に閉じ込めるので何も観測できません。
  その場合ツールは警告を出します。
- 大きなサンプル (2 MB の `UInt8MultiArray`) も SHM のまま流れます。Fast DDS が transport の
  最大メッセージ長に分割します。

## ホスト

`--stats` 無しでは、ホストは `local` (ツールと同じホスト id) か `host:<4 バイトの 16 進>` で表示
されます。`--stats` 付きでは statistics の `PHYSICAL_DATA` トピックからホスト名とプロセス id を
取ります。1 台のマシン上でネットワーク名前空間の異なるコンテナは、ホスト id が同じなのに異なる
IP アドレスを広告することがあり、警告 `host-id-match-but-ip-differs` で報告されます。

## Watch モード

`--watch` は `--interval` 秒ごとに再観測して再描画します。端末では代替スクリーンバッファを使い
(ちらつかず、終了時に元に戻る)、行を端末幅で切り詰め、前フレームからの変化を強調します。

| 印 | 意味 |
|---|---|
| `+` (緑) | ペアが現れた |
| `~` (黄) | transport、確信度、実測 transport、警告のいずれかが変わった |
| `-` (薄い) | ペアが消えた。3 フレーム残してから消す |

トピック行にはそのペアの印が付き、表の後に `changes:` の要約行が出ます。watch 中のキー: `q` 終了、
`p` 一時停止/再開 (停止中の変化は再開時に強調)、`v` ペア行の切り替え、`e` 理由コード凡例の
切り替え、`a` `--all` の切り替え。stdout が端末でないときはエスケープシーケンス無しでフレームを
順に出力します。`--json` では各フレームが 1 つの JSON Lines 文書になり、`changes` オブジェクト
(`added_pairs`、`removed_pairs`、`from`/`to` 付きの `changed_pairs`) が加わります。

色 (`--color auto|always|never`、既定は `auto`、`NO_COLOR` も尊重) は一回きりの表にも適用されます。
transport は web viewer と同じ配色、警告は赤です。
