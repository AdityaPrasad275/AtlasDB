What is a Mutex?
  In a database, multiple parts of the program will try to use the LRUReplacer at the same time.
   * Thread A might be Pin-ing a page.
   * Thread B might be Unpin-ing a different page.
   * Thread C might be calling Victim.

  If Thread A and Thread B both try to change the std::list at the same time, the pointers in the list will get messed up, and your program will segfault
  (crash).


  A Mutex (short for Mutual Exclusion) is like a "bathroom key."
   1. Before any thread touches the list, it must "Lock" the mutex (take the key).
   2. If another thread tries to lock it, it has to wait (block) until the key is returned.
   3. When the first thread is done, it "Unlocks" the mutex (returns the key).


  In database terminology, we often call a mutex a Latch. (We use "Lock" for high-level transaction stuff, and "Latch" for low-level memory structure
  protection like this).
 How to code it?
  We use std::mutex from the <mutex> header. Instead of manually calling lock() and unlock() (which is dangerous if you forget one), we use std::lock_guard.

  std::lock_guard is "RAII" (Resource Acquisition Is Initialization).
   * When you create it: It locks the mutex.
   * When it goes out of scope (at the end of the function }): It automatically unlocks it.
