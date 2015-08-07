//===- SimpleArray.h -------------------------------------------*- C++ --*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License.  See LICENSE.TXT in the LLVM repository for details.
//
//===----------------------------------------------------------------------===//

#ifndef OHMU_BASE_SIMPLEARRAY_H
#define OHMU_BASE_SIMPLEARRAY_H

#include "MemRegion.h"


namespace ohmu {

// A simple fixed size array class that does not manage its own memory,
// suitable for use with bump pointer allocation.
template <class T> class SimpleArray {
public:
  SimpleArray() : Data(nullptr), Size(0), Capacity(0) {}
  SimpleArray(T *Dat, size_t Cp, size_t Sz = 0)
      : Data(Dat), Size(Sz), Capacity(Cp) {}
  SimpleArray(MemRegionRef A, size_t Cp)
      : Data(Cp == 0 ? nullptr : A.allocateT<T>(Cp)), Size(0), Capacity(Cp) {}
  SimpleArray(SimpleArray<T> &&A)
      : Data(A.Data), Size(A.Size), Capacity(A.Capacity) {
    A.Data = nullptr;
    A.Size = 0;
    A.Capacity = 0;
  }

  SimpleArray &operator=(SimpleArray &&RHS) {
    if (this != &RHS) {
      Data = RHS.Data;
      Size = RHS.Size;
      Capacity = RHS.Capacity;

      RHS.Data = nullptr;
      RHS.Size = RHS.Capacity = 0;
    }
    return *this;
  }

  /// Reserve space for at least Ncp items, reallocating if necessary.
  void reserve(size_t Ncp, MemRegionRef A) {
    if (Ncp <= Capacity)
      return;
    T *Odata = Data;
    Data = A.allocateT<T>(Ncp);
    Capacity = Ncp;
    memcpy(Data, Odata, sizeof(T) * Size);
    return;
  }

  /// Resize to Nsz, initializing newly-added elements to V
  void resize(size_t Nsz, MemRegionRef A, const T& V) {
    if (Nsz <= Size)
      return;
    reserve(Nsz, A);
    for (size_t i = Size; i < Nsz; ++i)
      Data[i] = V;
    Size = Nsz;
  }

  /// Reserve space for at least N more items.
  void reserveCheck(size_t N, MemRegionRef A) {
    if (Capacity == 0)
      reserve(u_max(InitialCapacity, N), A);
    else if (Size + N > Capacity)
      reserve(u_max(Size + N, Capacity * 2), A);
  }

  typedef T *iterator;
  typedef const T *const_iterator;

  size_t size() const { return Size; }
  size_t capacity() const { return Capacity; }

  T &operator[](unsigned i) {
    assert(i < Size && "Array index out of bounds.");
    return Data[i];
  }
  const T &operator[](unsigned i) const {
    assert(i < Size && "Array index out of bounds.");
    return Data[i];
  }
  T &back() {
    assert(Size && "No elements in the array.");
    return Data[Size - 1];
  }
  const T &back() const {
    assert(Size && "No elements in the array.");
    return Data[Size - 1];
  }

  iterator begin() { return Data; }
  iterator end()   { return Data + Size; }

  const_iterator begin() const { return Data; }
  const_iterator end()   const { return Data + Size; }

  const_iterator cbegin() const { return Data; }
  const_iterator cend()   const { return Data + Size; }

  void push_back(const T &Elem) {
    assert(Size < Capacity);
    Data[Size++] = Elem;
  }

  template<typename... Params>
  void emplace_back(Params... Ps) {
    assert(Size < Capacity);
    new (&Data[Size++]) T(Ps...);
  }

  /// drop last n elements from array
  void drop(unsigned n = 0) {
    assert(Size > n);
    Size -= n;
  }

  /// drop all elements from array.
  void clear() {
    Size = 0;
  }

  void setValues(unsigned Sz, const T& C) {
    assert(Sz <= Capacity);
    Size = Sz;
    for (unsigned i = 0; i < Sz; ++i) {
      Data[i] = C;
    }
  }

  template <class Iter> unsigned append(Iter I, Iter E) {
    size_t Osz = Size;
    size_t J = Osz;
    for (; J < Capacity && I != E; ++J, ++I)
      Data[J] = *I;
    Size = J;
    return J - Osz;
  }

  // An adaptor to reverse a simple array
  class ReverseAdaptor {
   public:
    ReverseAdaptor(SimpleArray &Arr) : Array(Arr) {}
    // A reverse iterator used by the reverse adaptor
    class Iterator {
     public:
      Iterator(T *Dat) : Data(Dat) {}
      T &operator*() { return *Data; }
      const T &operator*() const { return *Data; }
      Iterator &operator++() {
        --Data;
        return *this;
      }
      bool operator!=(Iterator Other) { return Data != Other.Data; }

     private:
      T *Data;
    };
    Iterator begin() { return Array.end() - 1; }
    Iterator end() { return Array.begin() - 1; }
    const Iterator begin() const { return Array.end() - 1; }
    const Iterator end() const { return Array.begin() - 1; }

   private:
    SimpleArray &Array;
  };

  const ReverseAdaptor reverse() const { return ReverseAdaptor(*this); }
  ReverseAdaptor reverse() { return ReverseAdaptor(*this); }

private:
  // std::max is annoying here, because it requires a reference,
  // thus forcing InitialCapacity to be initialized outside the .h file.
  size_t u_max(size_t i, size_t j) { return (i < j) ? j : i; }

  static const size_t InitialCapacity = 4;

  SimpleArray(const SimpleArray<T> &A) = delete;

  T *Data;
  size_t Size;
  size_t Capacity;
};

}  // end namespace ohmu

#endif  // OHMU_BASE_SIMPLEARRAY_H
