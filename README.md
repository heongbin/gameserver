# gameserver
* monstermanager클래스의 일부 로직을 락이 아닌 worker에 doasync함수를 이용하여 일감형태로 큐에 밀어넣음.
* 그렇게 함으로써 worker클래스가 상속받는 jobqueue 내부에서 큐에 일감을 넣을때나 뺄때 일시적으로 잠까만락을 잡음으로써 최대한 락잡는 시간을 줄임.