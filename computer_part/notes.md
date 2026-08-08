Circular buffer:
https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/

Before we proceed, we should take a moment to discuss the method we will use to determine whether or buffer is full or empty.

Both the “full” and “empty” cases of the circular buffer look the same: head and tail pointer are equal. There are two approaches to differentiating between full and empty:

    “Waste” a slot in the buffer:
        Full state is head + 1 == tail
        Empty state is head == tail
    Use a bool flag and additional logic to differentiate states::
        Full state is full
        Empty state is (head == tail) && !full
