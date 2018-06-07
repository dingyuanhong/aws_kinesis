
#std库
rm -f /usr/lib64/libstdc++.so.6
ln -s /usr/local/lib64/libstdc++.so.6 /usr/lib64/libstdc++.so.6


#设置AMAZON路径
export AMAZON=$(pwd)/amazon-kinesis-video-streams-producer-sdk-cpp

#设置模块路径
export  LD_LIBRARY_PATH=LD_LIBRARY_PATH:${AMAZON}/kinesis-video-native-build:${AMAZON}/kinesis-video-native-build/downloads/local/lib/

#cacert.pem下载路径
#--without-ca-bundle --without-ca-path
#/etc/ssl/cert.pem
#wget https://www.baidu.com/link?url=DSuk2YwLfuXMyo1qDA6Yow5bWIPdSuFwbDQ9sE5VPTsOzG3VGfzt86ruXejkxzpn&wd=&eqid=fbd55f2a0000c253000000065abca2f0
#mv -f link?url=DSuk2YwLfuXMyo1qDA6Yow5bWIPdSuFwbDQ9sE5VPTsOzG3VGfzt86ruXejkxzpn cacert.pem
#mv -f cacert.pem /etc/ssl/certs/cacert.pem

#设置证书
export CURL_CA_BUNDLE=/etc/ssl/certs/cacert.pem