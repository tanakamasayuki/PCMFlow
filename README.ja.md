# PCMFlow

> English: [README.md](README.md)

Arduino 向けの軽量オーディオデコード / PCM フローライブラリ。

音声入力をデコードし、指定された形式（8-bit unsigned / 16-bit signed、mono / stereo、任意サンプルレート）の整形済み PCM を内部リングバッファへ蓄積する。特定の出力デバイス・コーデック・ファイルシステム・ネットワークスタック・OS・RTOS タスク機構には依存しない。

詳細な仕様は [SPEC.ja.md](SPEC.ja.md) を参照。

---

## 対応コーデックと入力源

**同梱デコーダ** (`PCMFlow::open()` で自動検出 / 明示指定):

- WAV (PCM、8-bit unsigned / 16-bit signed、mono / stereo)
- MP3 ([dr_mp3](https://github.com/mackron/dr_libs))
- FLAC ([dr_flac](https://github.com/mackron/dr_libs))

**入力源** (`ByteStream` 抽象化により、好きな源から取れる):

- メモリ (PROGMEM / RAM) — `MemoryByteStream`
- ファイル (SD / LittleFS) — `FileByteStream`、または `PCMFlow::open(SD, path)`
- 任意の Arduino `Stream` (HTTP / Serial 等) — `StreamByteStream`
- 自作の `ByteStream` 派生クラス

**外部コーデック接続**: `PCMSource` 抽象基底を実装すれば、`setInputSource()` で組み込みデコーダをバイパスして任意のデコーダを差し込める。

---

## クイックスタート

### ファイル再生 (SD カード等)

```cpp
#include <PCMFlow.h>
#include <SD.h>

PCMFlow audio;

void setup() {
    Serial.begin(115200);
    SD.begin();

    audio.setOutputFormat({44100, 2, 16});
    audio.setGain(0.8f);
    audio.open(SD, "/song.mp3");           // codec 自動検出
}

void loop() {
    audio.pump();
    if (audio.availableFrames() >= 256) {
        int16_t buf[256 * 2];              // stereo / 16-bit
        audio.readFrames(buf, 256);
        // I2S / DAC / USB Audio へ送る
    }
}
```

### PROGMEM 上の MP3 を再生

```cpp
audio.setOutputFormat({44100, 2, 16});
audio.open(kEmbeddedMp3);                  // 配列 = サイズ不要
```

### 自前の `ByteStream` を渡す

```cpp
MemoryByteStream src(progmemBuf, progmemLen);
audio.setOutputFormat({44100, 2, 16});
audio.setInput(src);                       // PCMFlow は src を借りるだけ
```

---

## バッファサイズの目安

`setBufferFrames()` で整形済み PCM のリングバッファ容量を設定する。**単位は frame**（チャンネル数 × bit 深度を含んだ 1 サンプル組）。

### 換算表 (frame → ms)

```
ms = bufferFrames × 1000 / sampleRate
```

| `setBufferFrames()` | 22.05 kHz | 44.1 kHz | 48 kHz |
|--------------------|-----------|----------|--------|
| 256                | 11.6 ms   | 5.8 ms   | 5.3 ms |
| 512                | 23.2 ms   | 11.6 ms  | 10.7 ms|
| 1024               | 46.4 ms   | 23.2 ms  | 21.3 ms|
| **2048 (default)** | 92.9 ms   | **46.4 ms** | **42.7 ms** |
| 4096               | 185.8 ms  | 92.9 ms  | 85.3 ms|
| 8192               | 371.5 ms  | 185.8 ms | 170.7 ms|

### 用途別の推奨

| ユースケース | バッファ目安 | 44.1 kHz での frame |
|-------------|------------|---------------------|
| リアルタイム監視 / 楽器系 | 5〜10 ms | 256〜512 |
| ゲーム効果音 | 20〜50 ms | 1024〜2048 |
| ローカル音楽再生 | 50〜200 ms | 2048〜8192 |
| ネットワーク経由 (HTTP MP3 等) | 500〜2000 ms | 22050〜88200 |
| BGM 等の緩い再生 | 100〜500 ms | 4096〜22050 |

**基本はデフォルト 2048 frame で問題ない**。音が途切れる → 増やす、OOM → 減らす、というスタンスで OK。

### USB Audio など低レイテンシ用途の注意

USB Audio は 1 ms 周期で読みに来るが、**リングバッファを 1 ms 分ピッタリにすると音欠けする**。

理由: MP3 / FLAC のデコードは毎回完了するわけではなく、内部キャッシュが切れたタイミングで 1 frame デコード (ESP32 で数百 μs 〜 1 ms) が走る。その瞬間に消費側が読みに来ると間に合わない。

| 入力 | USB Audio 用最小バッファ |
|------|------------------------|
| 圧縮なし PCM (生 WAV / 直接 PCM) | 2〜3 ms (96〜144 frame @ 48 kHz) |
| MP3 / FLAC 等のデコード経由 | **最低 5 ms、推奨 10 ms** (240〜480 frame @ 48 kHz) |

ループ間隔やジッタを考えると、安全側で常に余裕を持つほうが楽。

---

## ターゲット環境

`architectures=*`（あらゆる Arduino プラットフォーム向け）として配布しているが、現実的には以下のリソースが必要:

- 32-bit MCU (`int` が 32-bit)
- SRAM 数十 KB 以上
- Flash 数十〜100 KB 程度（MP3 / FLAC を含めた場合）

**実用想定**: ESP32 / ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-P4 / RP2040 / RP2350 / Teensy 4.x / SAMD51 / STM32 F4 以上 / nRF52 など。

**非対応**: AVR (Uno / Mega / Nano) — メモリ・CPU 制約。SAMD21 など極小 SRAM 環境では MP3 / FLAC は不可（WAV のみ可能性あり）。

---

## 例

[examples/](examples/) に動作確認用スケッチ:

- **DecodeWavInfo** — 最小エンドツーエンド (PROGMEM WAV をデコードして Serial 出力)
- **PlayMp3** — PROGMEM MP3 を codec 自動検出で再生
- **ResampleAndConvert** — 22.05 kHz mono 16-bit → 44.1 kHz stereo 8-bit + gain

詳細は [examples/README.ja.md](examples/README.ja.md)。

---

## テスト

`tests/` 配下に pytest-embedded ベースの自動テスト一式（host + ESP32 ビルド対応）。

実行: `cd tests && uv run pytest`

詳細は [tests/README.ja.md](tests/README.ja.md)。

---

## ライセンス

PCMFlow 本体は MIT ライセンスで配布します。

### 同梱の外部デコーダへの謝辞

PCMFlow は MP3 / FLAC のデコードに、David Reid 氏（[mackron](https://github.com/mackron)）による [dr_libs](https://github.com/mackron/dr_libs) の `dr_mp3.h` / `dr_flac.h` を同梱しています。さらに `dr_mp3` は Lion 氏（[lieff](https://github.com/lieff)）による [minimp3](https://github.com/lieff/minimp3) を基礎としており、`dr_mp3.h` 自身が *"Based on minimp3 — which is where the real work was done."* と明記しています。

両ライブラリは Public Domain (Unlicense) または MIT-0 のデュアルライセンスで提供されており、PCMFlow の MIT ライセンスとも整合します。法的には帰属表示は不要ですが、こうした素晴らしい仕事を公開・継続メンテナンスしてくださっている作者の方々へ敬意を表し、本プロジェクトでは明示的にクレジットを残します。

ライセンス全文と詳細は [src/external/LICENSE_dr_libs.md](src/external/LICENSE_dr_libs.md) を参照ください。なお、デコーダ実装そのものに関する issue は PCMFlow ではなく上流（dr_libs / minimp3）に報告するのが適切です。
