# MATLAB AI 参考音色变声（v0.3.0）

## 启动

1. 保持已经验证成功的 Seed-VC 后台运行。如未运行，在 PowerShell 执行：

```powershell
Set-Location "$env:USERPROFILE\VoiceLabAI\seed-vc"
& "$env:USERPROFILE\VoiceLabAI\.venv\Scripts\python.exe" app_vc.py --fp16 True
```

看到 `http://127.0.0.1:7860` 后即可，不必打开网页。端口必须是 7860，请勿重复启动多个后台。保持 FFmpeg 在 PATH 中，不要重新安装现有依赖。

2. 完整解压项目，在 MATLAB 中将当前文件夹切换到本项目 `matlab` 文件夹，执行：

```matlab
app = VoiceLab;
```

点击顶部「AI 参考音色变声」。也可以单独执行 `ai = VoiceLabAI;`。

3. 「导入原声」或「开始麦克风录音」→「停止录音」。AI 窗口使用 Windows 默认麦克风，48 kHz 单声道，最长 5 分钟。主界面已有录音也会带入 AI 窗口；主界面仍为 16 kHz。
4. 选择已保存的唐三参考 WAV/FLAC。推荐 10～20 秒；超过 25 秒模型只取前 25 秒。
5. 保持扩散步数 25、CFG 0.7，点击「执行 AI 变声」。长度参数固定为 1.0。
6. 播放原声/变声结果，导出 WAV。录音原声可以直接导出保存，方便后续对比。

AI 窗口保留导入音频采样率，输出保留模型采样率，导出 24-bit WAV。只修正最多 0.1 秒的模型帧取整差异（尾部裁切/补零），不拉伸语速。超出范围时报告错误，避免隐藏截断。

## 范围与排查

- 新入口集成已验证的离线参考音色转换。原有实时 DSP、离线 DSP、板卡串口功能保留；AI 尚不是实时麦克风监听。
- 只连接本机 127.0.0.1:7860；录音通过现有 Gradio API 交给本机模型。后台 Gradio 可能保留自身缓存，桥接临时目录会在任务结束后清理。
- 取消会尝试取消 Gradio 任务并停止等待；已开始的 GPU 计算不保证立即中断。不要立即连续提交多次。
- 默认 Python 路径为当前 Windows 用户目录下 `VoiceLabAI\.venv\Scripts\python.exe`，可在界面修改。
- 连接失败：检查 PowerShell 后台是否还运行以及端口；模型报错查看该窗口的 traceback。
- 不改动 Seed-VC 源码、模型或现有 PyTorch GPU 环境。需要已安装的 gradio-client 1.8.0、numpy、soundfile（此前环境已具备）。
- 配乐/混响未分离，本次接入不会自动去除参考素材中的空旷感。

## 验证情况

开发环境做了 Python 桥接模拟测试和源码检查；未运行 Windows MATLAB R2025b、RTX 4060 或真实 Seed-VC 模型。首次使用请用已验证的同一份原声与参考素材比较，确认音色、时长、播放和导出。

## 来源

Seed-VC：https://github.com/Plachtaa/seed-vc

按用户已部署提交 `51383efd921027683c89e5348211d93ff12ac2a8` 的 `app_vc.py` 接口实现连接，五个参数为 source、target、diffusion_steps、length_adjust、inference_cfg_rate，读取第二个（完整 WAV）输出。本项目只包含接口桥接，不包含 Seed-VC 源码、权重或角色录音。Seed-VC 及相关模型适用各自许可证。
