# thread_manager

This library provides tools for the user to manage threads with.
When the user wants to create a thread, it will store the context of the thread and its id in a linked list data structure. 
It will then swap to that context and run the function the user wants to run.
The library provides a function to switch to the next runnable thread when the user wants or when an interrupt is fired.
The library will store the result of the thread function in the thread_list data structure for it to be referred to later.
The library will provide tools to make functions atomic. It will keep track of all available locks in an array of integers. 
The lock array is as large as the number of locks the user wants.
When a lock is available, that lock is set to a '-1' indicating it is available. 
When it is unavailable, the index of the array that the lock is stored is set to the thread ID of the thread using it.
If a thread requires a lock that is unavailable, it can get access to it using a conditional variable. 
These variables are stored in another array of size NUM_LOCKS * NUM_CONDITIONS. 
Each condition is set to '0' when they have not been requested. 
When a thread wants a lock and passes in a condition, that condition is set to '1' and the corresponding lock becomes available.
