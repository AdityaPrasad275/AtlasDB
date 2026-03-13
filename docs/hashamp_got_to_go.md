# Hashmap you gotta go

In step 0, we created a fairly durable, kinda atomic database. With db.log acting as our well log. we write to it everything we, every insert, every delete. But our database, in RAM, is actually just a hashmap. We use our log, our Write Ahead log policy, as a crash recovery option. But out primary database in memory was just a hashmap, a key value store.

But how do you support a database 100 GB big with a hashmap? You cant have that much RAM. 

So you see, you're dealing with having to provide 100 GB database, But we've only got say 1 gb ram. or whatever.

So we dive into memory management!

# Pages

For the time being, forget a little bit abt actual data. Actual kv values, forget it. We'll only talk abt how we move data from disk to ram and back.

And here's a mental image to understand whats going on. Imagine a page. It stores data. Dont worry abt how ti stores it, what is the format, how data is in inside the page. dont worry abt that rn. Just imagine you got a page/sheet of data. And your disk is like a stack of million pages. Say this comprises of your 100 gb of data. 

Amazing. Now you are the chief, the captain, that wants to interact with that data. You've got your desk, on which you've got "slots". or transparent folders. say you've got 10 of those. what this means is at a moment, you can only read/access atmost 10 pages/sheets. This desk is your ram. The stack of sheets was your disk. Your job as a chief is to go to and fro from desk and stack. ram and disk. 

Don't worry too much abt "how do i tell from a million pages, where does my data lies". Worry abt a much sharper question -> I want to see page #132. All my folders are busy, I have to figure out which folder to empty out from my desk, back onto stack of sheets (from ram to disk), i have to take page #132 and put it into that folder. 

To take from non technical story a little bit more technical, you, the chief, the captain are the BufferPoolManager. A class whose job is to organise the folders, the pages. Your arms that empty out a folder, put some page from stack to folder is the DIskManager. A class whose responbility is to read and write from disk. And the pages are, for now, just containers. The class Page is just a trying to define a white blank page. actual data in it, how its organised, how structured, comes later.

We build the infra first, add in details later. 

# A little more technical

Here I'll only convery the API contract of each classes, basically explaining their header files, actual implementation I'll deeper into in their own docs/blogs

We go bottoms up. Before we figure out how to manager RAM to disk, we have to first build our universe's atom, the pages

## Page

