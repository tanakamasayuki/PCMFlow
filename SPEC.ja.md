# PCMFlow 仕様書

> English: [SPEC.md](SPEC.md)

## 1. 概要

**PCMFlow** は、Arduino 環境向けの軽量オーディオデコード / PCMフローライブラリである。

音声データ（圧縮データまたは生PCM）を入力として受け取り、利用者が指定した形式のPCMデータを逐次生成・整形し、内部リングバッファへ蓄積する。利用者は `frame` 単位で整形済みPCMを取り出せる。

本ライブラリは、特定の再生デバイス、ファイルシステム、ネットワーク、OS、RTOS、タスク機構には依存しない。Arduino Core API（`Arduino.h`、`Stream`、`Print` など）への依存は許容する。

### サポート対象プラットフォーム

本ライブラリは Arduino プラットフォーム全般を対象とする（`library.properties` の `architectures=*` を維持）。

ただし以下のリソース前提を満たす環境を想定する:

- SRAM: 数十 KB 以上（PCM リングバッファ + デコーダ作業領域）
- Flash: MP3 / FLAC デコーダを含めると数十〜100 KB 程度
- 32-bit MCU（`int` が 32-bit）

このため **AVR 系（Arduino Uno / Mega / Nano など）はメモリ・CPU 制約から非対応**。SAMD21 など極小 SRAM 環境も MP3 / FLAC は実質不可（WAV 8-bit mono 程度なら可能性あり）。

実用想定（一例）: ESP32 / ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-P4 / RP2040 / RP2350 / Teensy 4.x / SAMD51 / STM32 F4 以上 / nRF52 など。

MP3 / FLAC デコードは ESP32 クラス以上のリソースを前提とする。自動テストは [tests/](./tests/) 配下で host + ESP32 のみを対象とし、それ以外のターゲットでのビルド確認は [examples/](./examples/) のスケッチで担保する（[tests/TEST_PLAN.ja.md](./tests/TEST_PLAN.ja.md) 参照）。

---

## 2. 目的と基本コンセプト

本ライブラリの主責務は **コーデック集** ではなく、**PCMフローの統一的な取り扱い** である。

データの流れは以下のとおり:

```text
ByteStream
  ↓
Decoder
  ↓
PCM Format Conversion (rate / channel / bit-depth / gain)
  ↓
PCM Ring Buffer
  ↓
readFrames()
```

利用者は入力ソースと希望する出力PCM形式を設定し、`pump()` を定期的に呼び出すことで、整形済みPCMを取得できる。

### 価値

特定デバイスや特定コーデックに依存しない、軽量で組み合わせやすいPCMフロー基盤を提供する。

---

## 3. 想定する利用モデル

```cpp
#include <PCMFlow.h>

PCMFlow audio;

void setup() {
    audio.setInput(source);                // ByteStream など

    audio.setOutputFormat({
        .sampleRate    = 44100,
        .channels      = 2,
        .bitsPerSample = 16
    });

    audio.setGain(0.8f);
    audio.begin();
}

void loop() {
    audio.pump();

    if (audio.availableFrames() >= 256) {
        int16_t out[256 * 2];              // stereo / 16-bit
        audio.readFrames(out, 256);
        // out を I2S / DAC / バッファなどへ渡す
    }
}
```

---

## 4. 用語

| 用語           | 定義                                                                          |
| -------------- | ----------------------------------------------------------------------------- |
| `sample`       | 単一チャンネルの1値                                                            |
| `frame`        | 全チャンネル分の `sample` の組（例: stereoでは L+R の2 sample = 1 frame）       |
| `interleaved`  | チャンネルがフレーム単位で交互配置される形式（`L R L R ...`）                  |
| `PCMFormat`    | 出力PCMの形式（sampleRate / channels / bitsPerSample）                          |
| `ByteStream`   | バイト単位の入力ソース抽象                                                     |
| `ByteSink`     | バイト単位の出力先抽象                                                         |
| `PCMSource`    | 整形済みPCMを供給する抽象                                                      |
| `PCMSink`      | 整形済みPCMを受け取る抽象                                                      |
| `pump()`       | 入力読み込み・デコード・整形・バッファ蓄積を進める明示的処理関数                |

PCMの処理単位は **常に `frame`** とする。APIに現れる数量はすべて `frame` 数を単位とする。

---

## 5. 入力要件

入力は抽象化された `ByteStream` として扱う。

### 初期ユースケース

- メモリ上のデータ（PROGMEM / RAM）
- ファイル（SD / LittleFS など、Arduinoの `Stream` 経由）
- 逐次入力ストリーム

### 入力種別

入力ソースは以下の両方を考慮する。

- seek可能な入力
- seek不可の入力

### 責務外

ネットワーク通信は本ライブラリの責務に **含めない**。

