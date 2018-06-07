
#CURL证书
export CURL_CA_BUNDLE=/etc/ssl/certs/cacert.pem
export SSL_CERT_DIR=/etc/ssl/certs/
export SSL_CERT_FILE=cacert.pem

#本地文件测试
./kinesis_video_gstreamer_sample_app test -w 3040 -h 1520 -f 30 -b 10 -p /data/wwwroot/data/1.mpg
./kinesis_video_gstreamer_sample_app test -w 3040 -h 1520 -f 30 -b 10 -p /data/wwwroot/data/1.mp4
./kinesis_video_gstreamer_sample_app test -w 3040 -h 1520 -f 30 -b 10 -p /data/wwwroot/data/1.264

#rtsp视频测试
./kinesis_video_gstreamer_sample_rtsp_app rtsp://10.30.80.1/1.264 test

export CURL_CA_BUNDLE=
export SSL_CERT_DIR=
export SSL_CERT_FILE=

#测试命令
./put test -p /data/wwwroot/data/1.mp4
./put test -p /data/wwwroot/data/1.264
./put test -p /data/wwwroot/data/2.mp4

./put_audio test_audio -p /data/wwwroot/data/2.mp4

#rtsp服务器
cd E:\test\live.2016.10.11\live\Project\x64\Debug
mediaServer.exe

#cacert.pem下载路径
#--without-ca-bundle --without-ca-path
#/etc/ssl/cert.pem
#wget https://www.baidu.com/link?url=DSuk2YwLfuXMyo1qDA6Yow5bWIPdSuFwbDQ9sE5VPTsOzG3VGfzt86ruXejkxzpn&wd=&eqid=fbd55f2a0000c253000000065abca2f0
#mv -f link?url=DSuk2YwLfuXMyo1qDA6Yow5bWIPdSuFwbDQ9sE5VPTsOzG3VGfzt86ruXejkxzpn cacert.pem
#mv -f cacert.pem /etc/ssl/certs/cacert.pem

#error code
#https://docs.aws.amazon.com/zh_cn/kinesisvideostreams/latest/dg/producer-sdk-errors.html
#NAL
#https://docs.aws.amazon.com/zh_cn/kinesisvideostreams/latest/dg/producer-reference-nal.html