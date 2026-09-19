# 仕組み

> 英語版が正です。この文書は 2026-09-17 時点の英語版に対応しています。

このツールは Fast DDS 2.14 (ROS 2 Jazzy) と 3.x (Lyrical、Rolling) の両方に対してビルドできます。
API の差分は `include/fastdds_transport_viz/fastdds_compat.hpp` に閉じ込めてあり、以下の判定ルールは
両方で同じです。

`ros2 topic info -v` では transport は分かりません。rmw 層は locator (通信先アドレス) の情報を
公開しないためです。そこで `transport_viz` は自前の Fast DDS `DomainParticipant` を作り、
エンドポイントの discovery を観測します。discovery にはリモートの各 writer / reader が
**広告している locator** (`UDPv4`、`SHM` など) と QoS が含まれます。その情報に、Fast DDS 2.14 が
writer → reader の各ペアで transport を選ぶときと同じルールを適用します。

## 判定ルール

0. **そもそも QoS が合うか?** Fast DDS は request/offer のポリシーが合う writer と reader しか
   マッチさせません: reliability (BEST_EFFORT の writer は RELIABLE の reader に提供できない)、
   durability (writer は reader の要求以上を提供する必要がある: VOLATILE < TRANSIENT_LOCAL <
   TRANSIENT < PERSISTENT)、deadline (writer の周期が reader の周期を超えてはならない)、
   liveliness (種類と lease duration)、ownership (両方 SHARED か両方 EXCLUSIVE)、partition
   (共通の名前。パターン可)。合わなければペアは `NONE` になり、理由 `qos-incompatible-<policy>` と
   警告 `qos-incompatible` が付きます。transport に関係なくデータは流れません。ROS 2 側では
   publisher / subscription の incompatible QoS イベントとして報告される状況です。
1. **同じホストか?** Fast DDS は、2 つの participant の GUID プレフィックス先頭 4 バイトが等しい
   とき同じホスト上にあるとみなします。