HTTP、ICY、TLS、再接続、タイムアウト、ネットワークバッファリングなどは別モジュールの責務とする。ただし、ネットワーク入力を `ByteStream` へ変換したものは、本ライブラリの入力として扱える。

### Arduino Stream 連携

Arduino の `Stream` クラス（`File`、`WiFiClient` など）を `ByteStream` へ橋渡しするadapterを標準提供する。

---

## 6. デコード要件

- デコードは **逐次処理** を基本とする。
- 入力データ全体を事前にPCMへ展開することは前提としない。
- MP3 frame、AAC frame、Ogg page、FLAC block などコーデック固有の処理単位は、ライブラリ内部またはコーデックadapter内部で扱う。
- 利用者は圧縮データのフレーム構造を意識しない。

---

## 7. PCM出力要件

利用者は希望するPCM出力形式を `PCMFormat` で設定する。

### 初期対応範囲

| 項目             | 対応範囲                                              |
| ---------------- | ----------------------------------------------------- |
| チャンネル       | mono (1ch) / stereo (2ch)                              |
| ビット深度       | **unsigned 8-bit** / **signed 16-bit**                 |
| エンディアン     | little endian                                          |
| サンプルレート   | 8 kHz 〜 48 kHz 程度                                   |
| 配置             | interleaved                                            |

### ビット深度の扱い

| bitsPerSample | データ型    | 値域         | 用途例                                  |
| ------------- | ----------- | ------------ | --------------------------------------- |
| 8             | `uint8_t`   | 0〜255       | ESP32内蔵DAC、低メモリ環境              |
| 16            | `int16_t`   | -32768〜32767| I2S DAC、一般的なPCM出力                |

8-bit は **unsigned**（中心値 128）、16-bit は **signed**（中心値 0）とする。これは ESP32 内蔵DACのフォーマットおよびWAVの慣例に合わせるためである。

### interleaved 配置

```text
stereo (16-bit):
[L0_lo L0_hi R0_lo R0_hi] [L1_lo L1_hi R1_lo R1_hi] ...

stereo (8-bit):
[L0 R0] [L1 R1] ...

mono:
[M0] [M1] [M2] ...
```

### 1 frame のバイト数

```text
bytesPerFrame = channels * (bitsPerSample / 8)
```

---

## 8. PCM整形要件

本ライブラリは、出力先へPCMを渡しやすくするため、最低限のPCM整形を責務に含める。

### 対象

- サンプルレート変換（リサンプリング）
- mono / stereo 変換（チャンネル数変換）
- bit depth 変換（8-bit ⇔ 16-bit、unsigned ⇔ signed 含む）
- 音量 / gain 調整
- mute
- clipping / saturation（オーバーフロー時の飽和処理）

### 方針

これらは演出的な音声加工ではなく、**出力フォーマットへ合わせるための基本変換** として扱う。EQ / reverb / compressor / mixer などのエフェクトは責務外。

---

## 9. バッファリング要件

ライブラリは整形済みPCMを内部リングバッファへ蓄積する。

- リングバッファに入るPCMは、設定された出力形式へ変換済みであることを保証する。
- I2SやDACなどのリアルタイム出力では、出力要求が来てからデコードすると間に合わない可能性があるため、事前に整形済みPCMをバッファへ蓄積できること。
- バッファサイズは利用者が設定可能であること。

---

## 10. pump 要件

バッファ補充は `pump()` という明示的な処理関数によって行う。

`pump()` は可能な範囲で以下を進める:

1. 入力データ読み込み
2. デコード
3. PCM整形（rate / channel / bit-depth / gain）
4. 内部リングバッファへの蓄積

### 制約

- 本ライブラリはOSやRTOSのタスク機構に依存しない。
- `pump()` は呼び出し側のコンテキストで実行される（`loop()` 内、別タスク内、いずれも可）。
- FreeRTOS task などによる自動 pump は、コア機能ではなく **プラットフォーム別の補助モジュール** として扱う。
- `pump()` は無限にブロックしてはならない。入力やバッファが進められない状況では速やかに戻る。

---

## 11. データ取得要件

### `availableFrames()`

現在取得可能な整形済みPCM量を `frame` 単位で返す。

### `readFrames(out, frameCount)`

内部リングバッファから整形済みPCMを取得し、利用者が指定した出力バッファへコピーする。

- `out` の型は設定された `bitsPerSample` に対応する（8-bit なら `uint8_t*`、16-bit なら `int16_t*`）。
- `out` のサイズは `frameCount * channels * (bitsPerSample / 8)` バイト以上であること。
- 戻り値は実際に取得できた `frame` 数（要求未満になり得る）。

### 公開ポリシー

- 標準APIではリングバッファ内部のメモリを直接公開しない。
- zero-copy API は将来的な optional 機能とする。

---

## 12. コーデック方針

本ライブラリの主責務は、特定コーデックの実装ではなく、PCMフローの統一的な取り扱いである。コーデック実装は標準同梱または外部adapterとして扱う。

