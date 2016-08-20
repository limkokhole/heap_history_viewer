#ifndef HEAPHISTORY_H
#define HEAPHISTORY_H

#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

#include <QVector3D>

#include "heapblock.h"
#include "vertex.h"

class HeapWindow {
public:
  HeapWindow(uint64_t min, uint64_t max, uint32_t mintick, uint32_t maxtick);
  uint64_t height() { return maximum_address_ - minimum_address_; }
  uint32_t width() { return maximum_tick_ - minimum_tick_; }
  void reset(const HeapWindow &window) { *this = window; }
  uint64_t minimum_address_;
  uint64_t maximum_address_;
  uint32_t minimum_tick_;
  uint32_t maximum_tick_;
};

// Adds a double to a uint64_t or uint32_t, saturating the uint at max or at 0
// (so no integer overflow). Returns 0 if no overflow would have occured, 1 if
// an overflow has occurred, and -1 if an underflow has occured.
template <typename T> int32_t saturatingAddition(double value, T *result) {
  if ((value > 0) && (value > (std::numeric_limits<T>::max() - *result))) {
    // Addition results in overflow.
    *result = std::numeric_limits<T>::max();
    return 1;
  }
  if ((value < 0) && (fabs(value) > *result)) {
    // Addition would cause underflow.
    *result = 0;
    return -1;
  }
  *result += value;
  return 0;
}

class ContinuousHeapWindow {
public:
  ContinuousHeapWindow() {}
  ContinuousHeapWindow(uint64_t min, uint64_t max, uint32_t mintick,
                       uint32_t maxtick);
  double height() { return maximum_address_ - minimum_address_; }
  double width() { return maximum_tick_ - minimum_tick_; }
  void reset(const HeapWindow &window) {
    minimum_address_ = window.minimum_address_;
    maximum_address_ = window.maximum_address_;
    minimum_tick_ = window.minimum_tick_;
    maximum_tick_ = window.maximum_tick_;
  }
  uint64_t getMinimumAddress() const { return minimum_address_; }
  uint32_t getMinimumAddressLow32() const {
    return static_cast<uint32_t>(minimum_address_);
  }
  uint32_t getMinimumAddressHigh32() const {
    return static_cast<uint32_t>(minimum_address_ >> 32);
  }
  uint64_t getMaximumAddress() const { return maximum_address_; }
  uint32_t getMinimumTick() const { return minimum_tick_; }
  uint32_t getMaximumTick() const { return maximum_tick_; }
  double getMinimumAddressAsDouble() const {
    return static_cast<double>(minimum_address_);
  }
  double getMaximumAddressAsDouble() const {
    return static_cast<double>(maximum_address_);
  }
  double getMinimumTickAsDouble() const {
    return static_cast<double>(minimum_tick_);
  }
  double getMaximumTickAsDouble() const {
    return static_cast<double>(maximum_tick_);
  }

  // This code is made uglier by dealing with possible integer overflows.
  void pan(double dx, double dy);

  template <typename T>
  T saturatedAdd(T value, double addend, T upperlimit, T lowerlimit) {
    if (addend > 0) {
      if (addend > upperlimit - value) {
        return upperlimit;
      }
      return value + addend;
    }
    if (addend < 0) {
      if (fabs(addend) > value - lowerlimit) {
        return lowerlimit;
      }
      return value - fabs(addend);
    }
  }

  // Zoom toward a given point on the screen. The point is given in relative
  // height / width of the current window, e.g. the center is 0.5, 0.5.
  void zoomToPoint(double dx, double dy, double how_much_x, double how_much_y);

private:
  uint64_t minimum_address_;
  uint64_t maximum_address_;
  uint32_t minimum_tick_;
  uint32_t maximum_tick_;
  float x_shift_;
  float y_shift_;
};

class HeapConflict {
public:
  HeapConflict(uint32_t tick, uint64_t address, bool alloc);
  uint32_t tick_;
  uint64_t address_;
  bool allocation_or_free_;
};

class HeapHistory {
public:
  HeapHistory();
  size_t
  getActiveBlocks(std::vector<std::vector<HeapBlock>::iterator> *active_blocks);
  void setCurrentWindow(const HeapWindow &new_window);
  void setCurrentWindowToGlobal() { current_window_.reset(global_area_); }
  const ContinuousHeapWindow &getCurrentWindow() const {
    return current_window_;
  }
  const ContinuousHeapWindow &getGridWindow(uint32_t number_of_lines);

  // Input reading.
  void LoadFromJSONStream(std::istream &jsondata);

  // Attempts to find a block at a given address and tick. Currently broken,
  // hence the two implementations (slow works).
  // TODO(thomasdullien): Debug and fix the fast version.
  bool getBlockAt(uint64_t address, uint32_t tick, HeapBlock *result,
                  uint32_t *index);
  bool getBlockAtSlow(uint64_t address, uint32_t tick, HeapBlock *result,
                      uint32_t *index);

  // Record a memory allocation event. The code supports up to 256 different
  // heaps.
  void recordMalloc(uint64_t address, size_t size, uint8_t heap_id = 0);
  void recordFree(uint64_t address, uint8_t heap_id = 0);
  void recordRealloc(uint64_t old_address, uint64_t new_address, size_t size,
                     uint8_t heap_id);

  // Dump out triangles for the current window of heap events.
  size_t dumpVerticesForActiveWindow(std::vector<HeapVertex> *vertices);
  uint64_t getMinimumAddress() { return global_area_.minimum_address_; }
  uint64_t getMaximumAddress() { return global_area_.maximum_address_; }
  uint32_t getMinimumTick() { return global_area_.minimum_tick_; }
  uint32_t getMaximumTick() { return global_area_.maximum_tick_; }

  // Functions for moving the currently visible window around.
  void panCurrentWindow(double dx, double dy);
  void zoomToPoint(double dx, double dy, double how_much_x, double how_much_y);

private:
  void recordMallocConflict(uint64_t address, size_t size, uint8_t heap_id);
  void recordFreeConflict(uint64_t address, uint8_t heap_id);
  bool isBlockActive(const HeapBlock &block);
  // Dumps 6 vertices for 2 triangles for a block into the output vector.
  // TODO(thomasdullien): Optimize this to only dump 4 vertices?
  void HeapBlockToVertices(const HeapBlock &block,
                           std::vector<HeapVertex> *vertices);
  // When a new block has been put into the vector, this function needs to be
  // called to update the internal data structures for fast block search.
  void updateCachedSortedIterators();

  std::vector<std::vector<HeapBlock>::iterator>
      cached_blocks_sorted_by_address_;
  // Running counter to keep track of heap events.
  uint32_t current_tick_;
  // The currently active (visible, to-be-displayed) part of the heap history.
  ContinuousHeapWindow current_window_;
  // The rectangle for the grid drawing.
  ContinuousHeapWindow grid_rectangle_;

  // The global size of all heap events.
  HeapWindow global_area_;
  // The vector of all heap blocks. This vector will be sorted by the minimum
  // tick of their allocation.
  std::vector<HeapBlock> heap_blocks_;
  // A map to keep track of blocks that are "currently live".
  std::map<std::pair<uint64_t, uint8_t>, size_t> live_blocks_;
  // A vector of ticks that records the conflicts in heap logic.
  std::vector<HeapConflict> conflicts_;
};

#endif // HEAPHISTORY_H