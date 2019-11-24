# go-cpp
Golang syntax in C++

This project is a PoC demonstrating basic support of go syntax in C++.

## Technical description
To make it work we first need coroutine support. THis is done by using `coroutine.h` from https://github.com/tonbit/coroutine.

Then we add an abstraction layer implementing the go syntax and a basic scheduler. This is done in `go.h`.

Coroutines are executed on their own thread. This thread is started only when the first coroutine is started.

Qt is used to deal with low level stuff like mutex of thread messages.

## Example
```c++
auto c = new go::Channel<quint32>();
go::go([c](){
    for (;;) {
        auto val = QRandomGenerator::system()->generate();
        qDebug() << val;
        *c << val;
        go::sleep(5000);
    }
});
```

```go
c := make(chan int)
go func f() {
    for {
        val := rand.Intn(1024)
        fmt.Println(val)
        c <- val
        time.Sleep(5000)
    }
}

```

## To do
- Use a thread pool instead of a single thread.
- Create extra threads dynamically when a coroutine blocks.
- Make channel an "explicitly shared data" class so that we can copy channels around instead of dealing with pointers.