### ライセンス方針

- コアライブラリは **MITライセンス** で提供する。
- 標準同梱するコーデック実装は、MITと整合可能なライセンスを持つものに限定する。
- GPL系などライセンス上分離が望ましいコーデックは、外部adapterによる連携対象とする。

### 標準同梱候補

- WAV reader
- MP3 decoder
- FLAC decoder

### 外部 adapter 候補

- Opus decoder / encoder
- Vorbis decoder
- AAC decoder
- ESP8266Audio 連携
- platform decoder（ESP32 hardware など）
- その他 hardware decoder

---

## 13. 外部コーデック連携要件

外部デコーダは `PCMSource` interface を実装することで、PCM pipeline に接続できる。
外部エンコーダまたは WAV writer は `PCMSink` または `ByteSink` interface を実装することで、PCM pipeline から出力を受け取れる。

コーデック固有の処理、ライセンス、依存関係、内部バッファは **adapter 側の責務** とする。

コアは以下の抽象インターフェースのみを扱う:

```text
ByteStream
ByteSink
PCMSource
PCMSink
PCMFormat
```

---

## 14. WAV 出力（optional）

WAV 出力は、高圧縮コーデックのエンコードではなく、**PCM保存用コンテナ出力** として扱う。

オプション機能として、整形済みPCMを WAV 形式で `ByteSink` へ書き出せることを検討する。

### 用途

- デコード結果の保存
- テスト用 golden file 生成
- 録音データ保存
- 波形解析
- デバッグ

WAV writer はコアの必須機能ではなく、**optional module** として扱う。

---

## 15. 出力デバイスとの関係

本ライブラリは特定の出力デバイスを直接制御しない。

### 初期接続想定

- I2S DAC / I2Sアンプ（16-bit）
- 内蔵DAC / アナログDAC（ESP32など、8-bit）
- USB Audio DAC
- メモリバッファ
- WAV 保存
- 波形解析 / FFT / 可視化

本ライブラリは、これらへ渡しやすい整形済みPCMを生成することを目的とする。

---

## 16. メモリ要件

ESP32 / ESP8266 / AVR など、メモリ制約のある環境を考慮する。

### 利用者が設定可能な項目

- 入力バッファサイズ
- デコード一時バッファサイズ
- PCMリングバッファサイズ
- 最大使用メモリ量（任意上限）

### 方針

- ヒープ確保は `begin()` で集中させる。`pump()` / `readFrames()` 経路では原則として動的確保しない。
- 8-bit 出力モードでは、リングバッファ使用量が 16-bit モードの半分になる。

---

## 17. Arduino 統合方針

- Arduino Core API への依存は許容する（`Arduino.h`、`Stream`、`Print`、`millis()` 等）。
- 標準的な Arduino ライブラリレイアウト（`library.properties`、`src/`、`examples/`）に従う。
- Arduino の `Stream` を `ByteStream` として扱う adapter を標準提供する。
- `File`（SD / LittleFS）、`WiFiClient` などは `Stream` を継承するため、上記 adapter 経由で接続可能。

---

## 18. 非目標

本ライブラリは以下を目的としない。

- 音楽プレイヤーアプリ
- ネットラジオクライアント
- HTTPクライアント
- Bluetooth 制御
- I2S ドライバ
- USB Audio ドライバ
- GUI
- playlist 管理
- mixer
- EQ
- reverb
- compressor
- limiter
- DAW 機能

---

## 19. 設計方針

- 小さく保つ
- 入出力非依存（特定デバイスに縛らない）
- タスク非依存（OS / RTOS に縛らない）
- 逐次処理（事前一括展開しない）
- 整形済みPCMを保証（リングバッファ内は必ず出力フォーマット）
- コーデック非依存（コアにコーデックを内包しない）
- MIT core を維持する
- Arduino Core API への依存は許容する

---

## 20. コアAPI 概念

### 必須概念

```text
ByteStream        // 入力抽象
PCMFormat         // 出力PCM形式
PCMFlow           // パイプライン本体
  - setInput()
  - setOutputFormat()
  - setGain()
  - begin()
  - pump()
  - availableFrames()
  - readFrames()
```

### 将来的な拡張概念

```text
ByteSink
PCMSource
PCMSink
WavWriter
CodecAdapter
```

---

## 21. まとめ

本ライブラリは、音声入力を逐次デコードし、指定された形式の整形済みPCMを内部リングバッファへ蓄積し、利用者が `frame` 単位で取得できるようにする。

中心となる利用モデルは以下である。

```text
setInput()
setOutputFormat()   // 8-bit unsigned / 16-bit signed
begin()
pump()
availableFrames()
readFrames()
```

本ライブラリの価値は、特定デバイスや特定コーデックに依存しない、軽量で組み合わせやすい **PCMフロー基盤** を Arduino エコシステムへ提供することである。
