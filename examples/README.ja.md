# PCMFlow サンプル

> English: [README.md](README.md)

`PCMFlow` API のデモスケッチ集。いずれも特定のオーディオデバイスには出力しない — `loop()` 内の `TODO` コメント箇所が、I2S / DAC / USB Audio などの出力先へ PCM を渡す位置になる。

| サンプル | 内容 |
|---------|------|
| [DecodeWavInfo](DecodeWavInfo/) | 最小エンドツーエンド: flash 上の手書き PCM WAV を `PCMFlow` でデコードし、フォーマットとピーク値を Serial へ出力。 |
| [PlayMp3](PlayMp3/) | flash 上の MP3 → codec 自動検出 → チャンネル up-mix + リサンプル + gain → PCM 取得。 |
| [ResampleAndConvert](ResampleAndConvert/) | ランタイムに 22.05 kHz mono 16-bit の WAV を生成し、44.1 kHz stereo 8-bit（ESP32 内蔵 DAC 向け）+ gain 付きで変換。 |

## 埋め込みフィクスチャの再生成

`PlayMp3` の `embedded_mp3.h` はテスト側ツールで生成している。再生成するには（`ffmpeg` 必須）:

```sh
uv run --directory tests python tools/gen_test_audio.py
```

他 2 サンプルは入力データを実行時に生成するので、外部素材は不要。

## ビルド対象

各サンプルの `sketch.yaml` に検証済みプロファイル（`host` / `esp32` / `esp32s3` など）が宣言されている。Arduino CLI でビルド:

```sh
cd examples/DecodeWavInfo
arduino-cli compile --profile esp32 .
```
