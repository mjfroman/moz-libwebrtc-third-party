// Copyright 2014 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_COMPAT_MAC_MACH_O_LOADER_H_
#define CRASHPAD_COMPAT_MAC_MACH_O_LOADER_H_

#include_next <mach-o/loader.h>

// 10.7 SDK

#ifndef S_THREAD_LOCAL_ZEROFILL
#define S_THREAD_LOCAL_ZEROFILL 0x12
#endif

// 10.8 SDK

#ifndef LC_SOURCE_VERSION
#define LC_SOURCE_VERSION 0x2a
#endif

#endif  // CRASHPAD_COMPAT_MAC_MACH_O_LOADER_H_
