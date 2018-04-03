mq2image: *.cpp 
	g++ *.cpp -ljson-c `pkg-config --libs libSimpleAmqpClient opencv` -lcurl -pthread -o mq2image 