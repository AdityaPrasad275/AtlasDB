This document is step 0 in making a database from scratch. It describes the intial steps of creating a persistent key-value store. Code in cpp. 

# the fairly non technical story 

## Overview
First we build a simple Key Value store. Satisfying three simple objectives :-
(1) We can set a value to key (`INSERT key value`)
(2) We can get value of a key (`GET key`)
(3) We can delete a key.

## Hashmap!

Now technically, all of these objectives are achieved by a simple Hashmap. Just intialize a hashmap. Done. can set, get, delete, all in $$O(1)$$. But then if you close the program, the data's gone. Which just ruins the entire purpose of database. 

So we need two components 
(1) the data in memmory, in ram, super fast fetch and execution 
(2) persistance, saved to the disk, brought back up
# Persistance

In persistance we aim for two goals (1) atomicity (2) durability.

Every action we do should either complete fully or fail fully. In-memory is not a problem. If you add a (key, value) pair to hashmap, it will work perfectly (actually is there possibility of failure here too?). When it comes to persistance, writing to disk, theres the posibility of failure. Maybe power failure, or idk. 

There shouldnt be a case that a command of `INSERT alice hi` is just aburptly ending in mid and later you see in the db, just "h" is present against the key alice. or could be even  worse. 

So before trying to solve this, lets understand what happens in file writing. Suppose I kept a simple file, following csv format. Like it could contain the following data
```text
key,value
1,hi
2,bye
```
Then any command, say `INSERT 3 why`, check if this key exists, if not then we would need to go at end of file, create a new key value pair. 

In writing to a file we have to go through a billion layers of RAM/cache/page pool idk, like this

```text
Program
     ↓
libc / system call
     ↓
Kernel Page Cache (RAM)
     ↓
Filesystem
     ↓
Block layer
     ↓
Disk Controller Cache
     ↓
Actual Disk / SSD
```

Dont worry too much abt the names or terms, the idea is first you got RAM. the OS control the RAM. what data remains in cache, when writes to disk actually happen. But below OS we got the disk hardware. That has layers as well. We can only be as durable as the storage device claims to be. It is completely possible that that storage device signaled persistance but it was actually in Disk ram cache. Power gone, data gone. woops. 

But thats alright. a command called `fsync` helps us flush ram cache, os page cache and signal us persistence. Pretty powerful, well as powerful as the storage layer underneath. To protect further against half writes, we can implement `length` and `checksums` check as well (not done till now tho).

## Ok ok persistence done, but are we fast?

Suppose you're sold on this method of fsync, flushing the cache, ensuring persitance and atomicity. But think abt the actual actions for a second. If we simply think abt storing (key,value) in a file, line by line. Insert and get operations become O(n) in persistance. You have to check entire file to see if key already exists, if not, add to end.
## WAL : Write ahead log

Theres a cleverer way to store your data. Supoose you didnt store (key,value) format, but rather just stored logs. That is, any call of insert(key, value) actually appends `INSERT key value` to the end of the file. (Oh i just realised currently at this stage we are not checking if key already exists. need to fix.) And delete would be similar. (oh we are not checking here either lol. ok need to fix that.) The actual file then looks like this
```text
INSERT name Alice
INSERT age 25
INSERT city New York
INSERT age 26
DELETE city
DELETE job
```

At the start of program we need to load this file into a in-memory hashmap. And subsequent insert, delete commands are appened onto end of file. Actually here, ensuring you first log into file (hit persistance check first) and then update your in memory store , is better. And it's called write ahead log (WAL).

And boom, you have a persitant key value store! Not exactly amazing yet, but a pretty solid starting point. 

# Deep dive into tech part

We are coding in cpp. We'll make a class called Database. Boring name, but alr. And heres how we will have our db.h

```cpp
#pragma once

#include<string>
#include<unordered_map>

class Database {
public:
    Database(const std::string logFile);
    ~Database();

    void insert(const std::string &key, const std::string &value);
    std::string get(const std::string &key);
    void deleteKey(const std::string &key);
private:
    std::string _logFile;
    int _fd; //file descriptor
    std::unordered_map<std::string, std::string> _in_memory_store; // keeping logFile in memory for fast access

    void _replayLog();
    void _appendLog(const std::string& record);
}
```

