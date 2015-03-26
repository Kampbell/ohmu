//===- DiagnosticEmitter.h -------------------------------------*- C++ --*-===//
// Copyright 2014  Google
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
// Provide a simple class for emitting error and warning messages.
//
//===----------------------------------------------------------------------===//

#ifndef OHMU_TIL_DIAGNOSTIC_H
#define OHMU_TIL_DIAGNOSTIC_H

#include "LLVMDependencies.h"

#include <ostream>

namespace ohmu {


/// Wraps a std::ostream to provide custom output for various things.
class DiagnosticStream {
public:
  DiagnosticStream(std::ostream& s) : ss(s) { }
  ~DiagnosticStream() {
    ss << "\n";
  }

  DiagnosticStream& operator<<(bool b) {
    if (b) ss << "true";
    else   ss << "false";
    return *this;
  }

  DiagnosticStream& operator<<(int i) {
    ss << i;
    return *this;
  }

  DiagnosticStream& operator<<(unsigned i) {
    ss << i;
    return *this;
  }

  DiagnosticStream& operator<<(const char* msg) {
    ss << msg;
    return *this;
  }

  DiagnosticStream& operator<<(StringRef msg) {
    ss << msg.c_str();
    return *this;
  }

  std::ostream& outputStream() { return ss; }

public:
  std::ostream& ss;
};



/// This file defines a simple interface for publishing warnings and errors.
/// The current methods are placeholders; they will be improved in the future.
class DiagnosticEmitter {
public:
  DiagnosticEmitter() : dstream_(std::cerr) { }

  DiagnosticStream& error(const char* msg) {
    dstream_ << "\nerror: " << msg;
    return dstream_;
  }

  DiagnosticStream& warning(const char* msg) {
    dstream_ << "\nwarning: " << msg;
    return dstream_;
  }

private:
  DiagnosticStream dstream_;
};


}  // end namespace ohmu

#endif  // OHMU_TIL_DIAGNOSTIC_H
