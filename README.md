# Tiny2313 MIDI Interrupter
Tiny2313 AVRを使用したMIDI Interrupterです。

Blog URL: [http://kingyonull.blogspot.com/2013/09/midi-v21.html](http://kingyonull.blogspot.com/2013/09/midi-v21.html)

DRSSTCの制御用として開発しましたが、出力パルス幅は約9.6us固定です。外部に555タイマICなどを用いたパルス幅制御回路を組むことを前提としてます。

最終更新が2013年ですが、暇があれば修正&未実装機能の対応をしておきます。。

# 実装済機能
- MIDI信号を受信して、6和音までの固定パルス幅の信号を出力
- ノート番号（音の高さ）によって出力ポートを振り分けられる
- MIDIチャンネルによって　　　　　　　"

# 未実装機能
- ベロシティに対応してパルス幅を可変させる
- ピッチベンド

# 動作環境
- Atmel Studio 7 (Version: 7.0.1645)
- Optimization: -Os

