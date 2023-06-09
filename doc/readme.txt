﻿-------------------------------------------------------------------------------
 Luna v1.22
                                                  Copyright (c) 2009-2023 MAYO.
-------------------------------------------------------------------------------

■このソフトについて

ダウンロードありがとうございます。このソフトは、各種音楽データを再生する
メディアプレイヤーソフトです。
プラグインに対応しており、対応形式を増やすことができます。


○動作環境
・Windows XP (SP3)/7 (SP1以上)/10
※Windows 8/8.1での動作は未確認です。動くとは思いますが。


■インストール

・どこでも結構ですので、アーカイブを展開するだけでOKです。
・ショートカットは自動で作成しませんので、個別に作成してください。


■アンインストール

・展開したファイルを削除していただければ完了です。（レジストリは使用しません）
・起動すると、luna.lst、luna.iniというファイルができます。
  これらのファイルも、削除してください。


■歌詞表示について

・曲ファイルと同じ場所で、拡張子がlrcというファイルを歌詞ファイルとします。
　再生時間に合わせて表示します。
※ファイルの文字コードによっては正常に表示できない可能性があります。
　対応している文字コードは、Shift-JIS, UTF-8, UTF-16です。（自動判断）
　ただし、UTF16の場合は、BOMをつけるようにしてください。


■出力ビット数指定時

下記の設定から選べます。安定重視をおすすめします。

・自動判定：元データのビット数に応じて、同じビット数で出力します。
・安定重視：すべてのビット数のデータを16bitに変換して出力します。
・Fix16bit：すべてのビット数のデータを16bitに変換して出力します。
・Fix24bit固定：すべてのビット数のデータを24bitに変換して出力します。
・Fix32bit固定：すべてのビット数のデータを32bitに変換して出力します。
・16/24bit：8/32bitは24bitで、それ以外は元のビット数で出力します。
・16/32bit：8/24bitは32bitで、それ以外は元のビット数で出力します。
・24/32bit：8/16bitは32bitで、それ以外は元のビット数で出力します。


■CoreAudio出力使用時の注意点

・Windows Vista以降のWindowsに搭載されている、新しいオーディオ出力機能です。
　音量を100%にしている場合、ビットパーフェクトな出力が可能です。
・コントロールパネルで、排他的に使用することを許可してください。
・WindowsVistaの場合、サービスパック１以上に更新してください。
※排他モードの設定変更は、再生を停止している時にしてください。


■転載・配布

プラグインについては、自由に扱ってくださってかまいません。
単体配布や別ソフトにあわせての配布もOKです。他のソフトでの利用も自由にどうぞ。

プレイヤ本体についても、自由に行ってくださってかまいません。
ただし、実費を除き、無償にて配布をお願いいたします。


■免責事項

Lunaを使用したことにより、いかなる損害が発生したとしても、
作者(MAYO)はその責任を負いません。


■サポートについて

申し訳ありませんが、サポートは行っておりません。
自己責任にて、ご利用ください。


■最後に

開発やテストにご協力いただいた皆様、本当にありがとうございました。


■更新履歴

history.txtに記載しています。

