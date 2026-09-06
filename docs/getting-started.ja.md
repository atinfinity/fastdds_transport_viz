# はじめに

> 英語版が正です。この文書は 2026-09-06 時点の英語版に対応しています。

このページでは、素の ROS 2 環境から、最初の `ros2 transport list`、statistics、web viewer
までを通します。動作環境は Linux です。ツールは Fast DDS を観測するので、観測対象のノードは
`rmw_fastrtps_cpp` (Jazzy、Kilted、Rolling の既定 RMW) を使っている必要があります。

| ROS 2 ディストリビューション | Fast DDS | 備考 |
|---|---|---|
| Jazzy (Ubuntu 24.04) | 2.14 | 主対象 |
| Kilted (Ubuntu 24.04) | 3.2 | |
| Rolling | 3.x | ベストエフォート (CI は失敗を許容) |

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
source /opt/ros/jazzy/setup.bash              # または kilted / rolling
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
`ROS_DISTRO=kilted` / `rolling`)、`/ws` にマウントしたリポジトリ、ホストの共有メモリが
ツールから見えるようにする `ipc: host` を定義しています:

```
docker compose build
docker compose run --rm dev bash
colcon build --symlink-install && source install/setup.bash
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
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT  RATE  LATENCY  REASON
/chatter  std_msgs/msg/String  1     1     SHM x1     -     -        same-host-guid,datasharing-disabled-writer,both-shm-locators
    /talker@local -> /listener@local  SHM  -  -  same-host-guid,datasharing-disabled-writer,both-shm-locators

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
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC"
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --stats
```

ペアの行に `measured=SHM 47pkt 3.20 kB` が付き、`RATE` 列に payload のスループット、`LATENCY` 列に
そのペアの write-to-notification 遅延 (平均と最大) が出て、ホストは名前とプロセス id で表示されます。プロファイルファイルは statistics writer の
リソース制限を外すためのものです。トピックの説明と回避する落とし穴は
[statistics.ja.md](statistics.ja.md) を参照してください。

## 4. 見続ける

```
ros2 transport list --watch --stats --interval 2
```

2 秒ごとに再観測し、前のフレームからの変化に印を付けます (`+` 現れた、`~` 変わった、`-`
消えた)。キー: `q` 終了、`p` 一時停止、`v` ペア表示、`e` 凡例、`a` 全トピック。

## 5. ブラウザで見る

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1
# transport_viz_web: listening on http://127.0.0.1:8765/  (serving .../share/fastdds_transport_viz/web)
```

URL を開くと、ホストが列、ノードが箱、ペアが transport ごとに色分けされた矢印として
ライブで更新されます。`ros2 transport list --json > snapshot.json` で作った文書は、同じ
ページ (`web/index.html`) でオフラインでも開けます。[web-viewer.ja.md](web-viewer.ja.md) を
参照してください。

## 6. コマンドリファレンス

```
ros2 transport list [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--node REGEX]
                    [--all] [-v] [--explain] [--stats] [--json] [--color auto|always|never]
                    [--watch [--interval S]]
ros2 transport codes
```

`ros2 transport` は `fastdds_transport_viz` の `transport_viz` バイナリを exec します。
バイナリは `ros2 run fastdds_transport_viz transport_viz` で直接実行でき、同じオプションに
加えて `--list-codes` があります。終了コード: 成功 0、使い方の誤り 2、バイナリが見つからない・起動
できないときは `ros2 transport` が 1。

## うまくいかないときの最初の確認

| 症状 | 確認すること |
|---|---|
| `ros2: error: argument Call ... invalid choice: 'transport'` | このシェルで `source install/setup.bash` したか。`ros2transport` が同じワークスペースでビルドされているか。 |
| トピックが 1 つも出ない | ノードと同じ `ROS_DOMAIN_ID` か。`ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` は各 participant を自分だけに限定する。Discovery Server 使用時はツールにも同じ `ROS_DISCOVERY_SERVER` が必要 (自動で SUPER_CLIENT になる)。 |
| 別マシンのノードが出ない | 相手のマシンにマルチキャストで届くか、`ROS_STATIC_PEERS` に列挙するか、双方が Discovery Server を使う。[development.md](development.md#two-physical-hosts) (英語) 参照。 |
| `--stats` で `!stats-not-enabled-on-writer` | ノードが `FASTDDS_STATISTICS` 無しで起動された。変数はノードの起動前に設定する。 |
| `!shm-not-visible` | ノードが別の `/dev/shm` (別コンテナまたは別ホスト) を使っている。共有メモリの行はツールの環境だけを表す。 |
| `!shm-stale-files` | クラッシュしたプロセスがセグメントを残している。`fastdds shm clean` で削除できる。 |
| ツール自身がノードとして出る | 出ないはずです。自身のノード `/_transport_viz_<pid>` と participant は除外されます。出た場合は `--json` 出力を添えて issue を立ててください。 |

次は判定ルールを知る [仕組み](how-it-works.ja.md)、ツールを変更するなら
[Architecture](architecture.md) (英語) へ。
