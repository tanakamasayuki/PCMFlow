# Changelog / 変更履歴

## Unreleased

## 0.2.1
- (EN) Fix: `pump()` no longer latches EOF when a streaming source temporarily returns 0 frames; `srcEof_` is now set only when the active source reports `isEof()`.
- (JA) 修正: ストリーミングソースが一時的に 0 フレームを返した際に `pump()` が EOF を確定してしまう不具合を修正。`srcEof_` はアクティブソースの `isEof()` が真の場合のみ立てるように変更。

## 0.2.0
- (EN) Initial release
- (JA) 初期リリース
