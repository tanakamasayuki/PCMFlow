# テスト

> English: [README.md](README.md)

PCMFlow の自動テストを置くディレクトリ。

[pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) と Arduino CLI バックエンドを使い、ホスト（`lang-ship:host`）または実機（ESP32）でスケッチをビルド・実行する。

## テスト方針

PCMFlow はデバイス・コーデック・タスク非依存の **PCMフロー（純粋なデータ変換）** ライブラリのため、テストは **すべて自動テスト** で構成する。

PCMFlow の責務は「バイト列入力 → 整形済みPCMバイト列」までで完結し、出力先デバイスの制御は責務外（[SPEC.ja.md](../SPEC.ja.md) §18）。したがって正しさはすべて数値アサーションで検証できる。

- デコード結果 → golden PCM ファイルとのバイト比較
- 変換ロジック（bit depth / channel / rate / gain）→ 入出力サンプルの数値比較
- バッファ挙動 → `availableFrames()` / `readFrames()` の状態検証
- メモリ使用量 → 実機ターゲットでのフットプリント計測

実機 DAC での音出し確認や、複数ターゲット向けのビルドチェックはテスト対象外とする。それらは [examples/](../examples/) のスケッチをそれぞれの実機で動かして確認する（PCMFlow の自動テストではなく、利用者側・メンテナ側の統合確認）。

入力はすべてプログラムで生成、または固定のテスト用音源ファイルを使用する。期待される出力はすべてアサーションで検証する。

## 対象環境

自動テストは **負担が少なく自動化しやすい環境** に絞る。

| 環境 | プロファイル | 用途 |
|------|--------------|------|
| host | `lang-ship:host` | ロジック検証（メモリほぼ無制約、ファイル操作可、CI で高速） |
| ESP32 | `esp32:esp32:esp32` | 実機ビルド確認、フットプリント計測、Xtensa での挙動確認 |

ESP32-S3 やその他のターゲット（C3 / C6 / P4 / RP2040 など）でのビルドチェックは [examples/](../examples/) 配下のスケッチで担保する。

### host プロファイルの注意点

- Arduino Core API は動作するため、ロジック検証はほぼそのまま書ける
- ファイル操作も可能
- **メモリがほぼ無制約**で動いてしまうため、リングバッファや作業領域のサイズ制約は ESP32 想定の上限値を assertion で明示的に検証する

## ディレクトリ構成

各サブディレクトリが 1 つのテスト対象機能に対応する。

- `smoke/` — 雛形検証用のスモークテスト。ホスト上でビルド・実行できる最小スケッチ。テスト基盤の動作確認用。
- `ringbuffer/` — `PCMRingBuffer` の単体テスト。
- `convert/` — `PCMConvert`（bit depth / channel / gain）の単体テスト。
- 以降、機能ごとにディレクトリを追加していく。

## カバレッジ一覧

| 機能 | host 自動 | ESP32 自動 | 未カバー |
|------|-----------|-----------|---------|
| ライブラリのビルド | ✅ smoke | | ⬜ (ESP32) |
| `PCMFormat` 設定 | ✅ ringbuffer | | |
| リングバッファ書き込み / 読み出し | ✅ ringbuffer | | |
| `availableFrames()` / `readFrames()` | ✅ ringbuffer | | |
| bit depth 変換（8-bit ⇔ 16-bit） | ✅ convert | | |
| signed ⇔ unsigned 変換 | ✅ convert | | |
| mono ⇔ stereo 変換 | ✅ convert | | |
| gain / mute / clipping | ✅ convert | | |
| サンプルレート変換 | | | ⬜ |
| WAV reader | | | ⬜ |
| MP3 decoder | | | ⬜ |
| FLAC decoder | | | ⬜ |
| WAV writer（optional） | | | ⬜ |
| Arduino `Stream` adapter | | | ⬜ |
| メモリフットプリント計測 | — | | ⬜ |

---

## 必要なもの

- [uv](https://docs.astral.sh/uv/) — Python パッケージ・環境マネージャー
- [Arduino CLI](https://arduino.github.io/arduino-cli/) — pytest-embedded がビルド・フラッシュに内部で使用
- 実機テストを行う場合は、対象ボードを USB 接続しておくこと

## セットアップ

サンプル環境ファイルをコピーして編集する:

```sh
cp .env.example .env
```

各 `TEST_SERIAL_PORT_*` を対応するボードの実シリアルポートに合わせる。デフォルトの `host` プロファイルはソケット経由で動作するためシリアルポートは不要。

## テストの実行

`tests/` ディレクトリから:

```sh
# すべて実行（デフォルトは host プロファイル）
uv run --env-file .env pytest

# 特定のテストのみ
uv run --env-file .env pytest ringbuffer/

# 実機ESP32で実行
uv run --env-file .env pytest ringbuffer/ --profile=esp32
```

前回失敗したテストのみ再実行:

```sh
uv run --env-file .env pytest --lf
```

HTML レポート:

```sh
uv run --env-file .env pytest --html=report.html --self-contained-html
```

## pytest-embedded-arduino-cli

[pytest-embedded-arduino-cli](https://github.com/tanakamasayuki/pytest-embedded-arduino-cli) が pytest-embedded と Arduino CLI を接続し、各テスト実行前にスケッチを自動ビルド・フラッシュする。

### シリアルポート解決順序

1. `--port` CLI オプション
2. `TEST_SERIAL_PORT_<PROFILE>` 環境変数（`<PROFILE>` は sketch.yaml のプロファイル名を**大文字化してハイフンをアンダースコアに変換**）
3. `TEST_SERIAL_PORT` 環境変数（フォールバック）

### sketch.yaml

各テストスケッチはボードプロファイルを定義する `sketch.yaml` を持つ。`smoke/sketch.yaml` を雛形として参照。

### 実行モード

```sh
# ビルド・フラッシュ・テスト（デフォルト）
uv run --env-file .env pytest smoke/

# ビルドのみ（ボード不要）
uv run --env-file .env pytest smoke/ --run-mode=build

# テストのみ（書き込み済みファームウェアを使用）
uv run --env-file .env pytest smoke/ --run-mode=test
```

### Arduino CLI のセットアップ

Arduino CLI が `PATH` に存在し、必要なコアがインストールされていること:

```sh
arduino-cli core update-index
arduino-cli lib update-index
```

`host` プロファイルは [lang-ship Arduino core](https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json) を使用する。

## 依存関係

Python 依存は `pyproject.toml` に宣言し、`uv.lock` でロックする。`uv run` が初回に自動でローカル仮想環境へインストールする。

| パッケージ | 役割 |
|-----------|------|
| `pytest` | テストランナー |
| `pytest-embedded` | 組み込みテストフレームワーク |
| `pytest-embedded-serial` | シリアル通信 |
| `pytest-embedded-arduino-cli` | Arduino CLI によるビルド・フラッシュ |
| `pytest-html` | HTML レポート（任意） |
