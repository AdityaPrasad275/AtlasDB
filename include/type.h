#include<list>

// using a typedef (makes it easy to switch later to say 64 bit int)
using page_id_t = int;
using lsn_t = int;
using frame_id_t = int;
using list_iterator = std::list<frame_id_t>::iterator;