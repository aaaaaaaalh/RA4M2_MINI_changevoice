# VoiceLab v0.2 板卡采集接入

本版本把老师提供的 RA4M2MINI 工程补齐为麦克风采集固件，并在已有 MATLAB 界面中增加板卡串口输入。板卡只负责采集与传输；降噪、变声和播放仍在电脑完成。旧工程和第一版 MATLAB 请另存备份。

## 1 接线与工程

保持已确认的连接：AO→P001、G→GND、+→3.3V、DO不接。不要将 AO 接到 P109；P109 已用于板载 CH340E 的串口发送。用可传数据的 Type-C 线连接板卡与电脑。麦克风模块资料标注 5V，本版先测试它在当前 3.3V 供电下的实际信号，暂不改供电。声音弱时先测 ADC 波形，再决定是否需要更合适的模拟前端。

推荐先备份你本机已成功编译的工程，然后只复制以下四个文件到对应路径，覆盖三个文件并新增一个头文件：

- firmware/RA4M2MINI/src/hal_entry.c
- firmware/RA4M2MINI/src/adc/bsp_adc.c
- firmware/RA4M2MINI/src/debug_uart/bsp_debug_uart.c
- firmware/RA4M2MINI/src/audio_capture.h（新增）

其他文件不必覆盖。右键工程 Refresh，再 Clean、Build，按你原来成功的方法下载并运行。更换固件会替换板卡当前程序，原示例可以重新编译下载恢复。下载后要让 CPU 正常运行，不要停在断点；停机调试会中断采样。

包中也提供完整工程副本。如果要直接导入它，请用另一个 e² studio 工作空间，避免与现有同名 RA4M2MINI 工程冲突。使用你已经验证成功的 FSP 6.6.0、编译器与下载配置。本版未修改 configuration.xml、ra_gen 或时钟配置，也没有预编译可烧录固件。Debug/Release 旧构建产物已排除，必须重新编译。

板卡 OLED 此版本不刷新，原示例的 DAC、板端降噪与变声不启动。屏幕没有新画面不等于采集失败。

## 2 先测试串口和 ADC

关闭串口助手以及所有占用同一串口的程序。MATLAB 当前文件夹切换到本包 matlab 文件夹；如果旧 VoiceLab 正开着，先关闭旧窗口，再执行：

```matlab
clear app
clear classes
which VoiceLab -all
selftest_serial
serialportlist("available")
```

确认 `which` 指向本包中的 VoiceLab.m。根据设备管理器中板载 CH340 对应的 COM 号，运行（COM3 只是示例）：

```matlab
report = board_probe("COM3",5);
```

采集前约 2 秒保持安静，后面说话。该脚本不播放声音，只显示原始 ADC 和去直流波形，打印包数、丢包、同步异常和 ADC 范围。

先把输出 report 和两幅波形截图发回，确认采样幅度与传输稳定后再开启实时变声。

应关注：

- Lost 应尽量为 0，BadSync 不应持续增长。初次连接可能跳过半包字节，不算连续运行故障。
- ADC 不应长期卡在 0 或 4095；说话时波形应有可辨变化。静音中心不保证为 2048，本版将真实偏置保留给诊断工具。
- ADCMean、ADCMin、ADCMax、MaxBlockPeakToPeak 来自实际完整包；插入的补零帧不计入 ADC 统计。
- ClippedSamples 为接近量程边缘（≤4或≥4091）的累计样本数，用于提示可能削顶，不是精确失真测量。
- WallSeconds 与 AudioSeconds 只供粗略比较，包含启动同步和电脑调度，不是精确采样时钟校准。

## 3 使用完整界面

```matlab
app = VoiceLab;
```

点击“刷新设备”，在“输入设备”中选择“板卡串口 COMx”，输出选择电脑耳机。

