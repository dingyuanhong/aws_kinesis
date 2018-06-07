package main

import (
	"fmt"
	// "bytes"
	"io"
	// "io/ioutil"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	// "github.com/aws/aws-sdk-go/aws/awserr"
	// "github.com/aws/aws-sdk-go/aws/request"
	"github.com/aws/aws-sdk-go/aws/credentials"
	"github.com/aws/aws-sdk-go/aws/session"

	"github.com/aws/aws-sdk-go/service/kinesis"
	"github.com/aws/aws-sdk-go/service/kinesisvideo"
	"github.com/aws/aws-sdk-go/service/kinesisvideomedia"
)

func getEndpoint(sess *session.Session) *string {
	kv := kinesisvideo.New(sess);

	input := kinesisvideo.GetDataEndpointInput{
		APIName:aws.String(kinesisvideo.APINameGetMedia),
		StreamName:aws.String("test"),
	}
	
	endpointOutput,error := kv.GetDataEndpoint(&input);
	
	if(error == nil){
		return endpointOutput.DataEndpoint;
	}
	return nil;
}

func getMeidaData(sess *session.Session,endpoint *string,streamName *string) {
	sess_videomedia := sess.Copy(&aws.Config{
		Endpoint : endpoint,
	});

	kv := kinesisvideomedia.New(sess_videomedia);
	
	input := kinesisvideomedia.GetMediaInput{
		StartSelector: &kinesisvideomedia.StartSelector{
			StartSelectorType: aws.String(kinesisvideomedia.StartSelectorTypeNow),
		},
		StreamName: streamName,
	}

	output,error := kv.GetMedia(&input);
	if(error != nil){
		fmt.Printf("error:%v\n",error);
		return;
	}
	
	fmt.Printf(aws.StringValue(output.ContentType));

	r := output.Payload;
	defer r.Close();

	// readCloser(r);
	pipeFile(r);
}

func readCloser(reader  io.ReadCloser) {
	capacity := 1024*1024;
	buf := make([]byte, capacity, capacity)
	var n int64 = 0
	for {
		//把io.Reader内容写到buf的free中去
		m, e := reader.Read( buf )
		// m, e := reader.Read( buf[0:cap(buf)] )
		
		if m > 0 {
			fmt.Printf("cur:%d\n", m)
			n += int64(m)
		}

		//读到尾部就返回
		if e == io.EOF {
			fmt.Printf("over:%d\n", n)
			break
		}
		if e != nil {
			fmt.Printf("error:%d\n", n)
			break;
		}

		if (m == 0) {
			//fmt.Printf("read:%d\n", m)
			break;
		}
	}
}

func pipeFile(reader  io.ReadCloser) {
	capacity := 1024*1024;
	buf := make([]byte, capacity, capacity)

	r, w := io.Pipe()
	
	var n int64 = 0
	for {
		//把io.Reader内容写到buf的free中去
		m, e := reader.Read( buf )
		// m, e := reader.Read( buf[0:cap(buf)] )
		
		if m > 0 {
			fmt.Printf("cur:%d\n", m)
			n += int64(m)
		}

		//读到尾部就返回
		if e == io.EOF {
			fmt.Printf("over:%d\n", n)
			break
		}
		if e != nil {
			fmt.Printf("error:%d\n", n)
			break;
		}

		if (m == 0) {
			//fmt.Printf("read:%d\n", m)
			break;
		}
	}
}

func putStreamData(sess *session.Session,streamName *string ,previousSequenceNumer * string) {
	ki := kinesis.New(sess);
	data := make([]byte,1024*1024,1024*1024);

	input := kinesis.PutRecordInput{
		Data : data,
		// ExplicitHashKey : aws.String(""),
		// PartitionKey : aws.String(""),
		SequenceNumberForOrdering : previousSequenceNumer,
		StreamName : streamName,
	};
	output,error := ki.PutRecord(&input);
	if(error != nil) {
		fmt.Printf("%s%s\n", aws.StringValue(output.ShardId),
			aws.StringValue(output.SequenceNumber),
		);
	}
}

func getStreamData(sess *session.Session,ShardIterator *string) *string {
	ki := kinesis.New(sess);

	input := kinesis.GetRecordsInput{
		Limit : aws.Int64(1),
		ShardIterator : ShardIterator,
	};
	output,error := ki.GetRecords(&input);
	if(error != nil) {
		
		// for k,v := range(output.Records) {
		// 	// v.Data;
		// }

		return output.NextShardIterator;
	}
	return aws.String("");
}

func getShardIterator(sess *session.Session,streamName * string,sequenceNumber *string,timestamp *time.Time) * string {
	ki := kinesis.New(sess);

	input := kinesis.GetShardIteratorInput{
		ShardId : aws.String(""),
		ShardIteratorType : aws.String(kinesis.ShardIteratorTypeAtSequenceNumber),
		StartingSequenceNumber : sequenceNumber,
		Timestamp : timestamp,
		StreamName : streamName,
	};
	output,error := ki.GetShardIterator(&input);
	if(error != nil) {
		return output.ShardIterator;
	}
	return aws.String("");
}

func main(){
	credentials := 	credentials.NewStaticCredentialsFromCreds(credentials.Value{
		AccessKeyID:"",
		SecretAccessKey:"",
	})
	
	config := aws.Config{
		Credentials : credentials,
		Region: aws.String("ap-northeast-1"),
		MaxRetries : aws.Int(3),
		UseDualStack : aws.Bool(false),
	};
	
	sess := session.Must(session.NewSessionWithOptions(session.Options{
		Config: config,
//		CustomCABundle : nil 
	},
	))
	
	endpoint := getEndpoint(sess);
//	fmt.Printf(aws.StringValue(endpoint))

	getMeidaData(sess,endpoint,aws.String("test_audio"));
	
	return;
}