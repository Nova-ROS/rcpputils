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

#include <sstream>

namespace rcpputils
{
namespace class_loader
{

std::unordered_map<std::type_index, std::any>& get_global_factory_registry() {
    static std::unordered_map<std::type_index, std::any> registry;
    return registry;
}

ClassLoader::ClassLoader(const std::string & library_path)
  : library_handle_(nullptr), library_path_(library_path)
{
  load_library();
}

ClassLoader::~ClassLoader()
{
  if (is_library_loaded()) {
    unload_library();
  }
}

void ClassLoader::load_library()
{
  if (library_handle_) {
    return;
  }

  library_handle_ = std::make_shared<rcpputils::SharedLibrary>(library_path_);
}

void ClassLoader::unload_library()
{
  library_handle_->unload_library();
  library_handle_ = nullptr;
}

bool ClassLoader::is_library_loaded() const
{
  return library_handle_ != nullptr;
}

}  // namespace class_loader
}  // namespace rcpputils
