// Copyright 2026 Nova ROS, Inc.
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

#include "rcpputils/class_loader.hpp"

#include <map>
#include <mutex>

namespace rcpputils
{
namespace class_loader
{

// Intentionally leaked to avoid static destruction order issues.
impl::FactoryMapStorage & impl::FactoryMapStorage::instance()
{
  static auto * storage = new FactoryMapStorage();
  return *storage;
}

// We keep a global cache of loaded libraries so that:
// 1. Multiple ClassLoader instances opening the same path share one handle.
// 2. The library is never truly unloaded (dlclose'd) while factory functions
//    registered from it still exist in the FactoryMapStorage.
//
// The cache maps canonical library paths to weak_ptr<SharedLibrary>.
// When the last ClassLoader for a path is destroyed the weak_ptr expires,
// but the .so stays loaded (dlopen refcount > 0) because we keep a
// permanent shared_ptr in a secondary set when factories were registered.
namespace
{
std::mutex & library_cache_mutex()
{
  static auto * mtx = new std::mutex();
  return *mtx;
}

// Libraries are kept alive permanently (shared_ptr, intentionally leaked map)
// because plugin factories registered in FactoryMapStorage hold std::function
// objects whose code lives in the .so.  If the .so were ever dlclose'd those
// function pointers would dangle.
std::map<std::string, std::shared_ptr<rcpputils::SharedLibrary>> & library_cache()
{
  static auto * cache =
    new std::map<std::string, std::shared_ptr<rcpputils::SharedLibrary>>();
  return *cache;
}
}  // namespace

ClassLoader::ClassLoader(const std::string & library_path)
: library_handle_(nullptr), library_path_(library_path)
{
  load_library();
}

ClassLoader::~ClassLoader()
{
  library_handle_.reset();
}

void ClassLoader::load_library()
{
  if (library_handle_) {
    return;
  }

  std::lock_guard<std::mutex> lock(library_cache_mutex());

  // Check whether this library is already loaded
  auto & cache = library_cache();
  auto it = cache.find(library_path_);
  if (it != cache.end() && it->second) {
    library_handle_ = it->second;
    return;
  }

  // Load a fresh copy
  try {
    library_handle_ = std::make_shared<rcpputils::SharedLibrary>(library_path_);
  } catch (const std::exception & e) {
    throw ClassLoaderException(
      "Failed to load library '" + library_path_ + "': " + e.what());
  }

  cache[library_path_] = library_handle_;
}

void ClassLoader::unload_library()
{
  library_handle_.reset();
}

bool ClassLoader::is_library_loaded() const
{
  return library_handle_ != nullptr;
}

const std::string & ClassLoader::get_library_path() const
{
  return library_path_;
}

}  // namespace class_loader
}  // namespace rcpputils
