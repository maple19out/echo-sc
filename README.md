# echo-sc

C++을 이용한 간단한 client-server 소켓 프로그래밍 예제입니다.

client와 server 단에서 각각 thread를 이용해 데이터를 수신하는 부분을 recvThread로 따로 구현하였습니다. server에서는 다수의 client에 대한 접속을 처리하는 와중에 synchronization 문제를 해결하기 위해 mutex를 활용하였습니다.

추가적으로 서버에서 아래의 옵션을 실행시킬 때 인자로 주어 추가 기능을 사용할 수 있습니다.

`-e` option : 클라이언트로 부터 수신된 데이터를 다시 클라이언트로 전송한다. (echo back)

`-b` option : 클라이언트로부터 수신된 데이터를 모든 클라이언트에게 브로드캐스트 한다. (broadcast)
