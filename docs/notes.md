write fails
```
disk full
filesystem error
file descriptor invalid
interrupted system call
```

fysnc fails
```
disk IO error
device failure
filesystem corruption
```

2. write() can write less than requested

This is the part beginners miss.

You ask for:

20 bytes

Kernel writes:

12 bytes

and returns 12.

This is called a partial write.

If you don't loop until all bytes are written, your log entry gets truncated.

Which ruins recovery.

Write-ahead principle:

log first
memory second