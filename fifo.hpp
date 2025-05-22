#ifndef FIFO_HPP
#define FIFO_HPP

#include <stdint.h>
#include <stddef.h> // For size_t

// A simple FIFO (ring buffer) implementation
template <typename T, size_t size>
class FIFO {
public:
    FIFO() : head(0), tail(0), count(0) {}

    bool isEmpty() const {
        return count == 0;
    }

    bool isFull() const {
        return count == size;
    }

    size_t getCount() const {
        return count;
    }

    size_t getFreeSpace() const {
        return size - count;
    }

    // Add an item to the FIFO
    // Returns true if successful, false if FIFO is full
    bool put(const T& item) {
        if (isFull()) {
            return false; // FIFO is full
        }
        buffer[head] = item;
        head = (head + 1) % size;
        count++;
        return true;
    }

    // Get an item from the FIFO
    // Returns true if successful, false if FIFO is empty
    // The retrieved item is stored in 'item'
    bool get(T& item) {
        if (isEmpty()) {
            return false; // FIFO is empty
        }
        item = buffer[tail];
        tail = (tail + 1) % size;
        count--;
        return true;
    }

    // Peek at the next item to be retrieved without removing it
    // Returns true if successful, false if FIFO is empty
    bool peek(T& item) const {
        if (isEmpty()) {
            return false;
        }
        item = buffer[tail];
        return true;
    }

    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }

private:
    T buffer[size];
    size_t head; // Index of the next free slot
    size_t tail; // Index of the next item to retrieve
    size_t count; // Number of items in the FIFO
};

#endif // FIFO_HPP
