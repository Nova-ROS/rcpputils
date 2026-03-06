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

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "rcpputils/shared_library.hpp"
#include "rcpputils/visibility_control.hpp"

namespace rcpputils
{
namespace class_loader
{

/**
 * \brief Exception thrown when class loading operations fail.
 */
class ClassLoaderException : public std::runtime_error
{
public:
  explicit ClassLoaderException(const std::string & message)
  : std::runtime_error(message) {}
};

/**
 * \brief Type alias for factory creation function.
 */
template <typename Base>
using CreateClassObjFunc = std::function<std::unique_ptr<Base>()>;

/**
 * \brief Type alias for factory destruction function.
 */
template <typename Base>
using DeleteClassObjFunc = std::function<void(Base *)>;

/**
 * \brief Type alias for factory pair.
 */
template <typename Base>
using FactoryPair = std::pair<CreateClassObjFunc<Base>, DeleteClassObjFunc<Base>>;

/**
 * \brief Factory map for a specific base class.
 */
template <typename Base>
using FactoryMap = std::unordered_map<std::string, FactoryPair<Base>>;

namespace impl
{
/**
 * \brief Type-erased, thread-safe storage for factory maps.
 *
 * The singleton lives in librcpputils.so so that every translation unit
 * (including dynamically loaded plugin libraries) shares the same instance.
 */
class RCPPUTILS_PUBLIC FactoryMapStorage
{
public:
  /// Register a factory for a class name under the given Base type.
  template <typename Base>
  void register_factory(
    const std::string & class_name,
    CreateClassObjFunc<Base> create_func,
    DeleteClassObjFunc<Base> delete_func)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto & map = get_or_create_map_<Base>();
    map[class_name] = {std::move(create_func), std::move(delete_func)};
  }

  /// Unregister the factory for a class name under the given Base type.
  template <typename Base>
  void unregister_factory(const std::string & class_name)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto * map = find_map_<Base>();
    if (map) {
      map->erase(class_name);
    }
  }

  /// Check whether a class name is registered under the given Base type.
  template <typename Base>
  bool has_class(const std::string & class_name) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto * map = find_map_const_<Base>();
    if (!map) {
      return false;
    }
    return map->find(class_name) != map->end();
  }

  /// Return a snapshot (copy) of all registered class names for Base.
  template <typename Base>
  std::vector<std::string> get_registered_classes() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto * map = find_map_const_<Base>();
    if (!map) {
      return {};
    }
    std::vector<std::string> classes;
    classes.reserve(map->size());
    for (const auto & pair : *map) {
      classes.push_back(pair.first);
    }
    return classes;
  }

  /// Look up a factory pair.  Returns nullptr if not found.
  template <typename Base>
  const FactoryPair<Base> * find_factory(const std::string & class_name) const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto * map = find_map_const_<Base>();
    if (!map) {
      return nullptr;
    }
    auto it = map->find(class_name);
    if (it == map->end()) {
      return nullptr;
    }
    return &it->second;
  }

  /// Singleton accessor.  Defined in class_loader.cpp so that every shared
  /// library that links against librcpputils.so resolves to the same instance.
  static FactoryMapStorage & instance();

private:
  template <typename Base>
  FactoryMap<Base> & get_or_create_map_()
  {
    auto key = std::type_index(typeid(Base));
    auto it = storage_.find(key);
    if (it == storage_.end()) {
      auto pair = storage_.emplace(key, FactoryMap<Base>{});
      return std::any_cast<FactoryMap<Base> &>(pair.first->second);
    }
    return std::any_cast<FactoryMap<Base> &>(it->second);
  }

  template <typename Base>
  FactoryMap<Base> * find_map_()
  {
    auto key = std::type_index(typeid(Base));
    auto it = storage_.find(key);
    if (it == storage_.end()) {return nullptr;}
    try {
      return &std::any_cast<FactoryMap<Base> &>(it->second);
    } catch (const std::bad_any_cast &) {
      return nullptr;
    }
  }

  template <typename Base>
  const FactoryMap<Base> * find_map_const_() const
  {
    auto key = std::type_index(typeid(Base));
    auto it = storage_.find(key);
    if (it == storage_.end()) {return nullptr;}
    try {
      return &std::any_cast<const FactoryMap<Base> &>(it->second);
    } catch (const std::bad_any_cast &) {
      return nullptr;
    }
  }

  mutable std::mutex mutex_;
  std::unordered_map<std::type_index, std::any> storage_;
};
}  // namespace impl

