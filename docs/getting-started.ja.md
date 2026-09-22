# はじめに

> 英語版が正です。この文書は 2026-09-17 時点の英語版に対応しています。

このページでは、素の ROS 2 環境から、最初の `ros2 transport list`、statistics、web viewer
までを通します。動作環境は Linux です。ツールは Fast DDS を観測するので、観測対象のノードは
`rmw_fastrtps_cpp` (Jazzy、Lyrical、Rolling の既定 RMW) か `rmw_fastrtps_dynamic_cpp` を使っている必要があります。

| ROS 2 ディストリビューション | Fast DDS | 備考 |
|---|---|---|
| Humble (Ubuntu 22.04) | 2.6 | 予測のみ: Humble のバイナリには statistics モジュールが無く `--stats` は何も測れない。同一ホストの相手は SHM locator しか見えない ([仕組み](how-it-works.ja.md#fast-dds-26-ros-2-humble) 参照) |
| Jazzy (Ubuntu 24.04) | 2.14 | 主対象 |
| Lyrical (Ubuntu 26.04) | 3.6 | 現行 LTS |
| Rolling | 3.x | ベストエフォート (CI は失敗を許容) |

Kilted は 1.0.0 までの対応で、2026 年 12 月に EOL を迎えるため 1.1.0 以降は対象外です。
`1.0.0` タグは Kilted でもビルドでき、動作します。

## 1. ビルド

`src/` に 2 つのパッケージがあります: `fastdds_transport_viz` (C++ のツール本体) と
`ros2transport` (`ros2 transport` コマンド)。他の ROS 2 パッケージと同じように colcon
ワークスペースでビルドします。

### ネイティブ環境 (推奨)

前提: ROS 2 の desktop または base インストール、`python3-colcon-common-extensions`、
`rosdep` (初回のみ `sudo rosdep init && rosdep update`)。

```
mkdir -p ~/ws/src && cd ~/ws
git clone https://github.com/atinfinity/fastdds_transport_viz.git src/fastdds_transport_viz
source /opt/ros/jazzy/setup.bash              # または humble / lyrical / rolling
rosdep install --from-paths src --ignore-src -y
colcon build --symlink-install
source install/setup.bash
```

`rosdep` はビルド依存 (`rclcpp`、Fast DDS のヘッダ、`nlohmann-json`)、実行時依存
(`rmw_fastrtps_cpp`、例で使う `demo_nodes_cpp`)、テスト依存を入れます。ツールを動かす
シェルでは毎回 `source install/setup.bash` が必要で、これが `ros2 transport` コマンドも
登録します。

### Docker (代替)

リポジトリには `compose.yaml` があり、開発用イメージ (`ros:jazzy`、または
`ROS_DISTRO=humble` / `lyrical` / `rolling`)、`/ws` にマウントしたリポジトリ、ホストの共有メモリが
ツールから見えるようにする `ipc: host` を定義しています:

```
docker compose build
docker compose run --rm dev bash
colcon build --symlink-install && source build/$ROS_DISTRO/install/setup.bash
```

このページの残りはそのシェルの中でも同じように動きます。コンテナはネットワーク名前空間が
別なので、同じコンテナ内のノードは観測できますが Docker ホスト上のノードは観測できません。
そのための `hostnet` サービスは [development.md](development.md#docker-environment) (英語)
を参照してください。

## 2. 最初の実行

デモノードを 2 つ起動して見てみます:

```
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --explain
```

```
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT  LATENCY  HZ  LOSS  REASON
/chatter  std_msgs/msg/String  1     1     SHM x1     -            -     same-host-guid,datasharing-disabled-writer,both-shm-locators
    /talker@local -> /listener@local  SHM  -    -  same-host-guid,datasharing-disabled-writer,both-shm-locators

shared memory: /dev/shm 2.19 MB used of 16.7 GB (16.7 GB free) | Fast DDS 2.19 MB in 3 segment(s), 6 port(s), 0 data-sharing histories

Reason codes:
  both-shm-locators
      Both endpoints announce a shared-memory locator ...
```

見えているもの:

- トピックごとに 1 行、(`-v` で) writer → reader のペアごとに 1 行。予測された transport と、
  その根拠の理由コード (`--explain` で説明を表示、`ros2 transport codes` で全コードを一覧)。
- `shared memory:` の行は、ツールが動いている環境の `/dev/shm` の状態です
  ([how-it-works.ja.md](how-it-works.ja.md#環境の共有メモリ))。

共有メモリを使えない 2 つ目の listener を作ると、判定が変わります:

```
FASTDDS_BUILTIN_TRANSPORTS=UDPv4 ros2 run demo_nodes_cpp listener &
ros2 transport list -v                 # 2 つ目のペア: UDPv4, reader-no-shm-locator
```

ここまで、観測対象のノードには何も要求していません。判定はどの Fast DDS participant も
広告する discovery データから求めています。

!!! note "ノードと同じ場所で実行する"
    discovery データは同じ DDS ドメインからしか見えず、共有メモリは同じ IPC 名前空間からしか
    見えません。観測するノードと同じ環境変数 (`ROS_DOMAIN_ID`、`FASTDDS_BUILTIN_TRANSPORTS`、
    XML プロファイル、Discovery Server の設定)、同じネットワーク/IPC 名前空間でツールを
    実行してください。[how-it-works.ja.md](how-it-works.ja.md#ノードと同じ場所で実行する) を参照。

## 3. 予測ではなく実測する (`--stats`)

予測は Fast DDS の statistics モジュールで確認できます。観測対象のノードは起動前に
環境変数で有効にしておく必要があります:

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --stats
```

ペアの行に `measured=SHM 47pkt 3.20 kB` が付き、`LATENCY` 列にそのペアの write-to-notification
遅延 (平均と最大)、`HZ` 列に reader に届いた 1 秒あたりのサンプル数 (statistics の一部が失われて
下限になったときは `≥` 付き)、`LOSS` 列に欠落と再送が出て、ホストは名前と
プロセス id で表示されます。プロファイルファイルは statistics writer の
リソース制限を外すためのものです。トピックの説明と回避する落とし穴は
[statistics.ja.md](statistics.ja.md) を参照してください。

## 4. 見続ける

```
ros2 transport list --watch --stats --interval 2
```

2 秒ごとに再観測し、前のフレームからの変化に印を付けます (`+` 現れた、`~` 変わった、`-`
消えた)。キー: `q` 終了、`p` 一時停止、`v` ペア表示、`e` 凡例、`a` 全トピック、`l` locator、`f` 対処 (`--advise`)。

## 5. ブラウザで見る

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1
# transport_viz_web: listening on http://127.0.0.1:8765/  (serving .../share/fastdds_transport_viz/web)
```

URL を開くと、ホストが列、ノードが箱、ペアが transport ごとに色分けされた矢印として
ライブで更新されます。`ros2 transport list --json > snapshot.json` で作った文書は、同じ
ページ (`web/index.html`) でオフラインでも開けます。[web-viewer.ja.md](web-viewer.ja.md) を
参照してください。

サーバは `127.0.0.1` でだけ待ち受けます。Docker 環境では、viewer のポートを公開してシェルを起動し、
コンテナ内ではすべてのアドレスで待ち受けると、ホストのブラウザから届きます:

```
docker compose run --rm --service-ports dev bash
ros2 run fastdds_transport_viz transport_viz_web --bind 0.0.0.0 --stats --interval 1
```

同じ `--bind 0.0.0.0` で、ノート PC からロボットを見ることもできます。

## 6. コマンドリファレンス

```
ros2 transport list [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--node REGEX]
                    [--all] [-v] [--explain] [--locators] [--advise] [--stats] [--json | --csv]
                    [--color auto|always|never] [--watch [--interval S]]
ros2 transport diff BEFORE.json AFTER.json [--key node|guid] [--changes-only] [--json]
                    [--topic REGEX] [--node REGEX] [--all] [-v] [--explain] [--locators]
                    [--advise] [--color auto|always|never]
ros2 transport codes
```

`ros2 transport` は `fastdds_transport_viz` の `transport_viz` バイナリを exec します。
バイナリは `ros2 run fastdds_transport_viz transport_viz` で直接実行でき、同じオプションに
加えて `--list-codes` があります。終了コード: 成功 0、使い方の誤り 2、RMW が `rmw_fastrtps_cpp` でも
`rmw_fastrtps_dynamic_cpp` でもないとき (メッセージに RMW 名が出る) と、バイナリが見つからない・起動できないときの `ros2 transport` は 1。

`ros2 transport diff` は保存済みの 2 つの `--json` 文書を、何も観測せずに比較します
([how-it-works.ja.md](how-it-works.ja.md#2-つのスナップショットの比較))。終了コードは `diff(1)` と同じで、
変化なし 0、変化あり 1、エラー 2 です。

## うまくいかないときの最初の確認

`ros2 transport list --advise` は、使われている理由コードを解消するには何を変えるかを
ペアの下と凡例に出します (`fix reader-no-shm-locator: Enable SHM on the reader's
participant: unset FASTDDS_BUILTIN_TRANSPORTS ...`)。`ros2 transport codes` は全コードの対処を
一覧します。下の表は理由コードにならないものを扱います。

| 症状 | 確認すること |
|---|---|
| `ros2: error: argument Call ... invalid choice: 'transport'` | このシェルで `source install/setup.bash` (Docker イメージでは `build/$ROS_DISTRO/install/setup.bash`) したか。`ros2transport` が同じワークスペースでビルドされているか。 |
| `RMW is rmw_cyclonedds_cpp; this tool observes Fast DDS ...` (exit 1) | ツールは Fast DDS でしか動きません。ツールを実行するシェルで `RMW_IMPLEMENTATION=rmw_fastrtps_cpp` (または `rmw_fastrtps_dynamic_cpp`) を設定する (既定が Fast DDS の distro なら未設定でもよい)。 |
| トピックが 1 つも出ない | ノードと同じ `ROS_DOMAIN_ID` か。`ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` は各 participant を自分だけに限定する。Discovery Server 使用時はツールにも同じ `ROS_DISCOVERY_SERVER` が必要 (自動で SUPER_CLIENT になる)。 |
| 別マシンのノードが出ない | 相手のマシンにマルチキャストで届くか、`ROS_STATIC_PEERS` に列挙するか、双方が Discovery Server を使う。[development.md](development.md#two-physical-hosts) (英語) 参照。 |
| `--stats` で `!stats-not-enabled-on-writer` | ノードが `FASTDDS_STATISTICS` 無しで起動された (変数はノードの起動前に設定する) か、その participant が SHM transport しか持たない。ツールの statistics reader は UDP か TCP で受信する。 |
| `!stats-samples-lost`、または `warning: ... statistics samples were lost` | statistics の一部がツールに届かず、配送が証明されているのに実測パケットの無いペアが 1 つ以上ある (`stats.pairs_delivered_unmeasured`)。実際には通信していても、ペアが `(unmeasured, delivered)` や `none(delivered)` と表示され得る。実測を失わせなかった損失は、この警告なしで `N sample(s) lost` とだけ出る。statistics を有効にするノードを減らすか、`FASTDDS_STATISTICS` を必要な別名に絞る (transport を見るだけなら `RTPS_SENT_TOPIC;RTPS_LOST_TOPIC` で足りる)。`--timeout` を延ばしても損失は減らず、より多く集めるだけ。`--json` では `stats.samples_lost` に数が出る。そのうち `stats.samples_lost_latency` は best-effort で受け取る `HISTORY_LATENCY` の分で、どのペアの実測も失わせないため警告には数えない。`stats.samples_lost_at_start` は reader がマッチする前の正常な分で、警告にはならない。 |
| `!shm-not-visible` | ノードが別の `/dev/shm` (別コンテナまたは別ホスト) を使っている。共有メモリの行はツールの環境だけを表す。 |
| `NONE` のペアに `!shm-ipc-namespace-split` | 2 つのノードのホスト id は同じだが `/dev/shm` が別 (ホストネットワークで IPC 名前空間が別) なので、Fast DDS が SHM や data-sharing を選んでも受信側には何も届かない。両方のコンテナに `ipc: host` を付けるか、片側の SHM (と data-sharing) を無効にする (`--advise` で対処を表示できる)。 |
| `!shm-stale-files` | クラッシュしたプロセスがセグメントを残している。`fastdds shm clean` で削除できる。 |
| 実際にはあるペアが表に出ない、または `warning: discovery was still in progress (...)` | ノードが endpoint を announce し終える前に観測が終わっている。観測は discovery イベントが `--quiet` 秒間ないと終わるが、大規模なシステムでは announce の合間にも無音になる。メッセージが示す値で取り直す (`--quiet 3 --timeout 10` から。`--stats` 付きなら `--quiet 3 --timeout 60`)。`--json` では `discovery.complete` が `false` になり、その文書は差分比較には使えない。 |
| ツール自身がノードとして出る | 出ないはずです。自身のノード `/_transport_viz_<pid>` と participant は除外されます。出た場合は `--json` 出力を添えて issue を立ててください。 |

次は判定ルールを知る [仕組み](how-it-works.ja.md)、ツールを変更するなら
[Architecture](architecture.md) (英語) へ。