At its most basic, it is just some meta data and actual data. meta data includes page_id, pin_count, is_dirty (something we'll dive deeper into later). Data is just a array of bytes. one after the other.  
Now heres an interesting thing, on the OS level, whenever disk writes actually happen, they happen in bunches of 4KB. because disk writes are really slow. around a 100,000 times slower than RAM functions. So the OS bunches up a lot of writes and does them in one go. So it makes sense to make our page be 4 KB large as well. one page can be written in one go. Alignment with the OS. 
 
So we define a `PAGE_SIZE` variable equal to 4096. And our data, the array of bites can be defined as `char data[PAGE_SIZE]`. 

And thats practically it. give some getters and setters, and its all done! That's why you'll see theres a Page.h but no Page.cpp, dont really need it. Page is just a simple container, with some meta data and data!

Here's what it looks like in code

```cpp
#pragma once

#include <cstring> // for memset
#include "type.h"

class Page{
public:
    // Global rules for DB, DiskManager and BufferPoolManager needs them
    static constexpr int PAGE_SIZE = 4096;
    static constexpr int INVALID_PAGE_ID = -1;

    Page() { resetMemory(); }
    ~Page() = default; 

    // Accessors
    char* getData() { return _data; } 
    page_id_t getPageId() { return _page_id; }
    int getPinCount() { return _pin_count; } 
    bool isDirty() { return _is_dirty; }
    void setDirty(bool is_dirty = true) { _is_dirty = is_dirty; }

    // BufferManager uses this
    void pin() { _pin_count++; } 
    void unpin() { if (_pin_count > 0) _pin_count--; }

    void resetMemory() {
        std::memset(_data, 0, PAGE_SIZE); // sets entire _data to 0
        _page_id = INVALID_PAGE_ID;
        _pin_count = 0;
        _is_dirty = false;
        _lsn = 0; 
    }

    friend class BufferPoolManager; // so bufferpoolmanager can get access to private members

private:
    char _data[PAGE_SIZE]; // our actual data

    // meta data
    page_id_t _page_id;
    int _pin_count;
    bool _is_dirty;
    lsn_t _lsn;
    // lsn is for log recovery. stores the id of last log entry that modified this page 

};
```

Now that we've built the universe's atom, lets figure out how we move them

## DiskManager

The diskmanager is supposed to do only two things, read from disk and write to disk.
Now lets understand how our disk actually is structured.

Our `atlas.db` file serves as our disk. A contigous block of memory. We partition it in `PAGE_SIZE` blocks. Every `PAGE_SIZE` block stores the data of our page. So you only need page_id to get to that block, basically from beginning `0` to `0+ page_id*PAGE_SIZE` , then read next block as our page data.

Doubt, if our atlast.db is partitioned in page_size blocks, and we are only storing data in it, where are we storing the page_id, and pin_count and meta data stuff? I have to confirm this but my theory is, the meta data is something controlled by BufferPoolManager. what i mean is our atlas.db is continous block of memory. its got data upon data. But the page object, is actually a sort of like a sticker on top of a page. We only initialise page object in BufferPoolManager, where we control page_id and stuff. hmm i am not sure tho. Because like we got from page-id 0 to 1 million. sure we can store that data in atlas.db, getting data is simply a pointer thing, but where we getting somethign like the lsn or the idk other meta data for page_id = 300,000. idk. gotta figure this out.

So a disk manager is pretty simple, give it a page_id, it reads data for that block, or writes data to that block. Implementation is a little ardous as you have to deal with std errors with write and read and stuff, file descriptors, bla bla. but very doable

Here's what it looks like in code

```cpp
#pragma once

#include<string>
#include "type.h"

class DiskManager{
public:
    DiskManager(const std::string& fileName); // assigns _fd to filename
    ~DiskManager();  

    void writePage(page_id_t page_id, const char* data); // Jumps to page_id * 4096 in the file and writes exactly 4096 bytes.

    void readPage(page_id_t page_id, char* data); //Jumps to page_id * 4096 in the file and reads exactly 4096 bytes into the buffer.

    void shutDown(); // closes the file safely

private:
    int _fd; // file descriptor
};
```

Now that we have arms to move the atoms, now we need a brain to figure it all out.

## BufferPoolManager

Now we are building the brains that figures out which pages belong in disk, what on RAM, when to move in between. So it's a little more complicated, but one by one we'll figure it out

### The state / variables

First lets understand what our state looks like
1. Remember our analogy of a desk with folders? These are called frames. we decide how many frames we want to manage at a time, by `pool_size` variable. 
2. Our frames are just an array of Page objects. After all, our frames store pages.
3. Each frame gets a frame_id. each frame stores a page with page_id. going from frame_id to page_id, very simple. But reverse is hard, we wanna do it fast, we keep a map `page_table` for it. Hashmap you're back! 
4. We wanna store a list of free_frames, a simple vector<int>
5. Now when frames our fully occupied and we wanna load another page from disk. How do we decide which frame to empty? LRU cache! Finally somethign from leetcode actually shows up in real world lol. We'll design a LRU cache seperatly for this as well. For now just understand this gives a frame to be evicted. 
6. When we wanna write brand new data, we wanna keep track of `next_page_id`. the next "free space" in disk. Wait a second, in power cut how do we rebuild this one. interesting. idk.

Pretty big state, but we basically cover everything. (A little more abt mutex and latches to ensure thread safety too but i dont really understand what that is sooooo ... woops)

### The methods

We're dealing with moving pages from and to ram and disk, so we wanna fetch a page from disk put into a frame, we wanna write a page to disk from a frame, delete a page, flush frames clean. And a little smart method dealing with durability. 

On that lets talk abt pin_count and is_dirty meta data we postponed discussion in Page class. 

pin_count is the number of threads interacting with this page. if > 0, a (or more) thread is working on this page we simply cannot flush this page, data loss might happen. 

is_dirty is a flag that marks whether a page's data has been changed when in ram. basically, when a layer above this bufferPoolManager, the user, deals with actually modifying data, we dont always flush to disk. That will be slow. So we just keep the page in RAM, mark it is_dirty. when later we want the frame that has this page to be empty, we wanna know that before we evict this page from frame, we should write changed data back to disk. 

Now we can talk abt methods

1. `fetchPage`: pretty simple, standard. takes in a page_id, if already in ram, returns the page pointer. if not, reads from disk. Well its a little bit trickier than that. Because sure you know which page to read, but how do you know what frame to put that page in. Queue in lru cache, free frames list, amazing implementation details explained in BufferPoolManager's own blog
2. `newPage`: make a brand new emtpy page. here's where next_page_id is important. next_page_id dictates where in disk the page eventually gets stored in, but in RAM we still need to deal with finding an empty frame and stuff.
3. `deletePage`: now in this implementation, we've only focused on clearing the frame that contains the page. and now actually clearing db. 
4. `flush` and `flushAll`: In flushing, we gotta take modified data, pages that are dirty, flush their data to disk and empty frames. flush given a page_id, or flush all frames that are dirty
5. `unpinPage`: Now this is actually a user's method/responsibility that when they are done with a page (they fetched), they unpin it. fetching a page increments its pin count. denoting a thread is working on it. dont flush it. by unpunning we can make frames avaialbe to be flushed, cleared, and fetch new pages
6. `findVictim`: an internal function that figures out which frame is empty/ready to be filled with a new page

And that's basically it! This is how it looks like in code

```cpp
#include<unordered_map>
#include <list>
#include <mutex>

#include "DiskManager.h"
#include "Page.h"
#include "type.h"
#include "LRUReplacer.h"

class BufferPoolManager {
public:
    BufferPoolManager(size_t pool_size, DiskManager *disk_manager);
    ~BufferPoolManager();

    Page* fetchPage(page_id_t page_id); //"I need Page #5." If it's in RAM, return it. If not, find a free slot, read it from disk, and return it. Increments pin_count

    bool unpinPage(page_id_t page_id, bool is_dirty); //"I'm done with Page #5." decerements page_count, if is_dirty, we remerr to flush to disk later

    Page* newPage(page_id_t &page_id); //Give me a brand-new, empty page." The BPM allocates a new page_id, finds a slot in RAM, and clears it.
     
    bool deletePage(page_id_t page_id); // delete page from file

    bool flushPage(page_id_t page_id); // "Force Page #5 to disk right now." (Usually used for WAL/Logging).

    void flushAllPages(); // clear everything

private:
    size_t _pool_size;
    Page* _frames; // frames is array of pages

    DiskManager* _disk_manager; // writing and reading pages

    std::unordered_map<page_id_t, frame_id_t> _page_table;// which page belongs to which frame

    LRUReplacer* _replacer; // which page to evict
    std::list<frame_id_t> _free_frames_list;

    
    std::mutex _latch;
    
    page_id_t _next_page_id;
    
    // Find whcih frame to use (either from free frames or evict using lru replacer)
    bool _findVictim(frame_id_t *frame_id);

};
```

## LRU cache : BufferPoolsManager's friend

We got a limited number of frames, but so many pages. How do we figure out which to evict? We figure out which one is the last used. wait what is the full form of LRU. least resued? idk. 

We implement it in a standard way, we got a doubly linked list, our actual cache. We got a hashmap, mapping frame_id to list nodes for super fast access. Because list access is O(1)

We want to
1. add a frame to lru cache. when we unpin a page, we can add that frame (which contains that frame) to the front of our LRU cache. Hottest in front
2. Remove a frame. when we fetch a page, we increment pin_count of that page, denoting it is being worked on. so this frame must not be cleared out. 
3. Get a victim frame. The back of our list has the coldest frames. 

Here's how it looks like in code

```cpp
#include<list>
#include<unordered_map>
#include<mutex>

#include "type.h"

class LRUReplacer {
private:
    std::list<frame_id_t> _lru_list; //doubly linked list
    // front is the hottest, page just touched
    // back is coldest, havent touched in a long time
    std::unordered_map<frame_id_t, list_iterator> _lru_map;

    std::mutex _latch;

public:
    // what do we do with this
    LRUReplacer();
    ~LRUReplacer();

    void unpin(frame_id_t frame_id);// add frame_id to front , frame just got unpined? better to leave it wheever it is ?

    void pin(frame_id_t frame_id); // remove frame from list

    bool victim(frame_id_t* frame_id); // get the back of list

    int size();
};
```

# Hashmap you will be missed

And that covers our hashmap replacement. well not entirely. we havent sovled what actually goes inside a page, but we have figured out how to deal with limited RAM and basically infinite disk. durability is kind of weakly maintained here. that can be strengthened using Write ahead log, checkusms and stuff. that we will implement next time! 

