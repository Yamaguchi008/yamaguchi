# 成果物 "装着するだけで運動をアシストしてくれる装置"

## 機能  
・装置を体に装着することで装着者の運動量を計測
・スイッチを押すことで,装置から運動をアシストする音声を出力

## 使用方法  
1. 初期設定として，SDカード内に下記の「PLAYLIST」「AUDIO」「BIN」の各ファイルを書き込む
2. 装置を腕などに取り付け，ポータブルバッテリーなどの電源を接続する  
3. 電源に接続すれば距離計測が開始する
4. 現在の走行距離を知りたいときに付属のスイッチを押すことで音声が流れる

## プログラムの説明  
1. SPRESENSE.ino  
・メインのプログラム  
・加速度センサとジャイロセンサを用いた運動距離計測と距離に応じたサポート音声の出力を行う
・本音声は0kmから4kmまで，1kmごとに距離を知らせる

2. AUDIO
・出力するサポート音声が入ったファイル
・本プログラムでは0kmから4km通過まで，1kmごとの音声が入っている
・本音声は https://note.cman.jp/other/voice/ を用いて作成した
2.1. 00.mp3，01.mp3，02.mp3，03.mp3，04.mp3
・ファイル名の数字を読み上げる音声
2.2. km_over.mp3
・「km通過しました」と読み上げる音声

3. PLAYLIST 
・サポート音声のプレイリストが入ったファイル
3.1. TRACK_Dx.csv (xは0から4までの数字)
・上記のxが走行距離に対応している

## SysMLモデル  
![スライド1](https://user-images.githubusercontent.com/55196978/105039140-8b68c380-5aa3-11eb-8d15-6a7670e584bb.JPG)
![スライド2](https://user-images.githubusercontent.com/55196978/105039203-9facc080-5aa3-11eb-86eb-43834c9916a7.JPG)
![スライド3](https://user-images.githubusercontent.com/55196978/105039236-a6d3ce80-5aa3-11eb-8d46-01a1409cca77.JPG)
![スライド4](https://user-images.githubusercontent.com/55196978/105039258-adfadc80-5aa3-11eb-9c69-f8e538a083b6.JPG)
![スライド5](https://user-images.githubusercontent.com/55196978/105039281-b521ea80-5aa3-11eb-8d3b-1f72173fbc75.JPG)

##  プログラム ライセンス 
MIT  

## 合成音声 ライセンス
![LISENCE](https://user-images.githubusercontent.com/55196978/105039465-eef2f100-5aa3-11eb-98f3-e4e0cb44beef.png)
HTS Voice "Mei(Normal)" Copyright (c) 2009-2013 Nagoya Institute of Technology
