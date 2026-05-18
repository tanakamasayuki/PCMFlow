# テスト

> English: [README.md](README.md)

PCMFlow の自動テストと手動テストを置くディレクトリ。
テスト方針とカバレッジは [TEST_PLAN.ja.md](TEST_PLAN.ja.md) を参照。

[pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) と Arduino CLI バックエンドを使い、ホスト（`lang-ship:host`）または実機（ESP32）でスケッチをビルド・実行する。

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

# スモークテストのみ
uv run --env-file .env pytest smoke/

# 実機ESP32で実行
uv run --env-file .env pytest smoke/ --profile=esp32
```

前回失敗したテストのみ再実行:

```sh
uv run --env-file .env pytest --lf
```

HTML レポート:

```sh
uv run --env-file .env pytest --html=report.html --self-contained-html
```

## ディレクトリ構成

- `smoke/` — 雛形検証用のスモークテスト。ホスト上でビルド・実行できる最小スケッチ。
- 以降、`host/` `device/` `manual/` を必要に応じて追加する（[TEST_PLAN.ja.md](TEST_PLAN.ja.md) 参照）。

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