/**
 * \brief Check if a class is registered.
 * \param[in] class_name The name of the class.
 * \return True if registered.
 * \note Thread-safe.
 */
template <typename Base>
bool is_class_registered(const std::string & class_name)
{
  return impl::FactoryMapStorage::instance().has_class<Base>(class_name);
}

/**
 * \brief Register a class in the factory map.
 * \param[in] class_name The name of the class.
 * \param[in] create_func Creation function.
 * \param[in] delete_func Destruction function.
 * \note Thread-safe.
 */
template <typename Base>
void register_class(
  const std::string & class_name,
  CreateClassObjFunc<Base> create_func,
  DeleteClassObjFunc<Base> delete_func)
{
  impl::FactoryMapStorage::instance().register_factory<Base>(
    class_name, std::move(create_func), std::move(delete_func));
}

/**
 * \brief Unregister a class from the factory map.
 * \param[in] class_name The name of the class.
 * \note Thread-safe.
 */
template <typename Base>
void unregister_class(const std::string & class_name)
{
  impl::FactoryMapStorage::instance().unregister_factory<Base>(class_name);
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
   * \throw ClassLoaderException if loading fails.
   */
  explicit ClassLoader(const std::string & library_path);

  /**
   * \brief Destructor, unloads the library.
   */
  ~ClassLoader();

  // Non-copyable
  ClassLoader(const ClassLoader &) = delete;
  ClassLoader & operator=(const ClassLoader &) = delete;

  // Movable
  ClassLoader(ClassLoader &&) noexcept = default;
  ClassLoader & operator=(ClassLoader &&) noexcept = default;

  /**
   * \brief Get available classes derived from Base.
   * \return Vector of class names.
   * \throw ClassLoaderException if library not loaded.
   */
  template <typename Base>
  std::vector<std::string> get_available_classes() const;

  /**
   * \brief Create a shared instance of a class.
   * \param[in] class_name Name of the class to instantiate.
   * \return Shared pointer to the instance.
   * \throw ClassLoaderException if creation fails.
   */
  template <typename Base>
  std::shared_ptr<Base> create_shared_instance(const std::string & class_name) const;

  /**
   * \brief Create a unique instance of a class.
   * \param[in] class_name Name of the class to instantiate.
   * \return Unique pointer to the instance with custom deleter.
   * \throw ClassLoaderException if creation fails.
   */
  template <typename Base>
  std::unique_ptr<Base, DeleteClassObjFunc<Base>>
  create_unique_instance(const std::string & class_name) const;

  /**
   * \brief Check if the library is loaded.
   * \return True if loaded.
   */
  bool is_library_loaded() const;

  /**
   * \brief Get the library path.
   * \return Library path.
   */
  const std::string & get_library_path() const;

private:
  void load_library();
  void unload_library();

  std::shared_ptr<rcpputils::SharedLibrary> library_handle_;
  std::string library_path_;
};

// Template implementations

template <typename Base>
std::vector<std::string> ClassLoader::get_available_classes() const
{
  if (!is_library_loaded()) {
    throw ClassLoaderException(
      "Library '" + library_path_ + "' is not loaded");
  }
  return impl::FactoryMapStorage::instance().get_registered_classes<Base>();
}

template <typename Base>
std::shared_ptr<Base> ClassLoader::create_shared_instance(const std::string & class_name) const
{
  if (!is_library_loaded()) {
    throw ClassLoaderException(
      "Library '" + library_path_ + "' is not loaded");
  }

  auto * factory = impl::FactoryMapStorage::instance().find_factory<Base>(class_name);
  if (!factory) {
    throw ClassLoaderException("Class not registered: " + class_name);
  }

  Base * obj = factory->first().release();
  return std::shared_ptr<Base>(obj, factory->second);
}

template <typename Base>
std::unique_ptr<Base, DeleteClassObjFunc<Base>>
ClassLoader::create_unique_instance(const std::string & class_name) const
{
  if (!is_library_loaded()) {
    throw ClassLoaderException(
      "Library '" + library_path_ + "' is not loaded");
  }

  auto * factory = impl::FactoryMapStorage::instance().find_factory<Base>(class_name);
  if (!factory) {
    throw ClassLoaderException("Class not registered: " + class_name);
  }

  return factory->first();
}

}  // namespace class_loader
}  // namespace rcpputils

#endif  // RCPPUTILS__CLASS_LOADER_HPP_
