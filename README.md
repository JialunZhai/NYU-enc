# NYU-enc: Not Your Usual ENCoder

## A parallel run-length encoder for data compression

## Background

Data compression is the process of encoding information using fewer bits than the original representation. Run-length encoding (RLE) is a simple yet effective compression algorithm: repeated data are stored as a single data and the count.

A thread pool is a software design pattern for achieving concurrency of execution in a computer program. One benefit of a thread pool over creating a new thread for each task is that thread creation and destruction overhead is restricted to the initial creation of the pool, which may result in better performance and better system stability. Based on POSIX thread, this project implemented a general thread pool named **ThreadPool**. With the help of the thread pool, the encoding task can be easily parallelized. (The previous to implement a general pool was implementing with generic, but implementing with dynamic binding is more suitable. In fact, both of them are typical ways to implement polymorphism.)

## Enviroment

This encoder is built on **Linux** with **C++11** language.

## Compilation

If you have installed CMake on your Linux System, you can directly run command `make` to compile; Otherwise, you should run command

`g++ -pthread nyuenc.cpp -std=c++11 -o nyuenc`

to compile.
If you compiled successfully, you will see an executable file named **nyuenc** in current directory.

## Usage

The usage of the encoder is simple. Here the optional option _-j_ followed by an positive integer argument _$jobs_ indecate the amount of threads you want to use.

```bash
./nyuenc [-j $jobs] infile.txt > outfile.txt
```

To creat a thread pool, an unsigned integer, indicating the size of the pool, must be provided to the constructor of **ThreadPool**.

```C++
ThreadPool::ThreadPool(const unsigned &pool_sz);
```

Add a new task into the waiting queue. Note that the task must be encapsulated as an object of the subclass of class **Task**.

```C++
ThreadPool& addTask(unique_ptr<Task> task);
```

To sutdown the thread pool, just call

```C++
void ThreadPool::shutDown();
```

## Acknowledgement

This project came from course **Operating Systems (CSCI-GA.2250-002)** in **NYU**, offered by **Prof. Yang Tang**.
