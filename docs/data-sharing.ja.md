# Data-sharing (zero-copy) 配送

> 英語版が正です。この文書は 2026-09-05 時点の英語版に対応しています。

writer と reader が同じホストにあり、型が bounded (サイズ上限あり) なら、Fast DDS はすべての
transport を迂回できます。reader が writer の履歴を直接マップする data-sharing 配送です。
ツールはこれを `SHM` transport とは区別して `DATA_SHARING` と表示します。

## ROS 2 ノードが広告するもの

`rmw_fastrtps_cpp` は既定ですべてのエンドポイントに data-sharing `OFF` を広告するため、ROS 2 の
トピックは通常 `SHM` になり、`DATA_SHARING` にはなりません。Fast DDS は `AUTO` を型の boundedness
と履歴メモリポリシーに基づいて discovery の *前* に解決するので、ツールが広告 QoS で見るのは
実効的な設定です (`std_msgs/String` の writer は `AUTO` を指定しても `OFF` を広告します)。

bounded な型 (例: `std_msgs/Int32`) で有効にするには:

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/datasharing_auto.xml
export RMW_FASTRTPS_USE_QOS_FROM_XML=1
ros2 run fastdds_transport_viz bounded_pub &
ros2 run fastdds_transport_viz bounded_sub &
ros2 transport list -v                              # /bounded -> DATA_SHARING? (likely)
```

既定プロファイルには `ON` ではなく `AUTO` を使ってください。`ON` だと `/rosout` のような
unbounded な型で writer の作成が失敗します。

## 確信度

両エンドポイントが data-sharing を広告し、domain id に共通部分があれば、判定は `likely`
(`DATA_SHARING?`) です。`--stats` を付けると次の 2 通りで `certain` になります。

- `HISTORY_LATENCY` が配送を証明し、かつ reader の locator にパケットが 1 つも届いていない
  (`datasharing-confirmed-no-traffic`)。
- `HISTORY_LATENCY` が配送を証明し、かつ観測中に writer の `DATA_COUNT` が増えていない
  (`datasharing-confirmed-no-data-submessages`)。reliable な data-sharing エンドポイントは SHM 上で
  heartbeat を交換し続けますが、zero-copy 配送は DATA サブメッセージを一切生成しないので、この
  カウンタで両者を区別できます。観測対象ノードの `FASTDDS_STATISTICS` に `DATA_COUNT_TOPIC` が
  必要です。

  ```
  export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/datasharing_auto_stats.xml
  export RMW_FASTRTPS_USE_QOS_FROM_XML=1
  export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC"
  ros2 run fastdds_transport_viz bounded_pub &
  ros2 run fastdds_transport_viz bounded_sub &
  ros2 transport list -v --stats                      # /bounded -> DATA_SHARING (certain)
  ```

writer が data-sharing でない reader も相手にしている場合、その `DATA_COUNT` には両方の経路が
混ざるので判定は `likely` のままです (`datasharing-ambiguous-mixed-readers`)。すべての reader が
data-sharing なのにカウンタが増える場合、Fast DDS は zero-copy を使っていません。判定は実測された
transport に変わり、警告 `datasharing-not-used` が付きます。`DATA_COUNT_TOPIC` が無い場合、リンク上
のトラフィックがあると判定は `likely` のままです (`datasharing-ambiguous-participant-traffic`)。
JSON の `measured` オブジェクトには `data_submessages` (`DATA_COUNT` の増分。取得できなければ
`null`) と `delivered_samples` が入ります。

## 履歴のサイズ

data-sharing の writer は履歴を `/dev/shm` の `fast_datasharing_<writer の GUID>` というファイルに
置きます。ツールからそのファイルが見える (同じ IPC 名前空間) 場合、そのサイズを writer に紐付けて
JSON の `datasharing_history_bytes` と web viewer のエンドポイント詳細に出します。環境の共有メモリの
行はこうした履歴をすべて数えます ([how-it-works.md](how-it-works.md#環境の共有メモリ) 参照)。
Fast DDS は writer が kill されてもこのファイルを消さないので、終了した writer の履歴は
`fastdds shm clean` を実行するまで *unmatched* として現れます。
