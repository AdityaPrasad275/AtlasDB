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