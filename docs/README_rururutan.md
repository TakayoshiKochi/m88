# M88 - PC8801 Series Emulator

## 初めに

cisc氏作のPC-8801エミュレータ[M88](http://retropc.net/cisc/m88/)を改造した私家版です。

* 最近のコンパイラでビルドできる様に修正。
* 今時の環境に合わせてデフォルトの設定値を変更。
* 英語101キーモードの追加。
* ウインドウ座標を保存するオプションを追加。
* PC-8001mkIISR ひらがなフォントファイルの形式を変更。
* fmgenの修正。
* [c86ctl](https://launchpad.net/c86ctl)用の制御を追加。

## 動作環境

* Windows Vista以降

## C86CTLの使用

OPNA並びにOPN3Lモジュールに対応しています。
現在の所[G.I.M.I.C](http://gimic.jp/index.php?G.I.M.I.C)が使用可能です。

[c86ctl](https://launchpad.net/c86ctl)が必要です。
事前にM88.exeと同じパスにc86ctl.dllを配置して下さい。

OPN3Lはハードウェア的にFM/SSG音量バランスが固定で(88的には)小さい為、少々聞き苦しいと思います。

## 80SRひらがなフォント

ファイル名はFONT80SR.ROMです。

ファイル形式は[J80](http://www.geocities.jp/upd780c1/pc-8001/index.html)のPC-8001mkIISR.fonと同等の物です。

ソフトウェア的に読み出す方法が無い為、ROMリーダーを使って吸い出して下さい。

## ライセンス

[ライセンス](./LICENSE.md) に従います。
