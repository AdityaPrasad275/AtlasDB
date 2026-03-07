First order of business

Three operations (1) insert (2) read (3) delete 

For step0, we will only focus on (Key, value) pairs. 

Go as simple as possible. 

So I need to figure out 
(1) how to read a file
(2) write to it
(3) delete stuff

I'm reading (build you own db)[https://build-your-own.org/database/01_files] , and in it says the problem is "save data in a file and try to make it atomic and durable, i.e., resistant to crashes." 

Alright so turns out renaming is atomic. Interesting file system quirk. We are going to use renaming to ensure atomic and durablility


How? Suppose out db.txt contains our data. Initilialy it has 
```
1,hi
2,bye
3,hello
```
When we want to insert a new entry, say "4, yo" (Key, value), 
we do these
(1) read db.txt
(2) copy to db.tmp
(3) add "4, yo" to db.tmp
(4) then rename db.tmp to db.txt


So at any point in this operation if the system does fail, we would ensure no data corruption. 

But theres a bit more to this. We cant just depend on writing. We have to do fsync. or else it could be our write instruction went to RAM but not on disc, failure in there means we are screwed. fsync would flush buffers. whatever that means

But actually instead of doing a direct insert, delete, we will store logs. Append only logs. so we will have our db.txt that looks like this 
```
INSERT 1 hi
DELETE 1
INSERT 1 bye
DELETE 1
INSERT 1 hello
```

And reading would mean parsing entire file. Thats alright for our step 0.