2. 同じホストで、両エンドポイントが data-sharing (zero-copy) を広告し、domain id に共通部分がある
   か少なくとも片方が domain id を広告していない → `DATA_SHARING` (確信度 `likely`。
   [data-sharing.ja.md](data-sharing.ja.md) を参照)。広告された domain id が交わらない場合は次へ。
   2 つが別々の `/dev/shm` を使っていると分かる場合は、代わりに `NONE` になります
   ([IPC 名前空間の分断](#ipc-名前空間の分断) を参照)。
3. 同じホストで、両方が SHM locator を広告している → `SHM`。このとき Fast DDS はその participant
   間のユーザーデータに共有メモリだけを使います。discovery は引き続き UDP で行われます。
   2 つの participant が別々の IPC 名前空間で待ち受けていると分かる場合は、代わりに `NONE` に
   なります ([IPC 名前空間の分断](#ipc-名前空間の分断) を参照)。
4. それ以外は、reader が広告するネットワーク locator のうち writer も話せる最初の種類
   → `UDPv4` / `UDPv6` / `TCPv4` / `TCPv6`。
5. 共通の locator が無い → `NONE`。

publisher だけ、または subscription だけのトピックは `-` と理由 `no-matching-reader` /
`no-matching-writer` で表示されます。

`--topic REGEX` は名前が一致するトピックを残します。`--node REGEX` は writer か reader が
完全修飾ノード名 (`/ns/name`) の一致するノードに属するペアを、そのノードの未接続エンドポイントと
ともに残します。残ったペアの相手側は一致しなくても表示されます。2 つのフィルタは AND です。
不正な正規表現は起動時に拒否されます (終了コード 2)。

## LATENCY 列、HZ 列、LOSS 列

`HZ` はペアの reader に 1 秒あたりに届いたサンプル数で、ツールが statistics の `HISTORY_LATENCY`
(届いたサンプル 1 つにつき 1 サンプル、どの配送経路でも) を観測全体、`--watch` では直近 5 秒で
数えて求めます。`≥120` は reader 側の participant の statistics が途中で一部失われ、値が下限で
あることを示します。ペア行だけにあります。[#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137) で外した `RATE` 列
(`PUBLICATION_THROUGHPUT` を元にしていましたが、これは書き込みごとの payload/間隔であってレートでは
ありません) の代わりで、publish レートではなく届いたレートです。詳しくは
[statistics.ja.md](statistics.ja.md#hz-列-1-秒あたりに届いたサンプル数)
([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143)) を参照してください。

`--stats` を付けると、`LATENCY` は statistics の `HISTORY_LATENCY` で、writer の `write()` から
reader への通知までの時間をペアごとに観測期間の平均と最大で示します (`420 µs (max 1.30 ms)`)。
トピック行には最も遅いペアの平均が出ます。2 台のホストのクロックで測るのでマシン間ではその
ずれが含まれ (平均が負なら `latency-clock-skew-suspected` を警告)、同一ホストでは正確です。
`LOSS` は観測期間中のペアの信頼性カウンタの合計です。`lost` は reader の participant が writer の
participant から reader の unicast locator 宛てのパケットを取りこぼしたと報告した数 (`RTPS_LOST`、
シーケンス番号の欠落で participant の組ごと。[statistics.ja.md](statistics.ja.md#rtps_lost) を参照。
警告 `rtps-packets-lost`。reader の participant が publish していなければ `- lost`)、
`resent` は writer が再送した DATA の数 (`RESENT_DATAS`)。どちらも 0 なら `0` です。heartbeat、
gap、acknack、nackfrag は JSON の `measured.reliability` と web viewer のペアカードに出ます。
statistics が無ければ 3 つの列とも `-` です。ペア行の `measured=` は観測中に transport が
実際に運んだ量 (`SHM 148pkt 7.63 MB`。観測前にしか流れていなければ `(idle)`) です。

## 理由コード

すべての判定には機械可読な理由コード (`same-host-guid`、`reader-no-shm-locator` など) が付き、
必要に応じて `!` で始まる警告も付きます。`--explain` は現在の出力で使われているコードの凡例を
末尾に付け、`transport_viz --list-codes` は全コードを表示します。transport の後ろの `?` は
確信度が `certain` ではなく `likely` であることを意味します。

コードは「何が起きたか」を示し、`--advise` は「何を変えるか」を加えます。すべてのコードには
対処 (環境変数名・XML 要素名・QoS ポリシー名を挙げる 1 文。例: `reader-no-shm-locator` →
`FASTDDS_BUILTIN_TRANSPORTS` を外すか reader のプロファイルに SHM transport descriptor を足す、
`shm-stale-files` → `fastdds shm clean`) があるか、正常な状態 (`same-host-guid`)・実測の事実
(`measured-shm-traffic`)・バグ報告の依頼 (`measured-transport-mismatch`) を表すコードには
対処がありません。対処は目標の transport に依存しません。ツールは意図する transport を知らないので、
各文はそのコードが何を妨げているかを名指しし、選ぶのは読み手です。`--advise` はペアごとに
対処のあるコードについて `fix <code>: …` 行を出し (QoS の対処が最も役立つ `NONE` のペアにも)、
凡例の各コードの下にも対処を出します。`--list-codes` / `ros2 transport codes` は各説明の後に、
`--json` は `reason_code_remedies` (`reason_code_descriptions` と同じキー、対処なしは `null`) に、
web viewer は説明の下に表示します。説明文自体には対処を含めないので、それぞれ 1 回だけ現れます。

### native buffer のコンパニオントピック

Lyrical 以降の `rmw_fastrtps_cpp` は、上限の無い `uint8[]` フィールドを持つ型
(`std_msgs/msg/UInt8MultiArray`、`sensor_msgs/msg/Image` など) の writer と reader のそれぞれに、同じ
participant 内で `<topic>/_buf_cpu` 上のコンパニオンを作ります ("native buffers")。トピックのすべての
subscription が native buffer に対応していると、サンプルはコンパニオンだけを通るので、親のペア単体では
DATA サブメッセージもハートビートも配送も見えません。ツールは各コンパニオンを親 (同じ participant、
同じ種別、同じ型、`/_buf_cpu` を除いたトピック名。候補が複数ある場合は、rmw が親の直後に割り当てる
entity key で見分ける) に結び付け、コンパニオンの statistics カウンタを親のペアに加算します。対象は
配送サンプル数、DATA サブメッセージ、再送、ハートビート、GAP、ACKNACK、NACKFRAG、遅延です。
`RTPS_SENT` と `RTPS_LOST` は participant 単位なので、もともと両方を含みます。親のペアには
`buffer-companion-folded` が付き、判定ルールは変わりません。コンパニオンのトピックは
`buffer-companion` 付きで自身の値を保ち、そのすべての endpoint が結び付いていれば `--all` の無い出力
からは除かれます。結び付けられないコンパニオンは `buffer-companion-unmatched` 付きで表示されたままです。
JSON では結び付いたコンパニオンの endpoint が `buffer_parent_guid` に親を示します。
`rmw_fastrtps_dynamic_cpp` はコンパニオンを作らず、Humble と Jazzy ではどの RMW も作りません
([#119](https://github.com/atinfinity/fastdds_transport_viz/issues/119))。

判定ロジックは `src/fastdds_transport_viz/src/decision.cpp` に DDS 依存の無い純粋関数として
実装され、`test/test_decision.cpp` でテストされています。

## ノード名とツール自身の痕跡

ROS のノード名は rclcpp のグラフ API (エンドポイント GID → ノード) で解決するため、ツールは
隠しノード `_transport_viz_<pid>` を登録します。ツール自身のエンドポイントは出力から除外されます。
discovery の観測は別の生の Fast DDS participant で行い、rmw 自身の discovery リスナには触れません。

グラフ API は `ros_discovery_info` を読みます。各ノードの rmw はここにノード名とエンドポイント GID を
publish します。rclcpp の participant は SHM を広告するため、ツールと同じ host id で別の IPC 名前空間に
いるノードはこのサンプルを自分の `/dev/shm` に書き込み、rclcpp は rmw の
`_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_` を返します。そこで生の participant も
`ros_discovery_info` を自分で読みます。その reader は統計の reader と同じく、participant の SHM 以外の
unicast locator (UDP、`LARGE_DATA` では TCP) だけを広告します。Humble ではこの reader は SHM
トランスポートを持たない専用の participant に置きます。Fast DDS 2.6 は、SHM を持つ participant が受け取る
discovery データから同一ホストのエンドポイントの SHM 以外の locator を取り除くため、ノードの writer は
生の participant 上の reader に SHM でしか送れないからです。この名前でグラフ API が名前を返さない
エンドポイントを補い、両方で分かる場合はグラフ API の名前を使います。ノードかツールに SHM 以外の
トランスポートが無い場合 (`FASTDDS_BUILTIN_TRANSPORTS=SHM`) や、ノードのサンプルを復号できない場合
(別の ROS ディストリビューションのノード。GID は Humble で 24 バイト、Jazzy 以降で 16 バイト) は、
名前は読めません。そのようなエンドポイントのノード名は生の DDS エンドポイントと同じく空になり、表は
GUID で表示し、`--node` には一致せず、`diff` は GUID で対応付け、web viewer は participant を表示します。
以前のバージョンが書いた JSON の不明ノード名も空として読みます ([#112](https://github.com/atinfinity/fastdds_transport_viz/issues/112))。

## ノードと同じ場所で実行する

ツールは Fast DDS が読む環境をそのまま読み、変更はしません。観測したいノードと同じシェル環境で
実行してください。`FASTDDS_BUILTIN_TRANSPORTS`、`FASTRTPS_DEFAULT_PROFILES_FILE` (観測用
participant もノードと同様に既定の participant プロファイルをここから取ります)、
`ROS_DISCOVERY_SERVER`、`ROS2_EASY_MODE`、`ROS_AUTOMATIC_DISCOVERY_RANGE`、`ROS_STATIC_PEERS` を揃え、ネットワークと
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
  Lyrical / Rolling では `fastdds discovery -l <ip> -p <port>` です。
- `ROS2_EASY_MODE=<ip>` (Fast DDS 3.2 以降: Kilted、Lyrical、Rolling) を設定すると、Fast DDS は
  ホストとドメインごとに Discovery Server を 1 つ自動起動し (ポートは 7400 + 250 × ドメイン + 2、
  `fastdds discovery list` で確認できます。CLI は起動済みのサーバーを `ss` で探すので `iproute2` が
  必要です)、すべての participant を `P2P` builtin transport に
  切り替えます。ユーザーデータは SHM と TCPv4、discovery はローカルサーバーへの UDPv4 ユニキャストで、
  マルチキャストは一切使いません。判定は `LARGE_DATA` と同じで、同一ホストでは `SHM`、ホスト間では
  `TCPv4` (`common-tcpv4-locator`)、`--stats` は TCP のトラフィックを測定します。ツールもノードと
  同じ値の `ROS2_EASY_MODE` で起動してください (stderr にその旨を出します)。設定していない
  participant からはノードが見えません。実行するのはノードが動いているホスト、特にマスターの
  ホストが適しています。Fast DDS 3 はエンドポイントの型を解決してからでないとツールに渡さず、
  その型を使うノードのないホストではサーバー同士の中継を通じても解決されないため、そのような
  ホストからはツールも `ros2 topic list` もノードは見えてもトピックが 1 つも見えません。自動起動した
  サーバーはエンドポイントを持たない participant なので、`--all` を付けても表には現れません。
  `--stats` では、それを起動したノードに `FASTDDS_STATISTICS` が設定されていれば statistics の
  participant として現れます。Fast DDS が participant ごとに実行する `fastdds discovery` CLI は
  stdout に出力しますが、ツールはそれを stderr に回して `--json` を壊さないようにしています。
- `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` はそのまま動きます (ノードはループバックの locator だけ
  を広告します)。`OFF` はすべての participant を自分自身に閉じ込めるので何も観測できません。
  その場合ツールは警告を出します。
- 大きなサンプル (2 MB の `UInt8MultiArray`) も SHM のまま流れます。Fast DDS が transport の
  最大メッセージ長に分割します。

## Fast DDS 2.6 (ROS 2 Humble)

Humble の Fast DDS 2.6 では 2 点が異なります。

- **statistics が無い。** Humble のバイナリは statistics モジュール無しでビルドされています
  (`config.h` で `FASTDDS_STATISTICS` が無効)。`FASTDDS_STATISTICS` を設定しても観測対象ノードは
  statistics を出せません。`--stats` は警告を出し、すべてのペアが `stats-not-enabled-on-writer` に
  なり、`LATENCY`、`HZ`、`measured=` は空のままです。モジュールを有効にしてビルドした Fast DDS
  なら、ツールの 2.6 対応で動きます。
- **同一ホストの locator がフィルタされる。** 2.10 より前の Fast DDS は、同一ホストの participant に
  ついて SHM locator しかツールに広告しません。相手に SHM locator が無い (UDP のみの participant)
  場合、共通の locator 種別が見えないので、ツールは `UDPv4?` と理由 `same-host-locators-hidden`
  を出します。双方とも組み込みの UDPv4 transport を持っており、Fast DDS は実際に UDPv4 に
  フォールバックします。
- `FASTDDS_BUILTIN_TRANSPORTS` (Fast DDS 2.12 以降) と `ROS_AUTOMATIC_DISCOVERY_RANGE` /
  `ROS_STATIC_PEERS` (ROS 2 Iron 以降) はありません。transport は XML プロファイルで設定します
  (`test/launch/udpv4_only.xml` が UDPv4 のみの participant の例)。

## ホストとアドレス

`--stats` 無しでは、ホストは `local` (ツールと同じホスト id) か `host:<4 バイトの 16 進>` で表示
されます。`--stats` 付きでは statistics の `PHYSICAL_DATA` トピックからホスト名とプロセス id を
取ります。このときテーブルは各エンドポイントを `node@host(pid)` と表示し、`--json` にはエンド
ポイントごとの `host_name` (`<ホスト名>:<数値のホスト id>` の形式) と `process`、および participant
ごとの host / user / process を並べた `stats.physical` が入ります。1 台のマシン上でネットワーク
名前空間の異なるコンテナは、ホスト id が同じなのに異なる IP アドレスを広告することがあり、警告
`host-id-match-but-ip-differs` で報告されます。

アドレスそのものはテーブルの列にはありません (テーブルは transport の種別と理由コードを示します)
が、`--locators` を付けると verbose のテーブルのペア行の下に 1 行追加され、ツールが選んだ locator と、
`--stats` があれば実際にパケットを運んだ locator が表示されます。

```
$ ros2 transport list -v --locators --stats --topic '^/(chatter|bounded)$'
    /talker@host(61) -> /listener_udp@host(49)  UDPv4  414 us  0  measured=UDPv4 9pkt 1.19 kB  ...
        locators: UDPv4 127.0.0.1:7411 (selected = measured, 9 pkt)
    /talker@host(61) -> /listener@host(50)      SHM    453 us  0  measured=SHM 10pkt 1.31 kB   ...
        locators: SHM port 7413 (selected = measured, 10 pkt)
    /bounded_pub@host(56) -> /bounded_sub@host(55)  DATA_SHARING  195 us  0  ...
        locators: selected DATA_SHARING (no locator) | measured SHM port 7419 (1 pkt)
```

`selected` の語は、実測側が隣に並ぶときだけ現れます。SHM locator はアドレスではなく writer が
書き込む `/dev/shm` のポートを名乗るので、テーブルの下に出る共有メモリの行が数えている
`fastrtps_port<N>` と対応づけられます。マルチキャストアドレスには `(multicast)` が付きます。
zero-copy data-sharing には locator がそもそもありません。Fast DDS 2.10 より前では予測が使う
locator がツールに届かないので、`UDPv4 (hidden by Fast DDS < 2.10)` と表示されます。
選ばれた locator にパケットが 1 つも流れなかった場合 (reader が広告した別の locator、典型的には
マルチホームのホストの別インターフェースを通った場合) は、`!measured-locator-mismatch` が付きます。

`--locators` は `-v` を暗黙に有効化し、`--json` では無視されます。JSON は同じ情報を常に
`pairs[].locator` と `pairs[].measured.locators[]` に持っているためです。各エンドポイントが広告した
locator も、`--stats` 無しの discovery だけで `--json` に出ています。

```
ros2 transport list --json | jq '.topics[].writers[] | {node, unicast_locators}'
```

```json
{
  "node": "/bounded_pub",
  "unicast_locators": [
    { "kind": "SHM",   "address": "",          "port": 8169 },
    { "kind": "UDPv4", "address": "127.0.0.1", "port": 8169 }
  ]
}
```

`unicast_locators` と `multicast_locators` は公開スキーマ
(`schema/transport_viz.schema.json`) の一部です。ただし次の 3 点は読み取れません。

- これは participant が**広告した** locator、つまり受信を受け付けるアドレスであって、パケットの
  実際の送信元ではありません。NAT 越しや bridge ネットワーク上のコンテナでは、ツールを実行して
  いる場所から到達できないアドレスを広告することがあります。
- SHM locator の `address` は空です。`port` は Fast DDS の共有メモリポート (`/dev/shm` の
  `fastrtps_port<N>`) であり、ネットワークポートではありません。
- Fast DDS 2.10 より前では、同一ホストの相手について SHM locator しかツールに届きません。
  [Fast DDS 2.6](#fast-dds-26-ros-2-humble) を参照してください。

`--stats` 付きなら、実際にパケットを運んだ locator も報告されます。`stats.traffic[]` に
`dst_locator` (`RTPS_SENT`、送信側 participant がキー)、`stats.lost[]` にも `dst_locator`
(`RTPS_LOST`、報告した受信側 participant と送信側 participant がキー) があり、いずれも `kind` / `address` / `port` を持ちます。
Fast DDS は同一ホストの participant の locator を `127.0.0.1` / `::1` として報告する一方、リモート
の writer の `RTPS_SENT` は実際のアドレスを名乗るので、ツールはトラフィックを reader に対応づける
ときに両方の表記を突き合わせます。

## 環境の共有メモリ

SHM の判定は環境の共有メモリに依存するので、毎回の出力の末尾に、ツールが動いている環境の
共有メモリについて 1 行を出します:

```
shared memory: /dev/shm 396 MB used of 16.7 GB (16.3 GB free) | Fast DDS 63.4 MB in 114 segment(s) (110 stale), 14 port(s) (7 stale), 6 data-sharing histories (6 unmatched)
  !shm-stale-files: 117 file(s) without a living owner, run 'fastdds shm clean'
```

- **容量** は `statvfs("/dev/shm")` の値です: tmpfs の合計、使用中、空きバイト数。Docker は
  `--shm-size` や `--ipc=host` を指定しない限りコンテナに 64 MB しか与えません。Fast DDS は
  participant ごとに 1 つのセグメント (既定 512 KB、large data ではより大きい) を必要とし、
  ディレクトリが一杯だと作成に失敗します。
- **Fast DDS のファイル** は `fastrtps_*` (Fast DDS 3.x では `fastdds_*`) と `fast_datasharing_*`
  のエントリです: participant
  ごとの *セグメント* (`fastrtps_<hex>`)、SHM locator ごとの *ポート* のリングバッファ
  (`fastrtps_port<N>`)、zero-copy 配送を使う writer ごとの *data-sharing 履歴*。サイズは
  (小さな `sem.fastrtps_*` の mutex ファイルも含めて) 合計して表示します。
- **stale** なファイルは、`_el` ロックファイルが存在するのに誰も保持していないセグメントと
  ポートです (`fastdds shm clean` と同じ `flock` による判定)。所有プロセスが後始末せずに
  死んだもので、`/dev/shm` を消費し続けます。警告 `shm-stale-files` が `fastdds shm clean` を
  勧めます。これがまさにそれらを削除します。観測対象ノードがどれもこの IPC 名前空間にいない場合
  (下の可視性を参照) は、ここにあるものはノードのものではあり得ないので stale の数は報告しません。
  data-sharing 履歴にはロックがありません。発見済みの writer に属するものは writer 側に
  報告し (JSON の `datasharing_history_bytes`、web viewer のエンドポイント詳細)、残りは
  *unmatched* (別ドメイン、または終了した writer) として数えます。
  data-sharing の reader も同じ形の *通知* セグメント (`fast_datasharing_<reader の GUID>`) を
  持ちます。発見済みの reader のものは別に数えます (`datasharing_notifications`。あるときだけ表示)。
- **可視性**: ツールと同じ IPC 名前空間にいるノードは、自分の SHM ポートファイル
  (`fastrtps_port<N>_el`) のロックを保持しています。観測対象ノードのホスト id が違う、その
  ポートがここで保持されていない、またはツール自身のポート番号と同じ (別のネットワーク名前空間で
  同じ participant id) 場合、ノードは別の `/dev/shm` を使っており、警告 `shm-not-visible` が
  それを示します。この場合の数値はツールの環境のもので、ノードの環境のものではなく、ノードと
  このプロセスの間で SHM は使えません。ツール自身のポートは、プロセス内の全 participant
  (`DomainParticipantFactory::lookup_participants()`) のポートと、プロセスが開いたまま保持している
  ポートロック (`/proc/self/fd`) なので、ツール自身のロックをノードのものと取り違えません。送信側は
  書き込み先のポートをロックしないため、ホスト id がツールと同じで IPC 名前空間だけが別
  (`ipc: host` の無い `network_mode: host`) のノードも検出されます。
  そのようなノード同士でも Fast DDS は SHM を選びます (ホスト id が同じ) が、ポートは別々の
  `/dev/shm` にあるためメッセージはすべて失われます。ツールが見分けられる場合、ペアは `NONE` と
  `shm-ipc-namespace-split` になります ([IPC 名前空間の分断](#ipc-名前空間の分断) を参照)。
- `shm-nearly-full` は使用率 90 % 以上、または空きが 16 MiB 未満で警告します。

`/dev/shm` が無い環境 (macOS) では行自体を省きます。JSON では同じデータが `shm` オブジェクト
になり (`missing_ports` はここにロックファイルの無いアナウンス済みポート、`unknown_ports` は
ロックを調べられなかったポート)、`--watch` ではフレームごとに更新されます。

可視性の判定の根拠は participant ごとに文書の `participants` 配列に出ます
([#125](https://github.com/atinfinity/fastdds_transport_viz/issues/125))。発見した participant
1 つにつき 1 要素で、`guid_prefix`、`host_id`、`host`、`host_name`、`own` (ツール自身のプロセスの
participant)、`shm_visibility` (`visible`、`not-visible`、`unprobed`)、そして `shm_ports` に
ツールのホスト上でアナウンスされた SHM ポートと、ツールの IPC 名前空間から調べたロックの状態
(`held`、`own`、`absent` = ロックファイルが無い、`stale` = 誰も持っていないロック、`unknown` =
読めない、`unprobed` = `/dev/shm` が無いか別ホスト)、`announced_by` (その番号をアナウンスする
participant の数)、`proof` (そのポートの held なロックが根拠になるか。
[IPC 名前空間の分断](#ipc-名前空間の分断) 参照) を持ちます。participant を 1 つでも発見すれば
どのプラットフォームでも出ます。web viewer はエンドポイントパネルの `shm` 行に同じ内容を出します。

### IPC 名前空間の分断

Fast DDS はホスト id が同じなら共有メモリで相手の participant に届くとみなします。
`network_mode: host` のコンテナはホスト id を共有しますが、`ipc: host` (または共通の
`ipc: service:…` / `ipc: container:…`) が無いコンテナはそれぞれ自分の `/dev/shm` を持ちます。
すると writer は誰も待ち受けていないポートファイルに書き込み、listener には何も届きません。どちらの
側にもエラーは出ません。次のいずれかが成り立つとき、ツールはそのペアを `NONE`、`certain`、警告
`shm-ipc-namespace-split` と報告します:

- `shm-port-collision`: writer と reader の participant が同じ SHM ポート番号を広告している。
  1 つのポートで待ち受けられるのは IPC 名前空間ごとに 1 つの participant だけなので、番号が
  等しいのは名前空間が 2 つある証拠です。コンテナにノードが 1 つずつの構成では典型的に起こります:
  名前空間ごとに同じ番号からポートを割り当てるためです (Jazzy 以降では `ros_discovery_info` の
  reader のポートで、最初のノードは 7000)。ツールの IPC 名前空間には依存しませんが、ツールは
  自分と同じホストの participant の SHM ポートだけを集めるので、ノードと同じホスト id (ホスト
  ネットワーク) で動かす必要があります。
- `shm-reader-port-not-visible` / `shm-writer-port-not-visible`: ツールの IPC 名前空間から見て、
  片方の participant の SHM ポートはすべて保持されていて、そのうち 1 つは他の participant が広告
  しておらず、`ros_discovery_info` の reader 以外のエンドポイントが広告している。もう片方のポートは
  ここにロックファイルが無いか、ツール自身のポートである。ツールがどちらかの側と同じ IPC 名前空間に
  いる必要があります。ロックが空いているだけ (直前に終了したノードが残したもの) では判定しません。
  次の 2 種類の保持されたポートは根拠にも反証にもならず、判定をその participant の他のポートに
  委ねます: 他の participant も同じ番号を広告しているポート (IPC 名前空間が複数あると、どちらの
  ロックか分からない) と、`ros_discovery_info` の reader の 7000 番台のポート (その番号は名前空間内の
  どの Fast DDS participant も、ドメインに関係なく取り得る)
  ([#118](https://github.com/atinfinity/fastdds_transport_viz/issues/118))。Humble では participant の
  SHM ポートは 1 つだけで、IPC 名前空間ごとに番号が振られます: もう一方の名前空間の別のノードが
  片側と同じ番号を取り、両側どうしの番号は衝突しない場合、どちらの判定も働かずペアは `SHM` の
  ままです。

data-sharing のエンドポイント (規則 2) も同じように失敗します。Fast DDS は QoS だけでそれらを
組み合わせますが、reader は自分の `/dev/shm` で writer の history を開けずに writer を拒否し、writer は
その reader に transport 経由で何も送らないため、サンプルは届きません。このペアは、両側が SHM を
広告していれば上記の証拠で、SHM transport が無くても存在する data-sharing のセグメントでも、
`NONE`、`certain`、`shm-ipc-namespace-split` になります:

- `datasharing-reader-segment-not-visible` / `datasharing-writer-segment-not-visible`: writer の
  history (`fast_datasharing_<writer guid>`) がツールの `/dev/shm` にあり、reader の通知セグメント
  (`fast_datasharing_<reader guid>`) が無い、またはその逆。ツールがどちらかの側と同じ IPC 名前空間に
  いる必要があります ([#110](https://github.com/atinfinity/fastdds_transport_viz/issues/110))。

このように報告されたペアには `--advise` が対処を示します: 両方のノードを 1 つの IPC 名前空間に
入れる (`ipc: host`) か、片側の SHM を無効にして UDPv4 が選ばれるようにし、data-sharing のペアでは
data-sharing も無効にします (QoS プロファイルで `data_sharing` を OFF)。

どちらも分からない場合 (たとえばツールが 3 つ目の IPC 名前空間にいて、両側のポート番号が違う
場合) はペアは `SHM` (または `DATA_SHARING`) のままで、手がかりは共有メモリ行の `shm-not-visible`
だけです。両側のノード名は
表示されます: ツールは UDP でノード名を読みます (上の「ノード名とツール自身の痕跡」、[#112](https://github.com/atinfinity/fastdds_transport_viz/issues/112))。

`--stats` 付きでは、このペアで writer の SHM トラフィックが計測されても想定どおり (writer は自分の
`/dev/shm` のポートファイルに書き込むため。data-sharing の writer もハートビートは送る) なので、ペアは
`NONE` のままです。配送が証明された場合に
限り、分断の判定と矛盾するため `shm-ipc-namespace-split-but-delivered` が付きます。両端が SHM を
広告するペアで観測中に SHM 以外のパケットが流れた場合も矛盾です (Fast DDS は同一ホストの両者の
トラフィックを SHM だけで送ります)。この場合は `shm-ipc-namespace-split-but-non-shm-traffic` が
付きます ([#111](https://github.com/atinfinity/fastdds_transport_viz/issues/111))。どちらの警告も判定は
変えず、報告を求めるものです。片側が SHM を持たない data-sharing のペアには付きません。statistics は
participant 単位で、2 つのノードの他のエンドポイントは正当に UDP で通信するためです。statistics 自体は
IPC 名前空間をまたいでも失われません。ツールの statistics reader は participant の UDP (または TCP) の
locator だけを広告し SHM の locator を広告しないので、同一ホストの writer はツールがどこにいても
statistics をネットワークスタック経由で送ります ([#106](https://github.com/atinfinity/fastdds_transport_viz/issues/106))。

## Watch モード

`--watch` は `--interval` 秒ごとに再観測して再描画します。端末では代替スクリーンバッファを使い
(ちらつかず、終了時に元に戻る)、行を端末幅で切り詰め、前フレームからの変化を強調します。

| 印 | 意味 |
|---|---|
| `+` (緑) | ペアが現れた |
| `~` (黄) | transport、確信度、実測 transport、警告のいずれかが変わった |
| `-` (薄い) | ペアが消えた。行は薄い表示で残る |

どの印も変化から 3 フレーム残り、その後は通常の行に戻ります (消えたペアの行は削除)。

トピック行にはそのペアの印が付き、表の後に `changes:` の要約行が出ます。新しい
listener が現れ、UDP の listener が消えた直後の 1 フレーム:

![watch frame](images/example-watch.svg)

watch 中のキー: `q` 終了、
`p` 一時停止/再開 (停止中の変化は再開時に強調)、`v` ペア行の切り替え、`e` 理由コード凡例の
切り替え、`a` `--all` の切り替え。stdin と stdout の両方が端末でない限り、フレームを順に出力します
(`--color always` でなければエスケープシーケンス無し)。`--json` では各フレームが 1 つの JSON Lines 文書になり、`changes` オブジェクト
(`added_pairs`、`removed_pairs`、`from`/`to` 付きの `changed_pairs`) が加わります。

色 (`--color auto|always|never`、既定は `auto` で、`auto` は `NO_COLOR` を尊重) は一回きりの表にも適用されます。
transport は web viewer と同じ配色、警告は赤です。

![colored table](images/example-table.svg)

どちらの画像も実際の出力です (`scripts/render_examples.sh` が `--color always` で採取し、
`scripts/ansi2svg.py` が ANSI の色を SVG に変換します)。

## 2 つのスナップショットの比較

よくある流れは *プロファイルや環境変数を変えて、もう一度実行して、何が変わったかを見る*
です。`transport_viz diff before.json after.json` (`ros2 transport diff`) は、watch モードの比較を
保存した 2 つの `--json` 文書に対して行います。DDS participant は作りません。

```
ros2 transport list --json > before.json
# XML プロファイル / FASTDDS_BUILTIN_TRANSPORTS / ... を変えてノードを再起動
ros2 transport list --json > after.json
ros2 transport diff before.json after.json
```

後のスナップショットが `--watch` の印の列付きの表として出ます。現れたペアは `+`、transport、
確信度、実測 transport、選ばれた locator、実測 locator、警告のいずれかが変わったペアは `~`、
消えたペアは `-` (以前のラベルを持つ薄い行。トピックごと消えた場合は薄いトピック行) です。
表の後に `changes:` の要約行が続きます。一回きりの実行の表示オプションは比較の前に両方の文書へ
適用されます: `--topic`、`--node`、`--all` (無ければ観測時と同じくサービスと生の DDS トピックは
除外)、`-v`、`--explain`、`--locators`、`--advise`、`--color`。`--changes-only` は印か削除された
ペアのあるトピックだけを残します。

**ペアの対応付け。** `--watch` はフレーム間でペアを `(トピック, writer GUID, reader GUID)` で
対応付けます。2 回の実行の間にはたいていノードを再起動し、再起動のたびにエンドポイントの GUID
は変わるので、この鍵ではすべてのペアが削除されて追加し直されたように見えます。そのため `diff`
は既定で `(トピック, writer ノード, reader ノード)` で対応付けます (`--key node`)。1 つのノードが
同じトピックに複数の writer や reader を持つ場合は GUID 順に対応付け、ROS ノード名の無い
エンドポイントは GUID で対応付けます。同じ理由で、node キーでは選ばれた locator と実測 locator の
ポート番号は無視します (再起動で 7413、7415、… と participant id ごとに振り直されるため)。
種類とアドレスは比較対象のままです。`--key guid` は同じ実行の 2 フレームに対する `--watch`
そのものの意味になります。

**終了コード** は `diff(1)` に従います: 変化なしで 0、変化ありで 1、使い方の誤り、読めない
ファイル、別の `schema_version` の文書、不正な文書で 2。2 つの文書のドメインが違う場合は
警告だけです。片方の文書は `-` (標準入力) にでき、`--watch --json` が書いた JSON Lines ファイルは
最後の文書が使われます (stderr にその旨が出ます)。

**JSON。** `--json` では、後の文書に `--watch --json` の `changes` オブジェクトが加わったものが
出ます: `added_pairs`、`removed_pairs`、`changed_pairs` (各ペアの鍵は `topic`、GUID、
`writer_node` / `reader_node` を持ち、`changed_pairs[].from` は前の文書でのペアの GUID も持つ)
に加えて `key` (`node` か `guid`) と `before` (前の文書の `observed_at` と `domain`)。
`--changes-only` は表と同じように `topics` を刈り込みます。文書はスキーマに適合するので、
`jq .changes` で比較だけを取り出せ、[web viewer](web-viewer.ja.md#2-つの文書の比較) はそれを強調表示します
(viewer 自身で 2 つの文書を比較することもできます)。