1. 先选“录音后变声”，不启用降噪，录制几秒并试听原声。
2. 原声正常后选择音色，执行离线变声。
3. 最后试“实时变声”；使用耳机，避免扬声器声音回到麦克风。
4. 在板卡输入下，“学习环境噪声”也会从板卡采集，保持安静约 1 秒即可。

“默认”或普通音频设备名称仍是电脑麦克风。运行期间参数保持锁定，需要停止后调整。变声算法沿用你已验证效果良好的第一版。

板卡输入会先通过有状态的 DC blocker，消除实际静音偏置。界面里的原声录音因此是去直流之后的语音；board_probe 第一幅图才是未经去直流的 ADC 计数。DC blocker 不是语音降噪开关的一部分。

## 4 协议与实现

16 kHz、单声道、12-bit ADC、256 样本一帧，16 ms 一帧。

| 字节偏移（从0开始） | 内容 |
|---|---|
| 0～1 | AA 55 |
| 2～3 | 00 02，即小端长度512 |
| 4～5 | 16-bit小端序号，65535后回到0 |
| 6～517 | 256个小端有符号PCM16 |

PCM = (ADC - 2048) × 16；传输速率921600，8-N-1，无流控。生成配置的实际波特率有约1.725%误差，沿用老师工程，是否稳定须实测；出现持续同步异常时再调整波特率调制配置。

GPT0 溢出通过 ELC 触发 ADC0；ADC 完成中断只读取通道1并写入环形块缓冲。前台复制完整块、编码并异步发送。4个块缓冲，满时整块丢弃，序号继续推进，让主机能检测缺口。发送中的 UART 缓冲直到 TX_COMPLETE 都不修改。串口写入失败也保留序号缺口。

MATLAB 支持碎片化读入、从半包同步、长度检查、序号回绕、小范围丢包补零。初次同步或失步后须观察相距518字节的两个有效包头。重复包丢弃；超过256个缺包的跳变明确报错，要求重新开始。电脑处理跟不上、积压超过约5秒时停止并提示，不无限累计延迟。界面发生串口错误时会保留已采集的完整音频块。

此教学协议没有 CRC，不能检测所有载荷位错误；BadSync 只统计已锁定时的失步事件。丢包补零维持时间轴，但不能恢复丢失声音。板卡和声卡存在独立时钟，超长时间运行可能积压或欠载；本版仍限制单次5分钟，不含异步采样率漂移补偿。

调试器可观察 g_capture_samples、g_capture_last_adc、g_capture_dropped_blocks、g_capture_adc_errors、g_capture_uart_errors、g_capture_sent_packets。初始化失败停在 Capture_Check，查看 g_capture_error；不要在故障时让未初始化的外设继续运行。

## 5 验证记录与限制

开发环境已执行：

- C 主机测试（模拟FSP接口）：完整块发布、满队列整块丢弃、16-bit序号回绕、PCM两端点编码、异步发送缓冲所有权、写入失败后序号缺口。
- 测试使用 GCC 严格警告及地址/未定义行为检查；环境不支持泄漏检查，运行时关闭该项。
- MATLAB 文件通过 MISS_HIT 语法/静态检查。

未执行：Renesas目标交叉编译、实际板卡运行、MATLAB selftest_serial 与音频设备测试。这里没有你的 e² studio、MATLAB 或物理板卡，不能把主机逻辑测试等同于硬件验收。tests/host 中保留可复现C测试；matlab/selftest_serial.m 用于你本机检查解析器。

如果编译失败，请发第一条 error 及上下文；若5秒内没有完整包，请发 COM 号、board_probe 报错，并确认程序已运行。若有数据但声音弱，先看原始ADC摆幅和静音偏置，不急着把输出增益拉满或改供电。

本次不包含板端变声、OLED频谱展示或人物音色模型训练。

参考：老师提供的RA4M2MINI工程及板卡原理图；MATLAB串口API：
https://www.mathworks.com/help/matlab/ref/serialport.html
https://www.mathworks.com/help/matlab/ref/serialport.read.html
