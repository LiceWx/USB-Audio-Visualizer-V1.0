## USB Audio Visualizer V1.0

基于 STM32F446RCTx 的双 USB 外设，实现 USB 音频流的转发同时截获，计算瞬时音量（RMS）、Mel 频谱和实时 Onset 检测算法 SuperFlux，并用 OLED 屏幕和 WS2812 灯效呈现。

效果演示：

![show](https://img2024.cnblogs.com/blog/3231565/202606/3231565-20260611210156898-1074170287.gif)

其中上方的 LED Strip 的一个亮点代表一次 Onset（音符起始点），参考了部分 https://github.com/CPJKU/SuperFlux/tree/master 的方法。

硬件设计：https://oshwhub.com/ryths/project_zjpyvhhy

DevBlog：https://www.cnblogs.com/Ryths/p/20459807/usb-audio-visualizer-v1_0