As you can see, we expose constructor, insert, get, delete. Constructor sets up our file.

In our private variables we have the name of our logFile (which we dont technically need tbh). And we've got a file descriptor. Think of this as sort of a pointer to the file. We can use this to interact with our file/persistance layer. And a hashmap, a in-memory KV store. so get becomes $$O(1)$$. (Technically insert and delete are also $$O(1)$$ but as they deal with persistence as well, they are a little slower)

In our private methods, we got replayLog, a method ran in beginning to read our file, get all data in our RAM. and we got appendLog whose responsibility is to write. and do proper error handling. ensuring our persistance. 

## The beginning, constructor
Here we intialise our private variables, run `_replayLog()`.
```cpp
Database::Database(const std::string logFile)
: _logFile(logFile), _fd(-1)
{
    _fd = open(_logFile.c_str(), O_RDWR | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    // O_RDWR -> read and write
    // O_APPEND -> append only
    // O_CREAT -> if doesnt exist, create
    // O_CLOEXEC -> This prevents the file descriptor from leaking into child processes after fork/exec.
    // 0644 -> permissions

    if(_fd == -1){
        throw std::runtime_error(
            "Failed to open logfile: " + std::string(strerror(errno))
        );
    }

    _replayLog();
}
```

We intiliase our file descriptor that would be used to insert and delete logs. Make it read, write. but write only by appending. Add in a few more stuff. Ensure file descriptor actually was made. Details of `_replayLog()` arent mentioned here, its just figuring out INSERT means map[key] = value, delete means erase and get is just... well get. Its more file parsing problem than anything interesting. The code can be viewed in the github repo!

## Ensuring writes are actually good
```cpp
void Database::_appendLog(const std::string& record){
    const char* data = record.c_str();
    size_t remaining = record.size();

    while(remaining > 0){
        ssize_t written = write(_fd, data, remaining);

        if(written == -1)
            throw std::runtime_error("write failed: " + std::string(strerror(errno)));

        data += written;
        remaining -= written;
    }

    if(fsync(_fd) == -1)
        throw std::runtime_error("fsync failed: " + std::string(strerror(errno)));

}
```

Here a lot of error logging is done to be perfectly sure it has actually been written in disk. No actually, we are just ensuring that the storage device actually confirms persistance. Not necessary both are true lol!

The while loop is necessary because techincally `write()` doesnt guarantee full write in one go. Maybe the kernel cache got pull, maybe some signal interrupt. So we add a while loop to ensure we atleast write everything we want to. And if at some point we miss, we throw error. This is ensuring durability and atomicity

Not fully. After all we are still depending on storage device. things like checksums, lengths can help strengthen this more.

## Actually inserting, getting, deleting

Now that we've initialised our db, we have ensured writes are durable (to an extent), we just need to write our insert, get, delete as wrapper on top of this

```cpp
void Database::insert(const std::string& key, const std::string& value){

    std::string record = "INSERT " + key + " " + value + "\n";

    // a problem : value could have spaces, replay log wouldnt really work? idk
    // actually another problem is it could ahve "\n"? man this tuff
    
    _appendLog(record);

    _in_memory_store[key] = value;

    std::cout << "IF no error popped up, it was successful!\n";
}

std::string Database::get(const std::string& key){
    if(_in_memory_store.count(key))
        return _in_memory_store[key];
    return "";
}

void Database::deleteKey(const std::string &key){
    std::string record = "DELETE " + key + "\n";
    
    _appendLog(record);

    _in_memory_store.erase(key);

    std::cout << "IF no error popped up, it was successful!\n";
}
```

That "if no error" is annoying but, eh works. The idea is we display errors, theres no real "success" indicators. If nothing went wrong, great!

## And thats a wrap for step 0

We've built a fairly durable and atomic KV store db! You can check its test in main.cpp, a simple check if its working or not. We'll expand more in step 1. add in checksums, length for atomicity verification. We'll try implementing B+Tree,  and really hone in on performance. 

