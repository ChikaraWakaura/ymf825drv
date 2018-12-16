# 1.概要

本家 sample2 の 1ch MIDI ドライバをラズパイ向け ALSA 16ch MIDI ドライバ(ユーザランド)化するものです。
標準ドラムパート MIDI ChNo.9(0基点) は通常音色です。

# 2.ハード準備

最大の難問が R0 剥がし S1 盛りです。
S1 盛りがクラック、ボイドなしか目視確認不能ですので気合い入れます(笑)

ラズパイとの配線は以下のとおりです。

    ymf825board    Raspi  
    1 SS --------- 22 GPIO25  
    2 MOSI ------- 19 GPIO10 SPI0 MOSI  
    3 MISO ------- 21 GPIO 9 SPI0 MISO  
    4 SCK -------- 23 GPIO11 SPI0 SCLK  
    5 GND --------  6 GND  
    6 5V ---------  2 VCC(+5V)  
    7 RST_N ------ 36 GPIO16  
    8 Audio ------ NC  
    9 3.3V -------  1 VCC(+3.3V)  

ブレッドボードで基礎的動作確認が終わりましたので他実験で使用していた秋月ユニバーサル基板を
一度フリーにしてから配置、配線しました。

ymf825board に付属しているミニジャックは 100 回抜き差し試験に耐えられそうない感じ(笑)
なので RCA 端子より疑似ステレオ出力としました。

# 3.ソフト機能拡張及び変更について

 16ch                      対応(ChNo.9 は非標準ドラムパート)
 CC7                       MASTER VOL -> ChVol に変更
 CC11                      対応
 CC121                     対応
 CC120                     対応
 CC123                     対応

 fmasgn.c                  変更なし
 fmif.c
   Fmdriver_init()         本タイミングへ Asgn_init() と Tone_init() を移動(CC121に関連)
                           16ch 初期化処理へ変更
   generateMidiCmd()       16ch 処理へ変更
 fmnote.c
   Note_damp()             16ch 化に伴いリンクリスト破壊後のリンクはまり対策
   Note_chgChVol()         追加
   Note_chgExpression()    追加
 fmpart.c
   Part_cc11()             追加
   Part_init()             Asgn_init() と Tone_init() を削除(CC121に関連)
   Part_cc()               CC7 変更, CC11,CC121,CC120,CC123 追加
 fmsd1_raspi.c
   writeSingle()           bcm2835_spi_transfern() へ変更
   readSingle()            追加
   writeBurst()            bcm2835_spi_transfern() へ変更
   delayMs()               秒込みウェイト指定化へ変更
   initSPI()               RST ピン出力定義追加
   initSD1()               9,25 レジスタ書き込み定義見直しと変数化
 fmtone.c                  変更なし
 fmvoice.c
   Fmvoice_keyon()         VoVOL 設定見直し , ChVol CC7 化 , Part_toneNumber() 設定見直し
   Fmvoice_damp()          16ch 化に伴いリンクリスト破壊後のリンクはまり対策
   Fmvoice_chgVibDpt()     XVB 設定見直し
   Fmvoice_chgChVol()      CC7  対応(ChVol 利用のため CC11 とレジスタ共有)
   Fmvoice_chgExpression() CC11 対応(ChVol 利用のため CC7  とレジスタ共有)
 ymf825drv.c               新規
 fmasgn.h                  変更なし
 fmboard.h                 変更なし
 fmif.h                    マクロ追加
 fmnote.h                  プロトタイプ宣言追加
 fmpart.h                  プロトタイプ宣言追加
 fmsd1.h                   プロトタイプ宣言追加
 fmtone.h                  変更なし
 fmtype.h                  変更なし
 fmvoice.h                 プロトタイプ宣言追加

# 4.動作確認

SPI を必要とします。

$ cat /boot/config.txt | grep spi
dtparam=spi=on

libbcm2835 を必要とします。

$ wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.57.tar.gz
$ tar zxvf bcm2835-1.57.tar.gz
$ cd bcm2835-1.57/
$ ./configure
$ make
$ sudo make install

libasound2-dev を必要とします。

$ sudo apt-get update
$ sudo apt-get install libasound2-dev

メイクと実行

本リポジトリを展開します。

