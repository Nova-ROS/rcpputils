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

#ifndef RCPPUTILS__CLASS_LOADER_HPP_
#define RCPPUTILS__CLASS_LOADER_HPP_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <any>

#include "rcpputils/visibility_control.hpp"
#include "rcpputils/shared_library.hpp"

namespace rcpputils
{
namespace class_loader
{

/**
 * \brief Type alias for factory creation function.
 */
template <typename Base>
using CreateClassObjFunc = std::function<Base * ()>;

/**
 * \brief Type alias for factory destruction function.
 */
template <typename Base>
using DeleteClassObjFunc = std::function<void (Base *)>;

std::unordered_map<std::type_index, std::any>& get_global_factory_registry();

/**
 * \brief Get the global factory map for a base class.
 * \return Reference to the map of class names to factory functions.
 */
template <typename Base>
RCPPUTILS_PUBLIC
auto& get_factory_map_for_base_class() {
  using MapT = std::map<std::string, std::pair<CreateClassObjFunc<Base>, DeleteClassObjFunc<Base>>>;
  auto& reg = get_global_factory_registry();
  auto key = std::type_index(typeid(Base));

  if (reg.find(key) == reg.end()) {
      reg.emplace(key, MapT{});
  }

  return std::any_cast<MapT&>(reg.at(key));
}

/**
 * \brief Register a class in the factory map.
 * \param[in] class_name The name of the class.
 * \param[in] create_func Creation function.
 * \param[in] delete_func Destruction function.
 */
template <typename Base>
void register_class(
  const std::string & class_name,
  CreateClassObjFunc<Base> create_func,
  DeleteClassObjFunc<Base> delete_func)
{
  auto & factory_map = get_factory_map_for_base_class<Base>();
  factory_map[class_name] = {create_func, delete_func};
}

/**
 * \brief Class for loading and unloading shared libraries dynamically.
 */
class RCPPUTILS_PUBLIC ClassLoader
{
public:
  /**
   * \brief Constructor.
   * \param[in] library_path Path to the shared library.
   * \throw LibraryLoadException if loading fails.
   */
  explicit ClassLoader(const std::string & library_path);

  /**
   * \brief Destructor, unloads the library.
   */
  ~ClassLoader();

  /**
   * \brief Get available classes derived from Base.
   * \return Vector of class names.
   */
  template <typename Base>
  std::vector<std::string> get_available_classes() const;

  /**
   * \brief Create a shared instance of a class.
   * \param[in] class_name Name of the class to instantiate.
   * \return Shared pointer to the instance.
   * \throw std runtime exception if creation fails.
   */
  template <typename Base>
  std::shared_ptr<Base> create_shared_instance(const std::string & class_name) const;

  /**
   * \brief Create a unique instance of a class.
   * \param[in] class_name Name of the class to instantiate.
   * \return Shared pointer to the instance.
   * \throw std runtime exception if creation fails.
   */
  template <typename Base>
  std::unique_ptr<Base> create_unique_instance(const std::string & class_name) const;

  /**
   * \brief Check if the library is loaded.
   * \return True if loaded.
   */
  bool is_library_loaded() const;

protected:
  /**
   * \brief Load the library manually.
   * \throw LibraryLoadException if loading fails.
   */
  void load_library();

  /**
   * \brief Unload the library manually.
   * \throw LibraryLoadException if unloading fails.
   */
  void unload_library();

private:
  std::shared_ptr<rcpputils::SharedLibrary> library_handle_;
  std::string library_path_;
};

template <typename Base>
std::vector<std::string> ClassLoader::get_available_classes() const
{
  if (!is_library_loaded()) {
    throw std::runtime_error("Library" + library_path_ + "not loaded");
  }
  auto & factory_map = get_factory_map_for_base_class<Base>();

  std::vector<std::string> classes;
  for (const auto & pair : factory_map) {
    classes.push_back(pair.first);
  }
  return classes;
}

template <typename Base>
std::shared_ptr<Base> ClassLoader::create_shared_instance(const std::string & class_name) const
{
  if (!is_library_loaded()) {
    throw std::runtime_error("Library" + library_path_ + "not loaded");
  }
  auto & factory_map = get_factory_map_for_base_class<Base>();
  auto it = factory_map.find(class_name);
  if (it == factory_map.end()) {
    throw std::runtime_error("Class not registered: " + class_name);
  }
  Base * obj = it->second.first();
  return std::shared_ptr<Base>(obj, it->second.second);
}

template <typename Base>
std::unique_ptr<Base> ClassLoader::create_unique_instance(const std::string & class_name) const
{
  auto shared = create_shared_instance<Base>(class_name);
  return std::unique_ptr<Base>(shared.release());
}

}  // namespace class_loader
}  // namespace rcpputils

#endif  // RCPPUTILS__CLASS_LOADER_HPP_
