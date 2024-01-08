# M88k - PC8801 Series Emulator

## 初めに

rururutan氏による、私家版[M88](https://github.com/rururutan/m88)をforkしたものです。
オリジナルのREADMEのコピーは[こちら](./docs/README_rururutan.md)です。

ベースとなっているのは、cisc氏作のPC-8801エミュレータ[M88](http://retropc.net/cisc/m88/) Ver 2.21aです。

M88のコードを最新の環境でも動かしメンテナンスを続けられるように修正を加えています。

* Visual Studio 2022 / JetBrains CLion でビルドできる様に修正
* コードのフォーマットの統一
* 古くなったコードの削除
* 64bit 環境対応など各種バグ修正
* 実験的なコードの追加 (Direct3D12, [YMFM](https://github.com/aaronsgiles/ymfm), [ASIO](https://www.steinberg.net/developers/) 対応など)

## 動作環境

* Windows 10以降 (32/64bit)

## ライセンス

[ライセンス](./LICENSE.md) に従います。
