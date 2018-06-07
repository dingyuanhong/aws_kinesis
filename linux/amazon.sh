PWDIR=$(pwd)

if [ ! -d amazon-kinesis-video-streams-producer-sdk-cpp ]; then
	git clone https://github.com/awslabs/amazon-kinesis-video-streams-producer-sdk-cpp
fi
export AMAZON=$(pwd)/amazon-kinesis-video-streams-producer-sdk-cpp

cd $PWDIR
cd script
. ./copy.sh

cd $PWDIR
cd amazon-kinesis-video-streams-producer-sdk-cpp/kinesis-video-native-build/
./min-install-script