$ cd ymf825drv
$ make clean;make
$ sudo ./ymf825drv -v
Initialize SPI.
Initialize YMF825.
Dump YMF825 Register
     : +0 +1 +2 +3 +4 +5 +6 +7 +8 +9
0000 : 01 00 00 01 01 00 00 00 00 84
0010 : 00 00 00 00 00 00 00 00 00 00
0020 : 00 00 00 00 00 CC 00 3F 00 01
YMF825 Communication checking...
YMF825 Communication check complete.
Initialize FM Driver.
YMF825 MIDI Driver Version 0.1.0 ready.
ALSA MIDI Port(128:0) 'YMF825 MIDI' ready.
Enable MIDI Channel. 0123456789ABCDEF
                     1111111110111111

上記のような表示が出て待機になると OK です。
Ctrl + C で停止します。
利用可能なオプションは -h で確認できます。

レジスタダンプ値違いまたは以下の表示の場合は配線再確認、電圧、電流確認です。

YMF825 Communication checking...
R/W Failed : Write(xx) Read(yy)

他ターミナルより以下コマンドにて YMF825 MIDI ポートの存在確認が出来ます。

$ aplaymidi -l
 Port    Client name                      Port name
 14:0    Midi Through                     Midi Through Port-0
128:0    YMF825 MIDI                      YMF825 MIDI

ポートとファイル指定にて MIDI 再生可能です。

$ aplaymidi --port=128:0 hogehoge.mid

#5.応用編

raveloxmidi & rtpMIDI 利用の場合は以下のようになります。

$ sudo modprobe snd-virmidi
$ amidi -l
Dir Device    Name
IO  hw:0,0    Virtual Raw MIDI (16 subdevices)
IO  hw:0,1    Virtual Raw MIDI (16 subdevices)
IO  hw:0,2    Virtual Raw MIDI (16 subdevices)
IO  hw:0,3    Virtual Raw MIDI (16 subdevices)

$ aplaymidi -l
 Port    Client name                      Port name
 14:0    Midi Through                     Midi Through Port-0
 16:0    Virtual Raw MIDI 0-0             VirMIDI 0-0
 17:0    Virtual Raw MIDI 0-1             VirMIDI 0-1
 18:0    Virtual Raw MIDI 0-2             VirMIDI 0-2
 19:0    Virtual Raw MIDI 0-3             VirMIDI 0-3
128:0    YMF825 MIDI                      YMF825 MIDI

ポートを aconnect にて接続させます。

$ aconnect -x;aconnect 16:0 128:0;aconnect -l
client 0: 'System' [type=kernel]
    0 'Timer           '
    1 'Announce        '
client 14: 'Midi Through' [type=kernel]
    0 'Midi Through Port-0'
client 16: 'Virtual Raw MIDI 0-0' [type=kernel,card=0]
    0 'VirMIDI 0-0     '
        Connecting To: 128:0
client 17: 'Virtual Raw MIDI 0-1' [type=kernel,card=0]
    0 'VirMIDI 0-1     '
client 18: 'Virtual Raw MIDI 0-2' [type=kernel,card=0]
    0 'VirMIDI 0-2     '
client 19: 'Virtual Raw MIDI 0-3' [type=kernel,card=0]
    0 'VirMIDI 0-3     '
client 128: 'YMF825 MIDI' [type=user,pid=803]
    0 'YMF825 MIDI     '
        Connected From: 16:0

以下のようなファイルを ~/.config に準備します。

$ cat ~/.config/raveloxmidi-vmidi.conf
service.name = ymf825
file_mode = 0666
inbound_midi = /dev/null
alsa.output_device = hw:0,0,0

raveloxmidi ファイル指定実行

$ raveloxmidi -N -c ~/.config/raveloxmidi-vmidi.conf

これで rtpMIDI が動作する環境より任意の MIDI 再生ソフトからの MIDI 再生が可能になり
YMF825 MIDI へ MIDI 出力可能となります。

# 6.その他

YMF825-MIDI.xml は Domino 向けファイルです。利用される場合は Domino\Module へ
ファイルコピーして Domino 環境設定より選択利用下さい。

ほんと懐かしの FM 音源の発音に酔いしれることが出来ますね(笑)

以上です。

