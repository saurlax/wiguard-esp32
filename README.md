# WiGuard ESP32

[WiGuard](https://github.com/SugarLab-DLUT/wiguard) csi data collector on ESP32.

## Data format

| 字段                  | 释义                                                   | 用途                                                      |
| --------------------- | ------------------------------------------------------ | --------------------------------------------------------- |
| **type**              | 数据类型                                               | 标识数据包的类型，只会是 CSI_DATA                         |
| **seq**               | 序列号                                                 | 用于跟踪数据包的顺序，检测丢包和重传                      |
| **mac**               | MAC 地址                                               | 标识发送或接收数据包的设备                                |
| **rssi**              | 接收信号强度指示（Received Signal Strength Indicator） | 衡量信号强度，通常用于评估信号质量                        |
| **rate**              | 数据传输速率                                           | 指示数据包的传输速率，影响数据传输的效率和速度            |
| **sig_mode**          | 信号模式                                               | 指示信号的模式（例如，11b、11g、11n）                     |
| **mcs**               | 调制和编码方案（Modulation and Coding Scheme）         | 指示使用的调制和编码方案，影响数据传输速率和可靠性        |
| **bandwidth**         | 带宽                                                   | 指示信道带宽（例如，20MHz、40MHz）                        |
| **smoothing**         | 平滑                                                   | 指示是否启用信道平滑                                      |
| **not_sounding**      | 非探测                                                 | 指示数据包是否为探测包                                    |
| **aggregation**       | 聚合                                                   | 指示是否启用数据包聚合                                    |
| **stbc**              | 空时分组编码（Space-Time Block Coding）                | 指示是否启用 STBC                                         |
| **fec_coding**        | 前向纠错编码（Forward Error Correction Coding）        | 指示是否启用 FEC 编码                                     |
| **sgi**               | 短保护间隔（Short Guard Interval）                     | 指示是否启用 SGI                                          |
| **noise_floor**       | 噪声底限                                               | 测量环境噪声水平                                          |
| **ampdu_cnt**         | AMPDU 计数                                             | 指示 AMPDU（Aggregated MAC Protocol Data Unit） 的计数    |
| **channel**           | 信道                                                   | 指示当前使用的 WiFi 信道                                  |
| **secondary_channel** | 次信道                                                 | 指示次信道的位置（例如，上行或下行）                      |
| **local_timestamp**   | 本地时间戳                                             | 记录数据包接收的本地时间戳                                |
| **ant**               | 天线编号                                               | 指示使用的天线编号                                        |
| **sig_len**           | 信号长度                                               | 指示数据包的信号长度                                      |
| **rx_state**          | 接收状态                                               | 指示接收数据包的状态                                      |
| **len**               | 数据长度                                               | 指示 CSI 数据的长度                                       |
| **first_word**        | 第一个字                                               | CSI 数据的第一个字                                        |
| **data**              | 数据                                                   | 实际的 CSI 数据，包含信道状态信息，用于分析信道特性和性能 |
