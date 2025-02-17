# Summary of Shell

## fork

1. fork will create a dupliacte(copy) process of parent procees, including file descripter.
After fork, both child and parent will keep running their own codes independently.

## execvp

1. Its purpose is to replace the current process's address space (including the code segment, data segment, heap, stack, etc.) with a new program.
2. However, it **does not modify** the process's **file descriptor table**. This means that execvp retains all file descriptors that were open in the current process, unless they are explicitly closed or modified

## File Descriptor Table

The file descriptor table is a kernel-maintained data structure for each process, used to track files and other resources opened by the process. Each _entry_(fd) in the file descriptor table contains the following information:

1. Pointer to the file: Represents the file or resource currently associated with the file descriptor.

2. File offset: Indicates the current read/write position within the file.

3. File flags: For example, O_RDONLY (read-only), O_WRONLY (write-only), etc.

## dup2

dup2(int oldfd, int newfd): fd is an entry of file descriptor table. The process of dup:

1. check newfd is open or not. If open, close the resoure which refers to the following actions:

   1. Releasing resources:
        * If the file descriptor (e.g., newfd) currently points to a file, pipe, or other resource, the kernel decrements the reference count for that resource.

        * If the reference count drops to 0
        the kernel releases the resource (e.g., closes the file, frees the pipe buffer, etc.).
   2. Clearing the table entry:
        * The kernel empties the contents of the file descriptor table entry corresponding to newfd, including:
            * Setting the pointer to the file to NULL.
            * Resetting the file offset and flags.
2. copy oldfd to newfd:
   1. The kernel sets the file descriptor table entry of newfd to a copy of oldfd:

       * The entry for newfd is updated to point to the same resource as oldfd.

   2. Both newfd and oldfd now reference the same resource:

        * Any operations on newfd or oldfd will affect the same underlying resource (e.g., file, pipe, etc.).

   3. The reference count of the resource pointed to by oldfd is incremented by 1:

        * This is because there are now two file descriptors (oldfd and newfd) referencing the same resource.

## pipe and pipe in IPC

如果一个父程序创建了管道，然后fork，然在在子程序里关闭了管道写入端，这个时候管道写入端被关闭了么？

1. 管道的文件描述符
   * 管道（pipe）是一种进程间通信（IPC）机制，它有两个文件描述符：
       * 读取端（read end）：用于从管道中读取数据。
       * 写入端（write end）：用于向管道中写入数据。
   * 当父进程调用 pipe(int pipefd[2]) 时，会创建一个管道，并将读取端和写入端的文件描述符分别存储在 pipefd[0] 和 pipefd[1] 中。
2. fork() 的行为
   * fork() 会创建一个子进程，子进程是父进程的副本，包括文件描述符表。
   * 父进程和子进程都会继承管道的读取端和写入端的文件描述符。

3. 子进程关闭管道写入端
   * 如果子进程关闭了管道的写入端（close(pipefd[1])），那么只有子进程的写入端被关闭。
   * 父进程的写入端仍然保持打开状态，除非父进程也显式地关闭它。

4. 管道写入端是否被关闭？
   * 从子进程的角度：子进程关闭了写入端，因此子进程不能再向管道写入数据。
   * 从父进程的角度：父进程的写入端仍然打开，父进程可以继续向管道写入数据。
   * 从管道的角度：管道的写入端是否被关闭，取决于所有持有写入端文件描述符的进程是否都关闭了它。
5. 关键点：文件描述符的引用计数
   1. 文件描述符（File Descriptor）
      文件描述符是一个整数（如 0、1、2、3 等），它是文件描述符表中的一个索引。
      文件描述符本身不带有引用计数，它只是一个指向资源的“句柄”。

   2. 文件描述符指向的资源（Resource）
      文件描述符指向的资源（如文件、管道、套接字等）是由内核管理的。资源带有引用计数：内核会为每个资源维护一个引用计数，表示有多少个文件描述符指向该资源。

   3. fork() 的行为
      当父进程调用 fork() 时，子进程会继承父进程的文件描述符表。子进程的文件描述符会指向与父进程相同的资源。
      资源的引用计数会增加：因为现在有两个进程（父进程和子进程）的文件描述符指向同一个资源。
   4. 但之后，父子同样的文件描述符可以指向不同资源。

   5. 父进程创建的管道在 fork() 之后不会出现副本。管道本身是一个内核管理的资源，fork() 只会复制文件描述符表，而不会复制管道本身。

## wait(nullptr), waitpid(), 后台运行程序, 非阻塞等待

    启动 sleep 5 &：
    启动一个后台任务。
    sleep 5 会在后台运行，父进程不会被阻塞。

    启动 ls：
    启动一个前台任务。
    调用 wait(nullptr) 试图等待前台任务的完成。
    问题：
    如果 sleep 5（后台任务）在 ls（前台任务）完成之前终止，那么 wait(nullptr) 会立即返回，并且不会等待 ls 的完成。这就导致了前台任务变成了非阻塞的情况。

    所以要用waitpid(pid, nullptr, 0)指定等待的事哪个子程序。
    waitpid 函数：
    pid_t waitpid(pid_t pid, int *status, int options);
    pid: 指定要等待的子进程的 PID。
    status: 子进程的终止状态。
    options: 等待选项。
    第一个参数 pid：
    pid > 0: 等待指定 PID 的子进程。
    pid == 0: 等待与调用进程同属一个进程组的任意子进程。
    pid == -1: 等待任意子进程（等价于 wait）。
    pid < -1: 等待与调用进程同属一个进程组 ID 的任意子进程。
    第三个参数 options：
    WNOHANG: 非阻塞模式。如果没有子进程已终止，waitpid 立即返回 0。
    0: 阻塞模式，直到一个子进程终止。

## 让父程序收到后台运行的子程序终止的信号

   1. 需要用signal注册一个型号处理程序，
   2. 同时为了让前台的子程序终止的信号不要处理，还要有个信号屏蔽程序

## write 和 cout的不同

   1. cout 的缓冲机制：cout 是一个 C++ 标准库的输出流，它使用缓冲机制来提高性能。这意味着输出的数据会先存储在一个缓冲区中，只有在缓冲区满了或者显式地刷新缓冲区（例如通过 std::flush）时，数据才会真正写入到终端。这种缓冲机制在正常情况下工作得很好，但在信号处理函数中可能会引发问题。
   2. 信号处理函数的限制：信号处理函数有很多限制，其中之一就是它们必须尽可能简单和快速，因为它们会中断正常的程序执行。很多标准库函数（包括 cout）都不是异步信号安全的，这意味着它们在信号处理函数中调用时可能会出现未定义的行为。write 是一个系统调用，它是异步信号安全的，适合在信号处理函数中使用。
   3. write 是低级系统调用：write 是一个直接向文件描述符（在这种情况下是标准输出，即终端）写数据的低级系统调用。它不使用缓冲区，因此数据会立即写入到终端。这确保了在信号处理函数中使用 write 时，输出会立即生效，而不会受到缓冲机制的影响。
